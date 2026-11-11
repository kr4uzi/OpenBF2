// [TEMP] [CHOSEN, not recovered] Synthetic registry-lifetime observer. No game
// content, Metal device, engine startup or sockets. Unused interface calls abort.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "gameswf/SwiffPlayer/SwiffBitmapCache.h"
#include "gameswf/gameswf_render.h"
#include "gameswf/gameswf_styles.h"

using namespace dice::hfe::ui;
static int checks;
static void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}

struct renderer : ISwiffRender
{
    int refs = 1, uploads = 0, releases = 0, next = 1;
    bool refuse = false;
    std::map<int, std::vector<unsigned char> > textures;
    void addRef() override { ++refs; }
    int getRef() override { return refs; }
    int release() override { return --refs; }
    int releaseTexture(int id) override
    {
        ++releases;
        if (id == -1) return 0;
        check(textures.erase(id) == 1, "release addresses the owning registry exactly once");
        return 1;
    }
    int uploadAlpha(unsigned w, unsigned h, const void* data) override
    {
        ++uploads;
        if (refuse) return -1;
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        textures[next] = std::vector<unsigned char>(bytes, bytes + w * h);
        return next++;
    }
    int uploadRgba(unsigned int* descriptor) override
    {
        ++uploads;
        if (refuse) return -1;
        image::image_base* im = reinterpret_cast<image::image_base*>(descriptor);
        // Model the shipped destructive R/B swap, including rows with padding.
        for (int y = 0; y < im->m_height; ++y)
            for (int x = 0; x < im->m_width; ++x)
                std::swap(im->m_data[y * im->m_pitch + x * 4], im->m_data[y * im->m_pitch + x * 4 + 2]);
        textures[next] = std::vector<unsigned char>(im->m_data, im->m_data + im->m_pitch * im->m_height);
        return next++;
    }

#define UNUSED_RENDER_CALL(result, name, args) result name args override { std::abort(); }
    UNUSED_RENDER_CALL(void*, queryInterface, (unsigned) const)
    UNUSED_RENDER_CALL(int, init, ())
    UNUSED_RENDER_CALL(int, shutdown, ())
    UNUSED_RENDER_CALL(int, slot6, ())
#if !defined(_MSC_VER)
    UNUSED_RENDER_CALL(int, slot8, (void*))
    UNUSED_RENDER_CALL(int, fontForward16, (void*, const wchar_t*))
    UNUSED_RENDER_CALL(char, fontForward32, (void*, int*, float, float))
    UNUSED_RENDER_CALL(char, fontForward40, (void*, int*, int*))
#else
    UNUSED_RENDER_CALL(int, slot8, (int))
    UNUSED_RENDER_CALL(int, fontForward16, (void*, int))
    UNUSED_RENDER_CALL(char, fontForward32, (void*, int, float, float))
    UNUSED_RENDER_CALL(char, fontForward40, (void*, int, int))
#endif
    UNUSED_RENDER_CALL(int, loadTexture, (const char*, unsigned*, unsigned*))
    UNUSED_RENDER_CALL(void, beginDisplay, (int, int, int, int, int, float, float, float, float))
    UNUSED_RENDER_CALL(void, endDisplay, ())
    UNUSED_RENDER_CALL(void, setTransform, (const float*))
    UNUSED_RENDER_CALL(void, setCxform, (const float*))
    UNUSED_RENDER_CALL(int, recordStat, (int))
    UNUSED_RENDER_CALL(void, drawVertices, (const short*, unsigned))
    UNUSED_RENDER_CALL(void, drawLineStrip, (const short*, unsigned))
    UNUSED_RENDER_CALL(int, fillStyleDisable, (int))
    UNUSED_RENDER_CALL(void, fillStyleColor, (int, unsigned))
    UNUSED_RENDER_CALL(void, setTexture, (int, int, int, int, const float*, int))
    UNUSED_RENDER_CALL(void, lineStyleDisable, ())
    UNUSED_RENDER_CALL(int, lineStyleColor, (unsigned))
    UNUSED_RENDER_CALL(void, lineStyleWidth, (unsigned))
    UNUSED_RENDER_CALL(void, nullsub25, (const void*, int, const void*, const void*, unsigned))
    UNUSED_RENDER_CALL(void*, createFont, (const char*, int))
    UNUSED_RENDER_CALL(void, releaseFontHandle, (void*))
    UNUSED_RENDER_CALL(int, destroyFont, (void*))
    UNUSED_RENDER_CALL(void, drawGlyphBitmap, (void*, const float*, unsigned, int))
    UNUSED_RENDER_CALL(char, fontGetExtent, (void*, int, float*, float*))
    UNUSED_RENDER_CALL(char, fontForward44, (void*, float, float, float, float))
    UNUSED_RENDER_CALL(int, fontForward48, (void*))
    UNUSED_RENDER_CALL(int, fontSetSize, (void*, float, int))
    UNUSED_RENDER_CALL(int, fontSetField104, (void*, int))
    UNUSED_RENDER_CALL(void, nullsub38, (int))
    UNUSED_RENDER_CALL(void, beginMask, ())
    UNUSED_RENDER_CALL(void, endMask, ())
    UNUSED_RENDER_CALL(void, disableMask, ())
    UNUSED_RENDER_CALL(void, setMovieWidth, (unsigned))
    UNUSED_RENDER_CALL(void, setMovieHeight, (int))
    UNUSED_RENDER_CALL(void, setScale, (float, float))
    UNUSED_RENDER_CALL(void, setBackgroundColor, (unsigned))
    UNUSED_RENDER_CALL(void, setMouseState, (int, int, int))
    UNUSED_RENDER_CALL(int, slot47, (int))
    UNUSED_RENDER_CALL(void, nullsub48, (int))
    UNUSED_RENDER_CALL(char, slot49, (intptr_t))
    UNUSED_RENDER_CALL(char, slot50, (intptr_t))
    UNUSED_RENDER_CALL(int, beginFrame, ())
    UNUSED_RENDER_CALL(int, endFrame, ())
    UNUSED_RENDER_CALL(void*, setTextureTechnique, (const std::string&, int))
    UNUSED_RENDER_CALL(int, updateTime, (unsigned, float))
    UNUSED_RENDER_CALL(void*, getTextureRegistry, ())
    UNUSED_RENDER_CALL(int, uploadTexture, (const std::string&, unsigned*, unsigned*))
    UNUSED_RENDER_CALL(unsigned*, getDriverVersion, (unsigned*))
    UNUSED_RENDER_CALL(char, isNvidia, ())
    UNUSED_RENDER_CALL(char, getFlag, ())
    UNUSED_RENDER_CALL(char, setFlag, (char))
    UNUSED_RENDER_CALL(char, preloadCredits, ())
    UNUSED_RENDER_CALL(int, creditsCount, ())
    UNUSED_RENDER_CALL(std::string, getCreditsName, (unsigned))
    UNUSED_RENDER_CALL(std::string, getRandomCreditsName, ())
    UNUSED_RENDER_CALL(char, getWasDeviceLost, ())
    UNUSED_RENDER_CALL(void, clearWasDeviceLost, ())
#undef UNUSED_RENDER_CALL
};

