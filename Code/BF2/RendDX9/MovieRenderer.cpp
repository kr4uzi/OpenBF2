// Demo movie recording: per-frame back-buffer copies written as DDS files, or handed to an AVI
// encoder. Recovered from RendDX9.dll 1.50 (0x100031B0..0x10003F90); the __FILE__ string
// "Code\BF2\RendDX9\MovieRenderer.cpp" (0x101DC2E8) attests the placement.

#ifdef BF2_IMAGE_RENDDX9

#include "MovieRenderer.h"

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"    // dice::hfe::fileManager (dword_10244AC4)
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../Core/StringUtil.h"              // formatString (sub_10074AB0)
#include "../Game/DemoRecorder/DemoConfig.h"
#include "RendDX9Base.h"

#include <string>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#if defined(_WIN32)
#include <vfw.h>
#pragma comment(lib, "vfw32.lib")
#endif

namespace dice {
namespace hfe {
namespace dx9rend {

#if defined(_M_IX86) && defined(_MSC_VER)
// The retail x86 layout: slot +0x64 allocates 0x44 bytes; the encoder record is 0x34. Both sizes
// count a 0x1C-byte VC7.1 std::string; a current MSVC x86 build checks the members around it.
#if _MSC_VER == 1310
static_assert(sizeof(MovieRenderer) == 0x44, "MovieRenderer is 0x44 bytes");
static_assert(sizeof(MovieEncoderSettings) == 0x34, "MovieEncoderSettings is 0x34 bytes");
#endif
static_assert(offsetof(MovieRenderer, filePrefix) == 0x08
                  && sizeof(MovieRenderer) - offsetof(MovieRenderer, movieIndex) == 0x44 - 0x24,
              "MovieRenderer: the prefix at +0x08, then +0x24..+0x43 - see 0x10004FCB");
static_assert(offsetof(MovieEncoderSettings, file) == 0x00
                  && sizeof(MovieEncoderSettings) - offsetof(MovieEncoderSettings, width) == 0x34 - 0x1C,
              "MovieEncoderSettings: the name at +0x00, then +0x1C..+0x33 - see 0x10003390");
#endif

namespace {

// The frame file name: "<prefix>M<movie:04>_F<frame:06>.dds" (0x100032B0, format 0x101DC334).
std::string movieFrameFileName(const std::string& prefix, int movie, int frame)
{
    return formatString("%sM%04d_F%06d.dds", prefix.c_str(), movie, frame);
}

#if defined(_WIN32)
// [INFERRED] Class name and placement beside its MovieRenderer factory. The two-slot table
// at 0x101DC440 contains addFrame and destroy; there is no virtual destructor slot.
class AviMovieEncoder : public MovieEncoder
{
public:
    // PE 0x10003C20.
    explicit AviMovieEncoder(const MovieEncoderSettings& source)
        : settings(source), initialized(true), files(0), compressedStreams(0), streams(0),
          frameIndex(0), sliceIndex(0), buffer(0), stride(0), paddedRows(0), frameBytes(0)
    {
        initialize();
    }

    // PE 0x10003F00.
    ~AviMovieEncoder()
    {
        ::renddx9abi::engineFree(buffer);
        for (int i = 0; i < streamCount; ++i)
        {
            if (compressedStreams[i]) compressedStreams[i]->Release();
            if (streams[i]) streams[i]->Release();
            if (files[i]) files[i]->Release();
        }
        ::AVIFileExit();
        ::renddx9abi::engineFree(compressedStreams);
        ::renddx9abi::engineFree(streams);
        ::renddx9abi::engineFree(files);
    }

    // PE 0x10003EE0.
    void destroy() override { delete this; }

