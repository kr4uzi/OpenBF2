// Filesystem interfaces used for physical files and mounted archives. [INFERRED] Source placement.

#ifndef IO_FILE_IFILESYSTEM_H     // [INFERRED] guard style
#define IO_FILE_IFILESYSTEM_H

#include "../Stream/IStream.h"   // Stream handles returned by open().
#include <list>
#include <string>
#include <string.h>     // strcasecmp / _stricmp - see the comparator below

#include "../../Core/IBase.h"

namespace dice {
namespace hfe {

// Case-insensitive strict ordering for engine string-keyed containers.
struct NoCaseStringCompare
{
    // RendDX9.dll PE 0x10015140 (COMDAT copy, `retn 8`, this unread: _stricmp(a, b) < 0 via setl),
    // folded across the image's six string-keyed tree instantiations.
    bool operator()(const std::string& a, const std::string& b) const
    {
#ifdef _MSC_VER
        return _stricmp(a.c_str(), b.c_str()) < 0;
#else
        return strcasecmp(a.c_str(), b.c_str()) < 0;
#endif
    }
};

namespace io {

// Streams use the shared polymorphic declaration in IO/Stream/IStream.h.
// read/write return byte counts; seek operations belong to INonlinearStream.

// File name, directory flag, size and creation/access/write timestamps.
// The Unix builder zeroes all timestamps; the Windows builder supplies filetime values.
struct FileInfo
{
    std::string  name;                   // +0x00  (PE +0)
    bool         isDirectory;            // +0x04  (PE +28)  Unix (st_mode & S_IFMT) == S_IFDIR;
                                         //                  Win32 FILE_ATTRIBUTE_DIRECTORY
    unsigned int size;                   // +0x08  (PE +32)  Unix st_size; Win32 nFileSizeLow; zip
                                         //                  uncompressed_size
    unsigned int creationTimeLow;        // +0x0C  (PE +40)  three filetime pairs, low dword first;
    unsigned int creationTimeHigh;       // +0x10  (PE +44)  Win32 only - zero on Unix and in a zip
    unsigned int lastAccessTimeLow;      // +0x14  (PE +48)
    unsigned int lastAccessTimeHigh;     // +0x18  (PE +52)
    unsigned int lastWriteTimeLow;       // +0x1C  (PE +56)  what compareFileTime compares
    unsigned int lastWriteTimeHigh;      // +0x20  (PE +60)
};

// Filesystem modes and access flags. Namespace-level types are also exposed through interface aliases.
enum FileMode
{
    // [UNRESOLVED] enumerator names. FileManager::open treats 2 as the read mode: anything
    // other than 2 short-circuits to the write path. SettingsManagerBase::save passes 0.
    FILEMODE_READ = 2
};

enum FileAccess
{
    // [UNRESOLVED] enumerator names. FileManager::open also takes the write path when the
    // access is 1 or 2, so both of those are write-ish. SettingsManagerBase::save passes 2.
    FILEACCESS_1 = 1,
    FILEACCESS_2 = 2
};

class IFileSystem : public IBase
{
public:
    typedef io::FileMode   FileMode;      // aliases - see the scope note above the class
    typedef io::FileAccess FileAccess;
    static const FileMode   FILEMODE_READ = io::FILEMODE_READ;   // the nested spellings the tree
    static const FileAccess FILEACCESS_1  = io::FILEACCESS_1;    //   already uses keep compiling
    static const FileAccess FILEACCESS_2  = io::FILEACCESS_2;

    // Filesystem operations. Preserve virtual declaration order and parameter types.
    virtual void init();                                                          // +0x10: ELF 0x8489990 / PE empty ret
    virtual bool isLastFileOpenedFromArchive() const;                             // +0x14

    virtual IStream* open(const std::string& name, FileMode, FileAccess,
                        unsigned int)                                       = 0;  // +0x18
    virtual bool   fileExists(const std::string&)                           = 0;  // +0x1C
    virtual bool   deleteFile(const std::string&)                           = 0;  // +0x20
    virtual bool   moveFile(const std::string& from,
                            const std::string& to)                          = 0;  // +0x24
    // The third parameter is fail-if-exists, not overwrite - UnixFileSystem::copyFile returns
    // false when it is true and the destination is present. And that implementor hard-links.
    virtual bool   copyFile(const std::string& from, const std::string& to,
                            bool failIfExists)                              = 0;  // +0x28
    virtual bool   readFile(const std::string& name, void* buffer, unsigned int size,
                            unsigned int& bytesRead)                        = 0;  // +0x2C
    virtual bool   writeFile(const std::string& name, const void* buffer,
                             unsigned int size, bool append)                = 0;  // +0x30
    virtual int    findFiles(const std::string& path, bool recursive,
                             std::list<FileInfo>& out)                      = 0;  // +0x34
    virtual bool   getFileInfo(const std::string& name, FileInfo& out)      = 0;  // +0x38
    virtual bool   compareFileTime(const std::string& a,
                                    const std::string& b)                    = 0;  // +0x3C
    virtual bool   getFullFileName(std::string& path)                       = 0;  // +0x40
    virtual void   update()                                                 = 0;  // +0x44

