// Virtual filesystem with search paths, mod overloads and mounted archives. [INFERRED] Header placement.
// The first search path is the write path. Merge directories are unsupported.
// [UNRESOLVED] FileResolveMode enumerator names.

#ifndef IO_FILE_FILEMANAGER_H     // [INFERRED] guard style
#define IO_FILE_FILEMANAGER_H

#include <list>
#include <map>
#include <string>
#include <vector>

#include "IFileSystem.h"

namespace dice {
namespace hfe {
namespace io {

// IArchive, ZipArchive and NoCaseStringCompare come from IFileSystem.h.

// Virtual filesystem combining mounted archives, path overloads and ordered search paths.
// Preserve the inherited filesystem interface order.
class FileManager : public IFileManager
{
public:
    // File operations use the backing filesystem plus inline path and archive containers.
    // Archive mounting requires the permission flag set by permitMountArchives.
    // FileInfo is the namespace-level type in IFileSystem.h; unknown fields remain marked there.

    // IFileSystem, slots +0x00..+0x4C
    virtual void  addRef();                                          // +0x00
    virtual int   getRef();                                          // +0x04
    virtual int   release();                                         // +0x08
    virtual void* queryInterface(unsigned int iid) const;                  // +0x0C

    // Inherits IFileSystem::init: ELF table 0x88DE838 / PE 0x7F3308. No FileManager override.
    virtual bool isLastFileOpenedFromArchive() const;                // +0x14

    virtual IStream* open(const std::string& name, FileMode, FileAccess,
                        unsigned int);                               // +0x18
    virtual bool  fileExists(const std::string&);              // +0x1C
    // The write-side quartet: every path argument goes through getWritePath and is then handed to
    // m_fileSystem. None of them consults the archives or the search-path list, so writes always
    // land in the first path-list entry. moveFile takes two paths (an earlier draft said one).
    virtual bool  deleteFile(const std::string&);                    // +0x20
    virtual bool  moveFile(const std::string& from,
                           const std::string& to);                   // +0x24
    virtual bool  copyFile(const std::string& from, const std::string& to,
                           bool overwrite);                          // +0x28
    // note: its loose-file fallback concatenates path+name directly, so the @MOD@/@root@/@home@
    // macros are not expanded there - unlike open(), which expands per search-path entry.
    virtual bool  readFile(const std::string& name, void* buffer, unsigned int size,
                           unsigned int& bytesRead);                 // +0x2C
    virtual bool  writeFile(const std::string& name, const void* buffer,
                            unsigned int size, bool append);         // +0x30
    // Returns the count. clears `out` on entry. De-duplicates case-insensitively across archives
    // and search paths; the loose-file pass runs only when no archive matched at all. See the .cpp.
    virtual int   findFiles(const std::string& path, bool recursive,
                            std::list<FileInfo>& out);               // +0x34
    // Signature from compareFileTime, its only recovered caller: it fills a record and returns
    // success. [UNRESOLVED] everything in FileInfo except the timestamp compareFileTime reads.
    virtual bool  getFileInfo(const std::string& name,
                              FileInfo& out);                        // +0x38
    // Returns "a is at least as new as b" - a single bool, not a three-way compare. Returns false
    // if either getFileInfo fails, which is indistinguishable from a genuine "older".
    virtual bool  compareFileTime(const std::string& a,
                                  const std::string& b);             // +0x3C
    // not implemented in the shipping build - the body is a severity-7 "not impl" diagnostic.
    virtual bool  getFullFileName(std::string&);                // +0x40  0x8486990  stub
    virtual void  update();                                          // +0x44

    virtual const std::string& getHomeSubstitution() const;                 // +0x48
    virtual const std::string& getRootSubstitution() const;                 // +0x4C
    // Creates the minidump directory.
    void createMiniDumpDirectory();

    // FileManager's own slots begin here

    virtual void  setBaseSystem(IFileSystem*);                       // +0x50
    virtual IFileSystem* getBaseSystem();                      // +0x54
    // Identical to open() except that it never searches the archives - loose files only. A Win32
    // caller needing a real OS handle cannot be handed an entry unpacked from a .zip.
    virtual IStream* openwin32(const std::string& name, FileMode, FileAccess,
                             unsigned int);                          // +0x58

