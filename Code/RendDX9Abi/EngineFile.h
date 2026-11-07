// [CHOSEN, not recovered] Helper for the engine's open/size/read/release sequence.
// Reads through FileManager so mounted archives and mod layering apply to shaders, fonts and textures.
// Use the host-compatible typed stream interface and release each opened stream.

#ifndef RENDDX9ABI_ENGINEFILE_H
#define RENDDX9ABI_ENGINEFILE_H

#include <string>
#include "HostFileAbi.h"

namespace renddx9abi {

// The bytes of one engine file, plus whether the read actually happened. `data` is allocated with
// the engine allocator (bf_malloc) and must be released with free(), below - not with delete, and
// not with the CRT's free.
struct EngineFileData
{
    unsigned char* data;
    unsigned int   size;

    EngineFileData() : data(0), size(0) {}
    bool ok() const { return data != 0; }
};

// Opens `path` through the host FileManager, reads it whole, releases the stream, and returns the
// buffer. Returns an empty result if the FileManager is absent or the file is not found.

// the original does not fail quietly here. sub_1001E010 loops on a MessageBox reading
// "... not found!!! _DO_ check your working directory, _AND_ sync your shaders folder before
// calling upon your local rendering programmer/GP. (really!)" and calls exit(99) if the user
// answers Cancel. That dialog is not reproduced - a modal box in a rebuild under test is worse than
// a logged failure - and the divergence is recorded here rather than hidden.
EngineFileData readEngineFile(const char* path);

// Releases what readEngineFile returned.
void freeEngineFile(EngineFileData& file);

// HemiMapManager's resource/output helpers. These preserve the FileManager calls the retail body
// makes through HostFileAbi's typed IFileManager: +0x1C fileExists, +0x18 open with the editor
// write pair (3,2,0), and +0xA4 getWritePath.
bool engineFileExists(const std::string& path);
void engineFilePrepareWrite(const std::string& path);
void engineFileGetWritePath(const std::string& path, std::string& out);

// Owned engine stream for incremental reads and absolute/relative seeks, used by texture mip loading.
// Destruction closes the stream. The retail class is the RendDX9.dll copy of the IO file helper
// (rdebug IO/FileUtil.cpp) that Geom/Terrain.h models as TerrainRawReader; the null guards below are
// this adapter's, the shipped members dereference the stream unguarded.
class EngineStream
{
public:
    EngineStream() : m_stream(0) {}            // PE 0x10075DD0
    ~EngineStream() { close(); }

    // PE 0x10076340 (`retn 10h`): releases the held stream, opens path through FileManager +0x18
    // with the caller's mode, access and fourth argument, stores the result and answers whether it
    // is non-null. The readers pass (2, 0, 0), the editor writers (0, 2, 0).
    bool open(const std::string& path, int mode, int access, unsigned int fourth);
    void close();                                        // stream +0x08 release
    bool isOpen() const { return m_stream != 0; }

    unsigned int read(void* buffer, unsigned int size);  // stream +0x10
    int          seek(int origin, int offset);           // stream +0x30 - 0 = absolute, 1 = relative
    int          getSize();                              // stream +0x38, PE 0x10075E70
    // Stream +0x1C, unguarded as shipped. PE 0x10075E60 (`mov ecx,[ecx]; jmp [eax+1Ch]`).
    int          getAvailableBytes() { return m_stream->getAvailableBytes(); }
    // The held stream. PE 0x10075ED0.
    HostNonlinearStream* stream() const { return m_stream; }
    // Reads 16-bit units up to `terminator` or a short read into out, terminator excluded; always
    // answers true. PE 0x10076070 (`retn 8`). [PLATFORM] std::u16string stands for the retail
    // basic_string<unsigned short>.
    bool readWideLine(std::u16string& out, unsigned short terminator);

private:
    EngineStream(const EngineStream&);
    EngineStream& operator=(const EngineStream&);

    HostNonlinearStream* m_stream;
};

} // namespace renddx9abi

#endif // RENDDX9ABI_ENGINEFILE_H