    // The two path macros @root@ and @home@ expand through these - see FileManager::getWritePath
    // and the file-scope expandPath in FileManager.cpp.
    // On the dedicated server @home@ is "/tmp" and warns on every call; @root@ is ".".
    virtual const std::string& getHomeSubstitution() const                  = 0;  // +0x48
    virtual const std::string& getRootSubstitution() const                  = 0;  // +0x4C

protected:
    // Keep this destructor non-virtual. Derived interfaces introduce their own destructor slots after their methods.
    ~IFileSystem() {}
};

class IArchiveHandler;      // declared in full below; IArchive::getHandler returns one

// Mounted archive interface. Its destructor precedes the archive-specific virtual methods.
class IArchive : public IFileSystem
{
public:
    virtual ~IArchive() {}                                                        // +0x50 / +0x54

    virtual IArchiveHandler*   getHandler()                                 = 0;  // +0x58
    virtual const std::string& getFilename() const                          = 0;  // +0x5C
    virtual void               setPath(const std::string&)                  = 0;  // +0x60
    virtual void               setContentCheck(bool)                        = 0;  // +0x64
    virtual bool               getContentCheck()                            = 0;  // +0x68
};

// Include ZipArchive.h when the concrete archive type is required.
class ZipArchive;           // IO/File/ZipArchive.h

// The archive factory. Only 7 slots (from ZipArchiveHandler::VMT, 28 bytes): the IBase quartet plus
// three of its own. Register one with FileManager::addArchiveHandler and it becomes able to mount
// whatever extensions it claims.
class IArchiveHandler : public IBase
{
public:
    // Signature from FileManager::openArchive's call site, which passes exactly these three:
    //     h->open(file, m_fileSystem, flag)
    virtual IArchive*   open(const std::string& file, IFileSystem* baseSystem,
                             bool flag)             = 0;   // +0x10  produces the IArchive

    // Returns a borrowed reference to the supported extension list.
    virtual const std::string& getExtensions() const = 0;  // +0x14  drives FileManager::updateArchiveExts
    virtual bool        canWrite() const            = 0;   // +0x18
};

// FileManager interface shared with plugins. Preserve method order and place the destructor after fileExistsEx2.
class IFileManager : public IFileSystem
{
public:
    virtual void         setBaseSystem(IFileSystem*) = 0;                         // +0x50
    virtual IFileSystem* getBaseSystem() = 0;                                     // +0x54
    virtual IStream*     openwin32(const std::string&, FileMode, FileAccess,
                                   unsigned int) = 0;                             // +0x58
    virtual void         addArchiveHandler(IArchiveHandler*) = 0;                 // +0x5C
    virtual void         removeArchiveHandler(IArchiveHandler*) = 0;              // +0x60
    virtual bool         addPathOverload(const std::string&, const std::string&) = 0; // +0x64
    virtual bool         removePathOverload(const std::string&, const std::string&) = 0; // +0x68
    virtual bool         addMergeDirectory(const std::string&, const std::string&) = 0; // +0x6C
    virtual bool         removeMergeDirectory(const std::string&) = 0;            // +0x70
    virtual IArchive*    openArchive(const std::string&, int, bool) = 0;           // +0x74
    virtual void         permitMountArchives(bool) = 0;                            // +0x78
    virtual IArchive*    mountArchive(const std::string&, const std::string&,
                                      bool) = 0;                                   // +0x7C
    virtual bool         unmountArchive(const std::string&) = 0;                  // +0x80
    virtual void         unmountAllArchives() = 0;                                 // +0x84
    virtual void         getAllArchives(std::vector<IArchive*>&) = 0;              // +0x88
    virtual bool         hasMountedArchives() const = 0;                           // +0x8C
    virtual std::string  stripModPath(const std::string&) = 0;                    // +0x90
    virtual void*        getPathList() const = 0;                                  // +0x94
    virtual void         addPath(const std::string&) = 0;                         // +0x98
    virtual void         removePath(const std::string&) = 0;                      // +0x9C
    virtual void         removeAllPaths() = 0;                                     // +0xA0
    virtual void         getWritePath(const std::string&, std::string&) = 0;       // +0xA4
    virtual int          getFileResolveMode() const = 0;                           // +0xA8
    virtual void         setFileResolveMode(int) = 0;                              // +0xAC
    virtual std::string  fileExistsEx(const std::string&, bool) = 0;              // +0xB0
    virtual std::string  fileExistsEx2(const std::string&) = 0;                   // +0xB4
    virtual ~IFileManager() {}                                                      // +0xB8/+0xBC
    virtual void         setModPaths(std::vector<std::string>) = 0;                // ELF +0xC0 / PE +0xBC
};

// Concrete ZIP support is declared in ZipArchive.h.
class ZipArchiveHandler;    // IO/File/ZipArchive.h

// The concrete POSIX filesystem is declared in UnixFileSystem.h.

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_FILE_IFILESYSTEM_H