    // Archive layer
    virtual void addArchiveHandler(IArchiveHandler*);                // +0x5C
    virtual void removeArchiveHandler(IArchiveHandler*);             // +0x60
    virtual bool addPathOverload(const std::string& from,
                                 const std::string& to);             // +0x64
    // Erases one specific (from, to) pair - both halves are compared. The correct counterpart to
    // addPathOverload's insert_equal.
    virtual bool removePathOverload(const std::string& from,
                                    const std::string& to);          // +0x68
    virtual bool addMergeDirectory(const std::string&, const std::string&);  // +0x6C 0x8487810 stub
    virtual bool removeMergeDirectory(const std::string&);           // ELF 0x84884F0 / PE 0x64DE10: al=0/1
    // resolved arity: mountArchive's call site passes three arguments after `this`, while IDA's
    // decompilation of the callee shows two. Call sites push real arguments, whereas IDA elides
    // parameters the callee never reads - so three is the better reading and the middle one is
    // simply unused here. [UNRESOLVED] what that middle parameter means.
    virtual IArchive* openArchive(const std::string& file, int unused,
                                  bool flag);                        // +0x74
    virtual void permitMountArchives(bool);                          // +0x78

    // corrected: three parameters, not two - (archiveName, mountPath, contentCheck) - and it
    // returns the mounted IArchive*, not a bool. See the .cpp for the four gates it applies.
    virtual IArchive* mountArchive(const std::string& archiveName,
                                   const std::string& mountPath,
                                   bool contentCheck);               // +0x7C
    virtual bool unmountArchive(const std::string&);                 // +0x80
    virtual void unmountAllArchives();                               // +0x84
    // appends to the caller's vector rather than replacing its contents.
    virtual void getAllArchives(std::vector<IArchive*>& out);  // +0x88
    virtual bool hasMountedArchives() const;                         // +0x8C

    // Path layer
    // corrected after reading the body: narrower than the name suggests. It strips the mod prefix
    // only from shader paths - both "mods/" and "shaders" must be present. See the .cpp.
    virtual std::string stripModPath(const std::string&);      // +0x90
    virtual void* getPathList() const;                               // +0x94
    // Inserts at the front: the most recently added path is searched first and is the write path.
    // That is the whole mod-priority mechanism - see the .cpp.
    virtual void  addPath(const std::string&);                       // +0x98
    // case-sensitive, unlike almost everything else here - the comparison is std::string
    // operator==, not compareIgnoreCase. See the .cpp.
    virtual void  removePath(const std::string&);                    // +0x9C
    virtual void  removeAllPaths();                                  // +0xA0

    // corrected: takes the name and an out-parameter, and returns void. Writes only ever resolve
    // against the first path-list entry - never the archives, never the rest of the list.
    virtual void getWritePath(const std::string& name, std::string& out);  // +0xA4

    // 1 == the content lockdown: loose-file fallback is refused unless the path mentions @home@,
    // @root@ or "png". [UNRESOLVED] the enumerator names and the other modes.
    virtual int  getFileResolveMode() const;                         // +0xA8
    virtual void setFileResolveMode(int);                            // +0xAC

    // corrected: fileExistsEx does not return a bool. It returns the resolved full path, or an
    // empty string when not found - which is why it is 1,161 bytes while plain fileExists is 137.
    // The flag skips the archive layer so a caller can ask specifically for a loose file.
    virtual std::string fileExistsEx(const std::string&, bool skipArchives);  // +0xB0
    virtual std::string fileExistsEx2(const std::string&);                    // +0xB4

    virtual ~FileManager();                                          // +0xB8 / +0xBC
    // note the by-value vector - not a const reference, unlike every other string parameter in this
    // class. Taken verbatim from the mangled name at 0x8489A50.
    virtual void setModPaths(std::vector<std::string>);              // +0xC0

    FileManager();
    // Creates the requested interface; rejects aggregation and unsupported interfaces with null.
    static IBase* create(unsigned int iid, IBase* outer);

    // Calls an archive's deleting destructor (vtable +0x54) - what unmountArchive's exact branch
    // and unmountAllArchives both use. Modelled as a helper because it is a raw slot dispatch.
    static void destroyArchive(IArchive*);

    void updateArchiveExts();

    // Appends borrowed archive lists mounted above or below pattern. The mount table owns them.
    void findAllMatchingArchives(const std::string& pattern,
                                 std::vector<std::vector<IArchive*>*>& out) const;
    // Returns the archive list whose mount point prefixes dir, or null when none matches.
    std::vector<IArchive*>* findArchives(const std::string& dir);

private:
    // Layout, 68 (0x44) bytes
    /* 0x00 */ void*           pVMT;
    /* 0x04 */ int             m_refCount;

