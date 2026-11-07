// [CHOSEN, not recovered] Canonical text form of an effect Description (see Dump.h).
#include "Dump.h"
#include <cstdio>
#include <sstream>

namespace nativefx {
using namespace bf2fx;
namespace {
std::string hex(const Words& w) {
    std::ostringstream s;
    for (size_t i = 0; i < w.size(); ++i) { char b[16]; snprintf(b, sizeof b, "%s%x", i ? "," : "", w[i]); s << b; }
    return s.str();
}
std::string type(const Type& t) {
    std::ostringstream s;
    s << "t" << t.type << "/c" << t.cls << " " << t.rows << "x" << t.columns << " e" << t.elements;
    if (!t.members.empty()) {
        s << " {";
        for (const auto& m : t.members) s << m.name << ":" << type(m) << ";";
        s << "}";
    }
    return s.str();
}
struct Ctx {
    const Description& d;
    bool registers;
    const Shader* shader(uint32_t object) const {
        for (const auto& s : d.shaders) if (s.object == object) return &s;
        return nullptr;
    }
    std::string object(uint32_t id) const { return registers ? "#" + std::to_string(id) : ""; }
    std::string value(const Value& v) const {
        std::ostringstream s;
        if (v.expressionType == 1 && !v.expression.empty()) { s << "ref " << v.reference << " " << type(v.type); return s.str(); }
        if (v.expressionType == 0 && !v.expression.empty()) {
            s << "expr " << type(v.type) << " in=" << v.program.inputs << " temps=" << v.program.temps << " [";
            for (const auto& b : v.program.bindings)
                s << b.type.name << ":" << type(b.type) << " set" << b.set << " r" << b.index << "x" << b.count << ";";
            s << "] [";
            for (const auto& i : v.program.instructions) {
                s << "op" << i.opcode << "." << i.elements;
                for (const auto& o : i.operands) s << " k" << o.kind << ":" << o.index << (o.arrays.empty() ? "" : "[]");
                s << ";";
            }
            s << "] lit=" << v.program.literals.size();
            return s.str();
        }
        if (v.type.cls == 4 && (v.type.type == VertexShader || v.type.type == PixelShader)) {
            s << (v.type.type == VertexShader ? "vs" : "ps") << " e" << v.type.elements;
            for (auto w : v.words) {
                const Shader* sh = shader(w);
                if (!w) { s << " null"; continue; }
                if (!sh) { s << " missing" << object(w); continue; }
                if (sh->selectorKind == 2)
                    s << " select(" << (sh->selectorParameter < d.parameters.size() ? d.parameters[sh->selectorParameter].value.type.name : "?")
                      << "[" << sh->selectorElement << "])" << object(w);
                else if (sh->selectorKind == 1) s << " select(preshader)" << object(w);
                else s << " shader" << object(w);
            }
            return s.str();
        }
        if (v.type.cls == 4 && v.type.type >= Texture && v.type.type <= 9) {
            s << "texture " << type(v.type);
            for (auto w : v.words) s << " '" << (w < d.objects.size() ? d.objects[w] : "?") << "'" << object(w);
            return s.str();
        }
        if (v.type.type == String) { s << "string \"" << v.text << "\"" << (v.words.empty() ? "" : object(v.words[0])); return s.str(); }
        s << type(v.type) << " [" << hex(v.words) << "]";
        return s.str();
    }
    void annotations(std::ostringstream& s, const std::vector<Value>& a, const char* indent) const {
        for (const auto& v : a) s << indent << "@" << v.type.name << " " << value(v) << "\n";
    }
};
} // namespace

std::string dump(const Description& d, bool registers) {
    Ctx c{d, registers};
    std::ostringstream s;
    for (const auto& p : d.parameters) {
        const Value& v = p.value;
        s << "param " << v.type.name << " " << type(v.type) << " sem=" << v.semantic << " flags=" << p.flags << "\n";
        if (v.type.type >= Sampler && v.type.type <= 14 && v.type.cls == 4) {
            for (size_t k = 0; k < v.states.size(); ++k) s << "  sampler " << v.stateIds[k] << " = " << c.value(v.states[k]) << "\n";
        } else s << "  = " << c.value(v) << "\n";
        c.annotations(s, p.annotations, "  ");
    }
    for (const auto& t : d.techniques) {
        s << "technique " << t.name << "\n";
        c.annotations(s, t.annotations, "  ");
        for (const auto& p : t.passes) {
            s << "  pass " << p.name << "\n";
            c.annotations(s, p.annotations, "    ");
            for (const auto& st : p.states) s << "    state " << st.id << "[" << st.index << "] = " << c.value(st.value) << "\n";
        }
    }
    if (registers) {
        s << "objects " << d.objects.size() << "\n";
        for (size_t i = 0; i < d.objects.size(); ++i) if (!d.objects[i].empty()) s << "  #" << i << " " << d.objects[i] << "\n";
    }
    return s.str();
}
} // namespace nativefx
