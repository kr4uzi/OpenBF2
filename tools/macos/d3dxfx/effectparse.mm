// [CHOSEN, not recovered] s0 BYTECODE capture / Metal translation, with a binary input.
#import <Foundation/Foundation.h>
#include <mojoshader.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;
static const char *safe(const char *s) { return s ? s : ""; }
static NSString *ns(const std::string &s)
{
    return [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding]
        ?: @"Non-UTF8 diagnostic";
}
static void writeFile(const fs::path &path, const void *data, size_t size)
{
    std::ofstream out(path, std::ios::binary);
    out.write(static_cast<const char *>(data), size);
    if (!out) throw std::runtime_error("Cannot write " + path.string());
}
struct Shader {
    std::string entry;
    std::vector<unsigned char> bytes;
    std::vector<MOJOSHADER_swizzle> swizzles;
    std::vector<MOJOSHADER_samplerMap> samplers;
    const MOJOSHADER_parseData *parsed = nullptr;
};
static void *captureShader(const void *, const char *entry, const unsigned char *bytes,
    unsigned length, const MOJOSHADER_swizzle *swiz, unsigned swizCount,
    const MOJOSHADER_samplerMap *samplers, unsigned samplerCount)
{
    auto *shader = new Shader;
    shader->entry = safe(entry); shader->bytes.assign(bytes, bytes + length);
    if (swizCount) shader->swizzles.assign(swiz, swiz + swizCount);
    if (samplerCount) shader->samplers.assign(samplers, samplers + samplerCount);
    shader->parsed = MOJOSHADER_parse(MOJOSHADER_PROFILE_BYTECODE, entry, bytes, length,
        swiz, swizCount, samplers, samplerCount, nullptr, nullptr, nullptr);
    return shader;
}
static MOJOSHADER_parseData *parseData(void *s)
{ return const_cast<MOJOSHADER_parseData *>(static_cast<Shader *>(s)->parsed); }
static void deleteShader(const void *, void *ptr)
{
    auto *s = static_cast<Shader *>(ptr);
    MOJOSHADER_freeParseData(s->parsed); delete s;
}
static const char *backendError(const void *) { return "BYTECODE capture failed"; }
static unsigned expressionCount(const MOJOSHADER_effectValue &v)
{
    unsigned count = v.expression_length != 0;
    if (v.type.parameter_type >= MOJOSHADER_SYMTYPE_SAMPLER &&
        v.type.parameter_type <= MOJOSHADER_SYMTYPE_SAMPLERCUBE)
        for (unsigned i = 0; i < v.value_count; ++i) count += expressionCount(v.valuesSS[i].value);
    return count;
}
static void describeValue(std::ostream &out, const MOJOSHADER_effectValue &v)
{
    out << "name=" << safe(v.name) << " semantic=" << safe(v.semantic)
        << " type=" << v.type.parameter_type << " class=" << v.type.parameter_class
        << " rows=" << v.type.rows << " columns=" << v.type.columns
        << " elements=" << v.type.elements << " count=" << v.value_count;
    if (v.expression_length) {
        out << " expression-type=" << v.expression_type << " requires-evaluator raw=" << std::hex;
        auto *bytes = static_cast<const unsigned char *>(v.expression);
        for (unsigned i = 0; i < v.expression_length; ++i) out << unsigned(bytes[i]) << ',';
        out << std::dec;
    }
    if (v.type.parameter_type >= MOJOSHADER_SYMTYPE_SAMPLER &&
        v.type.parameter_type <= MOJOSHADER_SYMTYPE_SAMPLERCUBE) {
        for (unsigned i = 0; i < v.value_count; ++i) {
            out << "\n      sampler-state " << v.valuesSS[i].type << ' ';
            describeValue(out, v.valuesSS[i].value);
        }
    } else if (v.values) {
        out << " raw=";
        for (unsigned i = 0; i < v.value_count; ++i) out << std::hex << v.valuesI[i] << ',';
        out << std::dec;
    }
}
static int run(int argc, char **argv)
{
    bool standalone = argc > 1 && !std::strcmp(argv[1], "--shader");
    const char *entry = "Control";
    if (standalone) {
        if (argc < 5) throw std::runtime_error("usage: effectparse --shader ENTRY BINARY OUTPUT_DIR");
        entry = argv[2]; argc -= 2; argv += 2;
    }
    if (argc != 3) throw std::runtime_error("usage: effectparse EFFECT.fxo OUTPUT_DIR");
    fs::path output = argv[2]; fs::create_directories(output);
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read binary");
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), {}};
    if (bytes.empty() || bytes.size() > UINT32_MAX) throw std::runtime_error("Invalid binary size");
    // MojoShader's bad-magic cleanup can crash before returning its error object.
    // This tool consumes D3DX FX2, not XNA's extra container header.
    const unsigned char magic[] = {0x01, 0x09, 0xff, 0xfe};
    if (!standalone && (bytes.size() < 8 || std::memcmp(bytes.data(), magic, sizeof(magic))))
        throw std::runtime_error("Expected a D3DX FX2 effect header");
    NSMutableDictionary *result = [@{@"mode": standalone ? @"shader-only control" : @"effect",
        @"parse_ok": @NO, @"translate_ok": @NO, @"translate_status": @"not_run",
        @"shader_count": @0, @"msl_count": @0, @"preshader_count": @0} mutableCopy];
    NSMutableArray *errors = [NSMutableArray array], *shaders = [NSMutableArray array];
    auto error = [&](const std::string &s) { [errors addObject:ns(s)]; std::cerr << s << '\n'; };
    auto finish = [&]() {
        result[@"errors"] = errors; result[@"shaders"] = shaders;
        NSData *json = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];
        if (!json) throw std::runtime_error("JSON serialization failed");
        writeFile(output / "result.json", json.bytes, json.length);
        return [result[@"parse_ok"] boolValue] && ([result[@"translate_ok"] boolValue] ||
            [result[@"translate_status"] isEqual:@"no_shaders"]) ? 0 : 1;
    };
    unsigned count = 0, translated = 0;
    std::ofstream inventory(output / "inventory.txt");
    if (!inventory) throw std::runtime_error("Cannot write inventory");
    auto translate = [&](const Shader &s, int index) {
        ++count;
        std::string stem = "object_" + std::to_string(index);
        auto *msl = MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL, s.entry.c_str(), s.bytes.data(),
            static_cast<unsigned>(s.bytes.size()), s.swizzles.data(), static_cast<unsigned>(s.swizzles.size()),
            s.samplers.data(), static_cast<unsigned>(s.samplers.size()), nullptr, nullptr, nullptr);
        inventory << "shader object=" << index << " entry=" << s.entry << " model="
            << s.parsed->major_ver << '.' << s.parsed->minor_ver << " uniforms=" << s.parsed->uniform_count
            << " samplers=" << s.parsed->sampler_count << " preshader=" << (s.parsed->preshader != nullptr) << '\n';
        for (int i = 0; i < s.parsed->symbol_count; ++i) {
            const auto &sym = s.parsed->symbols[i];
            inventory << "  symbol " << safe(sym.name) << " register-set=" << sym.register_set
                << " index=" << sym.register_index << " count=" << sym.register_count << '\n';
        }
        writeFile(output / (stem + ".bin"), s.bytes.data(), s.bytes.size());
        NSMutableDictionary *record = [@{@"object": @(index), @"entry": ns(s.entry),
            @"translate_ok": @NO, @"embedded_preshader": @(s.parsed->preshader != nullptr),
            @"major": @(s.parsed->major_ver), @"minor": @(s.parsed->minor_ver),
            @"shader_type": @(s.parsed->shader_type)} mutableCopy];
        if (msl->error_count) {
            for (int e = 0; e < msl->error_count; ++e)
                error(stem + " " + s.entry + ": " + safe(msl->errors[e].error));
        } else if (!msl->output || msl->output_len <= 0) error(stem + ": empty MSL output");
        else {
            ++translated;
            writeFile(output / (stem + ".metal"), msl->output, msl->output_len);
            record[@"translate_ok"] = @YES; record[@"metal_file"] = ns(stem + ".metal");
        }
        [shaders addObject:record];
        MOJOSHADER_freeParseData(msl);
    };
    auto complete = [&]() {
        result[@"shader_count"] = @(count); result[@"msl_count"] = @(translated);
        result[@"translate_ok"] = @(count > 0 && count == translated);
        result[@"translate_status"] = !count ? @"no_shaders" : count == translated ? @"ok" : @"fail";
    };
    if (standalone) {
        auto *shader = static_cast<Shader *>(captureShader(nullptr, entry, bytes.data(),
            static_cast<unsigned>(bytes.size()), nullptr, 0, nullptr, 0));
        if (shader->parsed->error_count) {
            for (int i = 0; i < shader->parsed->error_count; ++i) error(safe(shader->parsed->errors[i].error));
        } else { result[@"parse_ok"] = @YES; translate(*shader, 0); complete(); }
        deleteShader(nullptr, shader);
        return finish();
    }
    MOJOSHADER_effectShaderContext context{};
    context.compileShader = captureShader; context.getParseData = parseData;
    context.deleteShader = deleteShader; context.getError = backendError;
    auto *effect = MOJOSHADER_compileEffect(bytes.data(), static_cast<unsigned>(bytes.size()),
        nullptr, 0, nullptr, 0, &context);
    if (!effect) { error("MOJOSHADER_compileEffect returned null"); return finish(); }
    if (effect->error_count) {
        for (int i = 0; i < effect->error_count; ++i) error(safe(effect->errors[i].error));
        MOJOSHADER_deleteEffect(effect); return finish();
    }
    result[@"parse_ok"] = @YES;
    inventory << "parameters " << effect->param_count << '\n';
    unsigned stateExpressions = 0;
    for (int i = 0; i < effect->param_count; ++i) {
        stateExpressions += expressionCount(effect->params[i].value);
        inventory << "  "; describeValue(inventory, effect->params[i].value); inventory << '\n';
    }
    inventory << "techniques " << effect->technique_count << '\n';
    unsigned passes = 0, preshaders = 0, shaderReferences = 0;
    for (int i = 0; i < effect->technique_count; ++i) {
        const auto &tech = effect->techniques[i];
        inventory << "  technique " << safe(tech.name) << " passes=" << tech.pass_count << '\n';
        for (unsigned p = 0; p < tech.pass_count; ++p) {
            ++passes;
            const auto &pass = tech.passes[p];
            inventory << "    pass " << safe(pass.name) << " states=" << pass.state_count << '\n';
            for (unsigned s = 0; s < pass.state_count; ++s) {
                stateExpressions += expressionCount(pass.states[s].value);
                inventory << "      state " << pass.states[s].type << ' ';
                describeValue(inventory, pass.states[s].value); inventory << '\n';
            }
        }
    }
    // All objects are visited, including shaders shared by multiple pass state assignments.
    for (int i = 0; i < effect->object_count; ++i) {
        const auto &obj = effect->objects[i];
        if (obj.type != MOJOSHADER_SYMTYPE_VERTEXSHADER && obj.type != MOJOSHADER_SYMTYPE_PIXELSHADER) continue;
        if (obj.shader.is_preshader == 2) {
            ++shaderReferences;
            inventory << "shader reference object=" << i << " parameter=" << obj.shader.params[0] << '\n';
        }
        else if (obj.shader.is_preshader) { ++preshaders; inventory << "selector preshader object=" << i << '\n'; }
        else if (obj.shader.shader) translate(*static_cast<Shader *>(obj.shader.shader), i);
    }
    result[@"technique_count"] = @(effect->technique_count); result[@"pass_count"] = @(passes);
    result[@"preshader_count"] = @(preshaders);
    result[@"shader_reference_count"] = @(shaderReferences); complete();
    result[@"state_expression_count"] = @(stateExpressions);
    MOJOSHADER_deleteEffect(effect);
    inventory.flush();
    if (!inventory) throw std::runtime_error("Cannot write inventory");
    return finish();
}
int main(int argc, char **argv)
{
    @autoreleasepool {
        try { return run(argc, argv); }
        catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
    }
}