    // Case-insensitive mount-point map. Multiple archives can share a mount point.
    /* 0x08 */ std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> m_archives;
    /* 0x0C */ // (the map's node count - part of the container, not a member)
    // Base filesystem used for physical file access; installed after construction.
    /* 0x14 */ IFileSystem*    m_fileSystem;             // the base system doing real I/O
    // Registered archive handlers, retained by the manager.
    /* 0x18 */ std::vector<IArchiveHandler*> m_archiveHandlers;   // occupies 0x18..0x23

    // Unused merge-directory record with borrowed target pointers. [UNRESOLVED] Target type.
    // addMergeDirectory refuses requests, so production code does not populate this list.
    struct MergeDirectory { std::string path; std::vector<void*> targets; };
    /* 0x24 */ std::list<MergeDirectory> m_mergeDirectories;

    // identified from addPathOverload's mangled _Rb_tree instantiation. A multimap - insert_equal,
    // so one source path may overload to several targets and all are kept.
    /* 0x28 */ std::multimap<std::string, std::string, NoCaseStringCompare> m_pathOverloads;

    // Cached uppercase, space-separated handler extensions, refreshed when handlers are added.
    // openArchive queries each handler separately.
    /* 0x34 */ std::string     m_archiveExtensions;
    // Inline search-path storage; the first entry is the write path. Native STL widths determine actual offsets.
    /* 0x38 */ std::list<std::string> m_pathList;
    /* 0x3C */ int             m_fileResolveMode;        // [UNRESOLVED] enumerators
    /* 0x40 */ bool            m_isLastFileOpenedFromArchive;
    /* 0x41 */ bool            m_permitMountArchives;
    // [UNRESOLVED] Reserved bytes have no observed dedicated-server readers or initialization.
    /* 0x42 */ bool            byte42;                   // [UNRESOLVED] - no toucher, no initialiser
    /* 0x43 */ bool            byte43;                   // [UNRESOLVED] - no toucher, no initialiser
};

// Layout check for this toolchain's native FileManager declaration.
// Modern STL containers differ from VC7.1; this class also retains an explicit pVMT beside its
// implicit vptr. Rebuilt instances are internally consistent but must not overlay retail host objects.
// If this assertion changes, measure field/container widths before updating the expected size.
#if defined(_WIN64)
// [CHOSEN, not recovered] Native MSVC x64 port, /MD and _ITERATOR_DEBUG_LEVEL=0:
// string 32, vector 24, list/map/multimap 16. The existing explicit pVMT is retained;
// with native pointers and alignment, m_fileSystem is +0x28, m_pathList +0x88,
// m_fileResolveMode +0x98 and sizeof is 0xA0. This is not a retail-host ABI view.
static_assert(sizeof(std::string) == 32 && sizeof(std::vector<IArchiveHandler*>) == 24,
              "native x64 FileManager string/vector widths");
static_assert(sizeof(std::list<std::string>) == 16 &&
              sizeof(std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare>) == 16 &&
              sizeof(std::multimap<std::string, std::string, NoCaseStringCompare>) == 16,
              "native x64 FileManager list/map widths");
static_assert(sizeof(FileManager) == 0xA0, "native x64 FileManager layout changed - revalidate STL widths");
#elif defined(__LP64__) && defined(_LIBCPP_VERSION)
// [CHOSEN, not recovered] Native macOS port on libc++: string 24, vector 24, list 24. As on
// Linux, no total size is pinned and nothing reads a shipped host object through this class.
static_assert(sizeof(std::string) == 24 && sizeof(std::vector<IArchiveHandler*>) == 24 &&
              sizeof(std::list<std::string>) == 24,
              "libc++ FileManager string/vector/list widths");
#elif defined(__LP64__)
// [CHOSEN, not recovered] Native Linux x86-64 port on libstdc++: the same
// native declaration as the MSVC x64 branch, but libstdc++'s containers are wider (list 24, map and
// multimap 48), so no total size is pinned here - only the widths the table above reasons about.
// Nothing reads a shipped host object through this class on Linux either.
static_assert(sizeof(std::string) == 32 && sizeof(std::vector<IArchiveHandler*>) == 24 &&
              sizeof(std::list<std::string>) == 24,
              "libstdc++ FileManager string/vector/list widths");
#else
static_assert(sizeof(FileManager) == 0x5C,
              "FileManager's layout changed - re-validate the STL widths against the retail table "
              "in this header before updating this number");
#endif

extern FileManager* fileManager;     // _ZN4dice3hfe2io11fileManagerE

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_FILE_FILEMANAGER_H
