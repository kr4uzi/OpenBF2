// [TEMP] [CHOSEN, not recovered] Content-free checks of terrain height source preparation.
#include "../BF2/RendDX9/ShaderSource.h"

#include <cstdio>
#include <string>

#if defined(BF2_SHADER_SOURCE_CACHE_CHECK)
#include "../BF2/Platform/Gfx/Effect/Cache.h"
#endif

using dice::hfe::dx9rend::correctTerrainShaderHeight;

namespace {

const char stock[] = "wPos.yw = (indata.Pos1.xw * vScaleTransY.xy) + vScaleTransY.zw;";
const char corrected[] = "wPos.yw = indata.Pos1.xw * vScaleTransY.xy;";
int failures = 0;
unsigned int checks = 0;

void check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
}

void checkText(const char* name, const char* path, std::string text,
               const std::string& expected, unsigned int expectedCount)
{
    unsigned int size = static_cast<unsigned int>(text.size());
    const unsigned int count = correctTerrainShaderHeight(path, text.empty() ? nullptr : &text[0], size);
    text.resize(size);
    check(text == expected && count == expectedCount, name);
    check(correctTerrainShaderHeight(path, text.empty() ? nullptr : &text[0], size) == 0,
          "correction is idempotent");
}

#if defined(BF2_SHADER_SOURCE_CACHE_CHECK)
struct Include final : bf2fx::IncludeResolver
{
    std::string text = stock;
    bool open(std::uint32_t, const std::string&, const void*, const void*& data, std::size_t& size) override
    {
        data = text.data(); size = text.size(); return true;
    }
    void close(const void*) override {}
};

void checkCache()
{
    // Synthetic include directive; no game-derived shader content or compiler/GPU is needed.
    const std::string root = "#include \"TerrainShader_Hi.fx\"\n";
    Include include;
    bf2fx::Request before, after, unchanged, rootBefore, rootAfter;
    auto capture = [&](const std::string& source, bf2fx::Request& request) {
        return bool(bf2fx::capture(source.data(), source.size(), {}, 0, std::string(64, 'a'),
                                  "source-fix-check", &include, request));
    };
    check(capture(root, before), "capture original include");
    unsigned int size = static_cast<unsigned int>(include.text.size());
    check(correctTerrainShaderHeight("mods/any/Shaders/TerrainShader_Hi.fx", &include.text[0], size) == 1,
          "correct included source before capture");
    include.text.resize(size);
    check(capture(root, after), "capture corrected include");
    check(after.includes.size() == 1 && after.includes[0].bytes == bf2fx::Bytes(corrected, corrected + sizeof(corrected) - 1),
          "request holds corrected include bytes");
    check(bf2fx::cacheKey(before) != bf2fx::cacheKey(after), "old include cache key cannot match");
    check(capture(root, unchanged) && bf2fx::cacheKey(unchanged) == bf2fx::cacheKey(after),
          "corrected include cache key is stable");
    check(capture(stock, rootBefore) && capture(corrected, rootAfter)
          && bf2fx::cacheKey(rootBefore) != bf2fx::cacheKey(rootAfter), "root source changes cache key");
}
#endif

} // namespace

int main()
{
    checkText("two stock lines", "TerrainShader_Hi.fx", std::string(stock) + "\r\n\t" + stock + "\n",
              std::string(corrected) + "\r\n\t" + corrected + "\n", 2);
    checkText("compact tokens", "mods/custom/shaders/TERRAINSHADER_HI.FX",
              "wPos.yw=(indata.Pos1.xw*vScaleTransY.xy)+vScaleTransY.zw;", corrected, 1);
    checkText("spaces and tabs between tokens", "mods\\custom\\Shaders\\TerrainShader_Hi.fx",
              "wPos \t. yw\t =\t( indata . Pos1\t. xw\t * vScaleTransY . xy )\t+ vScaleTransY . zw ;",
              corrected, 1);
    checkText("another file", "TerrainShader_Shared.fx", stock, stock, 0);
    checkText("filename prefix", "MyTerrainShader_Hi.fx", stock, stock, 0);
    checkText("filename suffix", "TerrainShader_Hi.fx.backup", stock, stock, 0);
    checkText("rewritten source", "TerrainShader_Hi.fx", corrected, corrected, 0);
    checkText("unrelated text", "TerrainShader_Hi.fx", "unchanged\tbytes\r\n", "unchanged\tbytes\r\n", 0);
    checkText("empty source", "TerrainShader_Hi.fx", "", "", 0);
    checkText("identifier boundary", "TerrainShader_Hi.fx", std::string("other") + stock,
              std::string("other") + stock, 0);
    const std::string comments = std::string("// ") + stock + "\n/* " + stock + " */\n\"" + stock + "\"\n";
    checkText("comments and strings", "TerrainShader_Hi.fx", comments + stock, comments + corrected, 1);
    for (std::size_t n = 0; n < sizeof(stock) - 1; ++n)
    {
        const std::string truncated(stock, n);
        checkText("truncated assignment", "TerrainShader_Hi.fx", truncated, truncated, 0);
    }
    const std::string binary = std::string("before\0", 7) + stock + std::string("\0after", 6);
    checkText("bounded bytes and surrounding text", "TerrainShader_Hi.fx", binary,
              std::string("before\0", 7) + corrected + std::string("\0after", 6), 1);
#if defined(BF2_SHADER_SOURCE_CACHE_CHECK)
    checkCache();
#endif
    if (failures) return 1;
    std::printf("terrain shader source: %u checks passed\n", checks);
    return 0;
}
