// Records a rendering demo to disk: one DDS file per frame, or one AVI stream through an encoder.
// Placement attested by the __FILE__ string "Code\BF2\RendDX9\MovieRenderer.cpp" (0x101DC2E8).
// Method names are [INFERRED]; the class name is the one its log lines use.

#ifndef RENDDX9_MOVIERENDERER_H
#define RENDDX9_MOVIERENDERER_H

#include <string>

struct IDirect3DSurface9;

namespace dice {
namespace hfe {

class DemoConfig;

namespace dx9rend {

struct RendDX9Base;

// The 0x34-byte record MovieRenderer's constructor hands to the AVI encoder (0x10003390): the
// output name and the frame geometry. A panorama renders `slices` views per frame; with a single
// requested slice the strip is laid out side by side (width multiplied by the slice count).
// [INFERRED] names.
struct MovieEncoderSettings
{
    std::string file;           // +0x00  DemoConfig::renderFile
    int         width;          // +0x1C
    int         height;         // +0x20
    int         fps;            // +0x24
    bool        panorama;       // +0x28
    int         slices;         // +0x2C
    bool        sliceStreams;   // +0x30  true when the demo asked for more than one slice
};

// The AVI encoder MovieRenderer feeds (vtable 0x101DC440, two slots). Windows owns the
// AVIFIL32 implementation in MovieRenderer.cpp; initialization failure destroys it immediately.
// Other platforms refuse compressed recording through the same null factory result.
class MovieEncoder
{
public:
    // +0x00  0x10003CA0, `retn 0Ch`: one frame of 24-bit rows, `pitch` bytes apart.
    virtual void addFrame(const void* bits, int pitch, int rows) = 0;
    // +0x04  0x10003EE0: destroys and frees the encoder (no flag argument).
    virtual void destroy() = 0;

protected:
    ~MovieEncoder() {}
};

// Base +0x3F4. Built by Renderer slot +0x64 from the demo's render settings, destroyed by slot
// +0x68; renderFrame and slot +0x30 capture through it once per presented frame.
class MovieRenderer
{
public:
    // 0x10003390. Keeps `config`, which must outlive the recorder. A failure clears isRecording.
    MovieRenderer(RendDX9Base* renderer, const DemoConfig* config);
    // 0x100031B0.
    ~MovieRenderer();

    // 0x100032F0: DemoConfig::renderPanorama.
    bool isPanorama() const;
    // 0x10003300: the panorama slice count; 10 unless the demo asks for more than one.
    int sliceCount() const;
    // 0x10003510: copies swap chain 0's back buffer and writes it out as the next frame.
    void captureFrame();

private:
    // 0x10003320: the first movie number whose frame 0 file does not exist yet.
    int firstFreeMovieIndex() const;
    // 0x100031E0: the scratch R8G8B8 surface of the back-buffer size; also latches its height.
    IDirect3DSurface9* createCaptureSurface();

public:
    bool                isRecording;   // +0x00  cleared by any failure; nothing is captured then
    RendDX9Base*        renderer;      // +0x04
    std::string         filePrefix;    // +0x08  DemoConfig::renderFile
    int                 movieIndex;    // +0x24
    int                 frameIndex;    // +0x28
    int                 surfaceRows;   // +0x2C  back-buffer height
    IDirect3DSurface9*  surface;       // +0x30
    MovieEncoder*       encoder;       // +0x34  null for DDS frames
    int                 height;        // +0x38  DemoConfig::renderHeight
    int                 width;         // +0x3C  DemoConfig::renderWidth
    const DemoConfig*   config;        // +0x40
};

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // RENDDX9_MOVIERENDERER_H