    // PE 0x10003CA0.
    void addFrame(const void* bits, int pitch, int rows) override
    {
        if (!initialized)
            return;
        if (!settings.panorama)
        {
            copyFrame(bits, pitch, rows, 0);
            ++frameIndex;
            return;
        }
        if (settings.sliceStreams)
        {
            const int stream = sliceIndex++;
            copyFrame(bits, pitch, rows, stream);
            if (sliceIndex == settings.slices)
            {
                ++frameIndex;
                sliceIndex = 0;
            }
        }
        else
            copyStrip(bits, pitch, rows);
    }

private:
    // PE 0x10003810.
    void initialize()
    {
        AVICOMPRESSOPTIONS options = {};
        LPAVICOMPRESSOPTIONS selectedOptions = &options;
        streamCount = settings.panorama && settings.sliceStreams ? settings.slices : 1;
        files = static_cast<PAVIFILE*>(::renddx9abi::engineAlloc(
            static_cast<unsigned int>(sizeof(PAVIFILE) * streamCount)));
        compressedStreams = static_cast<PAVISTREAM*>(::renddx9abi::engineAlloc(
            static_cast<unsigned int>(sizeof(PAVISTREAM) * streamCount)));
        streams = static_cast<PAVISTREAM*>(::renddx9abi::engineAlloc(
            static_cast<unsigned int>(sizeof(PAVISTREAM) * streamCount)));
        for (int i = 0; i < streamCount; ++i)
        {
            files[i] = 0;
            compressedStreams[i] = 0;
            streams[i] = 0;
        }

        const int paddedWidth = ((settings.width - 1) / 16 + 1) * 16;
        const int paddedHeight = ((settings.height - 1) / 16 + 1) * 16;
        paddedRows = paddedHeight;
        stride = paddedWidth * 3;
        frameBytes = paddedHeight * paddedWidth * 3;
        buffer = static_cast<unsigned char*>(::renddx9abi::engineAlloc(
            static_cast<unsigned int>(frameBytes)));
        ::memset(buffer, 0, static_cast<size_t>(frameBytes));

        BITMAPINFOHEADER bitmap = {};
        bitmap.biSize = static_cast<DWORD>(sizeof(bitmap));
        bitmap.biWidth = paddedWidth;
        bitmap.biHeight = paddedHeight;
        bitmap.biPlanes = 1;
        bitmap.biBitCount = 24;
        bitmap.biSizeImage = static_cast<DWORD>(frameBytes);
        ::AVIFileInit();
        for (int i = 0; i < streamCount; ++i)
        {
            std::string name;
            if (streamCount > 1)
            {
                // shipped defect: the four-byte decimal buffer overflows at stream 1000.
                char number[4];
                ::_itoa(i, number, 10);
                name = extractEverythingExceptExt(settings.file) + '_' + number + '.'
                     + extractFileExt(settings.file);
            }
            else
                name = settings.file;
            if (FAILED(::AVIFileOpenA(&files[i], name.c_str(), 0x1001, 0)))
            {
                initialized = false;
                return;
            }

            AVISTREAMINFOA info = {};
            info.fccType = 0x73646976; // 'vids'
            info.dwScale = 1;
            info.dwRate = static_cast<DWORD>(settings.fps);
            info.dwSuggestedBufferSize = static_cast<DWORD>(frameBytes);
            info.rcFrame.right = paddedWidth;
            info.rcFrame.bottom = paddedHeight;
            if (FAILED(::AVIFileCreateStreamA(files[i], &streams[i], &info))
                || ::AVISaveOptions(0, 0, 1, &streams[i], &selectedOptions) == 0
                || FAILED(::AVIMakeCompressedStream(&compressedStreams[i], streams[i], &options, 0))
                || FAILED(::AVIStreamSetFormat(compressedStreams[i], 0, &bitmap,
                                               static_cast<LONG>(bitmap.biSize))))
            {
                initialized = false;
                return;
            }
            // shipped defect: AVISaveOptionsFree is never called for options' allocations.
        }
    }

    // PE 0x10003D30. Copies only the input rows/bytes that fit, keeping the padded area intact.
    void copyFrame(const void* bits, int pitch, int rows, int stream)
    {
        int copyRows = settings.height;
        if (copyRows > rows) copyRows = rows;
        int copyBytes = settings.width * 3;
        if (copyBytes > pitch) copyBytes = pitch;
        const unsigned char* input = static_cast<const unsigned char*>(bits);
        for (int row = 0; row < copyRows; ++row)
        {
            ::memcpy(buffer + (paddedRows - row - 1) * stride, input,
                     static_cast<size_t>(copyBytes));
            input += pitch;
        }
        writeFrame(stream);
    }

    // PE 0x10003DC0. A failed write permanently disables the encoder.
    void writeFrame(int stream)
    {
        if (FAILED(::AVIStreamWrite(compressedStreams[stream], frameIndex, 1, buffer,
                                    frameBytes, 0, 0, 0)))
            initialized = false;
    }