int main()
{
    gameswf::set_render_handler(NULL);
    renderer first, second;
    std::shared_ptr<SwiffBitmapRegistry> a = std::make_shared<SwiffBitmapRegistry>(&first);
    std::shared_ptr<SwiffBitmapRegistry> b = std::make_shared<SwiffBitmapRegistry>(&second);
    check(first.refs == 2 && second.refs == 2, "leases retain each renderer");

    // Padded two-row RGBA image with nonopaque, distinct endpoint channels.
    const unsigned char rgba[] = { 250, 3, 17, 0, 90, 60, 30, 255, 7, 8, 9, 10,
                                  11, 22, 33, 127, 44, 55, 66, 200, 1, 2, 3, 4 };
    std::unique_ptr<gameswf::bitmap_info> bitmap(new gameswf::bitmap_info);
    bitmap->m_render_resource.reset(new SwiffBitmapPixels(SwiffBitmapPixels::Rgba, 2, 2, 12, rgba));
    SwiffBitmapPixels& pixels = static_cast<SwiffBitmapPixels&>(*bitmap->m_render_resource);
    SwiffBitmapRegistry::bind(bitmap.get(), a);
    const int firstId = static_cast<int>(bitmap->m_texture_id);
    check(first.uploads == 1 && first.textures[firstId][0] == 17
        && first.textures[firstId][2] == 250, "RGBA upload takes the channel swap once");
    check(first.textures[firstId][3] == 0 && first.textures[firstId][7] == 255
        && first.textures[firstId][15] == 127, "straight alpha survives upload");
    check(std::memcmp(pixels.bytes.data(), rgba, sizeof(rgba)) == 0, "canonical CPU bytes stay RGBA");
    for (int i = 0; i < 100; ++i) SwiffBitmapRegistry::bind(bitmap.get(), a);
    check(first.uploads == 1, "repeated draws reuse the current registry upload");
    SwiffBitmapRegistry::bind(bitmap.get(), b);
    check(second.uploads == 1 && second.textures[bitmap->m_texture_id] == first.textures[firstId],
          "another renderer uploads identical pixels despite overlapping handle values");
    SwiffBitmapRegistry::bind(bitmap.get(), a);
    check(first.uploads == 1 && second.uploads == 1, "two live players reuse independent bindings");
    a.reset();
    check(first.refs == 1 && first.textures.empty(), "player teardown releases GPU resources while CPU cache lives");
    for (int i = 0; i < 100; ++i)
    {
        a = std::make_shared<SwiffBitmapRegistry>(&first);
        SwiffBitmapRegistry::bind(bitmap.get(), a);
        check(first.textures[bitmap->m_texture_id] == second.textures.begin()->second, "reload uses canonical pixels");
        a.reset();
        check(first.refs == 1 && first.textures.empty(), "reload leaves no GPU resources or retained renderer");
        check(pixels.registries.size() <= 2, "expired registry records do not grow across reloads");
    }
    bitmap.reset();
    check(second.textures.empty() && second.refs == 2, "descriptor destruction releases its other live binding");
    b.reset();
    check(second.refs == 1 && second.releases == 1, "lease destruction does not double release deleted bitmap");

    const unsigned char alpha[] = { 0, 127, 255, 32 };
    bitmap.reset(new gameswf::bitmap_info);
    bitmap->m_render_resource.reset(new SwiffBitmapPixels(SwiffBitmapPixels::Alpha, 2, 2, 2, alpha));
    a = std::make_shared<SwiffBitmapRegistry>(&first);
    SwiffBitmapRegistry::bind(bitmap.get(), a);
    check(first.textures[bitmap->m_texture_id] == std::vector<unsigned char>(alpha, alpha + 4), "A8 glyph pixels survive upload");
    bitmap.reset(); a.reset();

    first.refuse = true;
    a = std::make_shared<SwiffBitmapRegistry>(&first);
    bitmap.reset(new gameswf::bitmap_info);
    bitmap->m_render_resource.reset(new SwiffBitmapPixels(SwiffBitmapPixels::Alpha, 2, 2, 2, alpha));
    SwiffBitmapRegistry::bind(bitmap.get(), a);
    const int uploads = first.uploads;
    SwiffBitmapRegistry::bind(bitmap.get(), a);
    check(static_cast<int>(bitmap->m_texture_id) == -1 && first.uploads == uploads,
          "failed upload keeps -1 without fabricated success or draw-time retry");
    a.reset(); bitmap.reset();
    check(first.refs == 1 && first.textures.empty(), "failed upload teardown keeps ownership balanced");
    std::printf("swf_bitmap_cache_runtime: %d checks passed\n", checks);
}