    // PE 0x10003E00. The strip is emitted only after its last slice, even if that write fails.
    void copyStrip(const void* bits, int pitch, int rows)
    {
        const int slice = sliceIndex++;
        unsigned char* output = buffer + (paddedRows - 1) * stride
                              + (settings.width * slice * 3) / settings.slices;
        int copyRows = settings.height;
        if (copyRows > rows) copyRows = rows;
        int copyBytes = (settings.width * 3) / settings.slices;
        if (copyBytes > pitch) copyBytes = pitch;
        const unsigned char* input = static_cast<const unsigned char*>(bits);
        for (int row = 0; row < copyRows; ++row)
        {
            ::memcpy(output, input, static_cast<size_t>(copyBytes));
            input += pitch;
            output -= stride;
        }
        if (sliceIndex == settings.slices)
        {
            sliceIndex = 0;
            writeFrame(0);
            ++frameIndex;
        }
    }

    MovieEncoderSettings settings; // +0x04
public:
    bool initialized;              // +0x38
private:
    int streamCount;               // +0x3C
    PAVIFILE* files;               // +0x40
    PAVISTREAM* compressedStreams; // +0x44
    PAVISTREAM* streams;           // +0x48
    int frameIndex;                // +0x4C
    int sliceIndex;                // +0x50
    unsigned char* buffer;        // +0x54
    int stride;                   // +0x58
    int paddedRows;               // +0x5C
    int frameBytes;               // +0x60
};

#if defined(_M_IX86) && defined(_MSC_VER)
// The retail x86 encoder is 0x64 bytes, counting the settings record's 0x1C-byte VC7.1 string; a
// current MSVC x86 build checks the run after the settings record.
#if _MSC_VER == 1310
static_assert(sizeof(AviMovieEncoder) == 0x64, "AviMovieEncoder is 0x64 bytes");
#endif
static_assert(sizeof(AviMovieEncoder) - offsetof(AviMovieEncoder, initialized) == 0x64 - 0x38,
              "AviMovieEncoder: +0x38..+0x63 after the settings record");
#endif
#endif // _WIN32

// PE 0x100037E0. Failed initialization destroys the encoder and stops the recording.
MovieEncoder* __fastcall createMovieEncoder(const MovieEncoderSettings& settings)
{
#if defined(_WIN32)
    void* storage = ::renddx9abi::engineAlloc(static_cast<unsigned int>(sizeof(AviMovieEncoder)));
    AviMovieEncoder* encoder = storage ? new (storage) AviMovieEncoder(settings) : 0;
    // shipped defect: allocation failure still dereferences the initialization flag.
    if (!encoder->initialized)
    {
        encoder->destroy();
        return 0;
    }
    return encoder;
#else
    // [CHOSEN, not recovered] This platform has no Windows AVI encoder. Refuse recording
    // through the null result the retail factory produces for failed initialization.
    (void)settings;
    std::fprintf(stderr, "[CHOSEN, not recovered] dice::hfe::dx9rend::createMovieEncoder: "
                         "Windows AVIFIL32 encoding unavailable; compressed recording refused\n");
    std::fflush(stderr);
    return 0;
#endif
}

} // namespace

// PE 0x10003390. The DDS numbering starts at the first free movie number; the scratch surface is
// made at once. A compressed recording also builds the encoder from a settings record: the
// demo's size and rate, and for a panorama the slice layout (several slices: one view each;
// otherwise a strip ten frames wide).
MovieRenderer::MovieRenderer(RendDX9Base* owner, const DemoConfig* demo)
    : isRecording(true),
      renderer(owner),
      filePrefix(demo->renderFile)
{
    movieIndex = firstFreeMovieIndex();                                       // sub_10003320
    frameIndex = 0;
    surfaceRows = 0;
    surface = createCaptureSurface();                                         // sub_100031E0
    encoder = 0;
    height = demo->renderHeight;                                              // +0x60
    width = demo->renderWidth;                                                // +0x5C
    config = demo;

    MovieEncoderSettings settings;
    settings.file = filePrefix;
    settings.width = demo->renderWidth;
    settings.height = demo->renderHeight;
    settings.fps = demo->renderFps;                                           // +0x64
    if (demo->renderPanorama)                                                 // +0x6D
    {
        settings.panorama = true;
        if (demo->numPanoramaSlices > 1)                                      // +0x70, signed
        {
            settings.sliceStreams = true;
            settings.slices = sliceCount();
        }
        else
        {
            settings.sliceStreams = false;
            settings.slices = sliceCount();
            settings.width = settings.slices * settings.width;
        }
    }
    else
    {
        settings.panorama = false;
        settings.sliceStreams = false;
        settings.slices = 1;
    }

    if (demo->renderCompressed)                                               // +0x6C
    {
        encoder = createMovieEncoder(settings);                               // sub_100037E0
        if (!encoder)
        {
            ::renddx9abi::logf("MovieRenderer.cpp:72 Failed to initialize AVI encoder.");
            isRecording = false;
        }
    }
}

// PE 0x100031B0.
MovieRenderer::~MovieRenderer()
{
    if (surface)
        surface->Release();                                                   // +0x08
    if (encoder)
        encoder->destroy();                                                   // +0x04
}

// PE 0x100032F0.
bool MovieRenderer::isPanorama() const
{
    return config->renderPanorama;
}

// PE 0x10003300.
int MovieRenderer::sliceCount() const
{
    if (config->renderPanorama && config->numPanoramaSlices > 1)
        return config->numPanoramaSlices;
    return 10;
}

// PE 0x10003320. Probes frame 0 of each movie number through the file manager (+0x1C).
int MovieRenderer::firstFreeMovieIndex() const
{
    int index = 0;
    while (::dice::hfe::fileManager->fileExists(movieFrameFileName(filePrefix, index, 0)))
        ++index;
    return index;
}

// PE 0x100031E0. D3DFMT_R8G8B8 (20) in D3DPOOL_SCRATCH, sized like the back buffer.
// [CHOSEN, not recovered] The shipped log line appends DXGetErrorDescription9A(hr), DirectX SDK
// library code that is not in this tree; the result code is logged instead.
IDirect3DSurface9* MovieRenderer::createCaptureSurface()
{
    IDirect3DSurface9* created = 0;
    const HRESULT hr = renderer->device->CreateOffscreenPlainSurface(
        renderer->pp.BackBufferWidth, renderer->pp.BackBufferHeight, D3DFMT_R8G8B8,
        D3DPOOL_SCRATCH, &created, 0);                                        // +0x90
    if (FAILED(hr))
    {
        ::renddx9abi::logf("MovieRenderer.cpp:166 MovieRenderer: Failed to create off-screen "
                           "surface: hr=0x%08X", static_cast<unsigned int>(hr));
        isRecording = false;
    }
    surfaceRows = static_cast<int>(renderer->pp.BackBufferHeight);
    return created;
}

// PE 0x10003510. Any failure logs and stops the recording for good. The frame counter advances
// even when the DDS write failed. Error descriptions: see createCaptureSurface.
void MovieRenderer::captureFrame()
{
    if (!isRecording)
        return;

    IDirect3DSurface9* backBuffer = 0;
    D3DLOCKED_RECT locked;
    locked.Pitch = 0;
    locked.pBits = 0;

    HRESULT hr = renderer->swapChains[0]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO,
                                                        &backBuffer);         // +0x14
    if (FAILED(hr))
    {
        ::renddx9abi::logf("MovieRenderer.cpp:103 MovieRenderer: Failed to get back-buffer: "
                           "hr=0x%08X", static_cast<unsigned int>(hr));
        isRecording = false;
    }
    else if (FAILED(hr = D3DXLoadSurfaceFromSurface(surface, 0, 0, backBuffer, 0, 0,
                                                    D3DX_FILTER_NONE, 0)))
    {
        ::renddx9abi::logf("MovieRenderer.cpp:110 MovieRenderer: Failed to copy frame to "
                           "off-screen surface: hr=0x%08X", static_cast<unsigned int>(hr));
        isRecording = false;
    }
    else if (encoder)
    {
        hr = surface->LockRect(&locked, 0, D3DLOCK_READONLY);                 // +0x34
        if (FAILED(hr))
        {
            ::renddx9abi::logf("MovieRenderer.cpp:118 MovieRenderer: Failed to lock offscreen "
                               "surface. hr=0x%08X", static_cast<unsigned int>(hr));
            isRecording = false;
        }
        else
        {
            encoder->addFrame(locked.pBits, locked.Pitch, surfaceRows);       // +0x00
        }
    }
    else
    {
        std::string name = movieFrameFileName(filePrefix, movieIndex, frameIndex);
        pathReplaceSeparators(name, '\\');                                    // sub_100751C0
        hr = D3DXSaveSurfaceToFileA(name.c_str(), D3DXIFF_DDS, surface, 0, 0);
        if (FAILED(hr))
        {
            ::renddx9abi::logf("MovieRenderer.cpp:133 MovieRenderer: Failed to write rendered "
                               "frame to disk: hr=0x%08X", static_cast<unsigned int>(hr));
            isRecording = false;
        }
        ++frameIndex;
    }

    if (backBuffer)
        backBuffer->Release();
    if (locked.pBits)
        surface->UnlockRect();                                                // +0x38
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
