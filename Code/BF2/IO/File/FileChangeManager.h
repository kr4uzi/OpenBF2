// Debounced file-change notifications. Header path [INFERRED].
// Registers each resolved directory once and records file timestamps. Directory callbacks enqueue
// at most eleven pending entries; update processes at most one directory after the 0.1-second delay.
// Timestamp comparison uses FileInfo's trailing 64-bit field, not its st_mtime field. The original
// Unix loose-file path zeroes that field, so it cannot detect such changes; preserve this distinction.

#ifndef IO_FILE_FILECHANGEMANAGER_H     // [INFERRED] guard style, matched to FileManager.h
#define IO_FILE_FILECHANGEMANAGER_H

#include <map>
#include <string>
#include <vector>

#include "../../Core/IBase.h"
#include "../../Core/Ids.h"            // the io:: filesystem-change IDs
#include "IFileSystem.h"                // dice::hfe::NoCaseStringCompare, io::FileInfo, IFileSystem

namespace dice {
namespace hfe {
namespace io {

// Receives file-change callbacks. The manager holds a counted reference while registered.
// [UNRESOLVED] Interface slots beyond those observed here.
class IFileChangeListener : public IBase
{
public:
    // Called from processChange 0x8481904 as (fullPath, file.userData). [CHOSEN, not recovered]
    // name - the slot and both argument types are read from the call site, the identifier is not.
    virtual void onFileChange(const std::string& fullName, unsigned int userData) = 0;  // +0x10

protected:
    ~IFileChangeListener() {}
};

// Directory-change callback interface. It is FileChangeManager's secondary base.
class IDirectoryChangeListener : public IBase
{
public:
    // Signature attested from the mangled name: ...17onDirectoryChangeERKSsbj.
    virtual void onDirectoryChange(const std::string& dir,
                                   bool               unknownB,      // [UNRESOLVED] - unused
                                   unsigned int       unknownJ) = 0; // [UNRESOLVED] - unused

protected:
    ~IDirectoryChangeListener() {}
};

// Directory watching provided by the base OS filesystem through queryInterface.
class IDirectoryChangeNotifier : public IBase
{
public:
    // addPath pushes (listener, path, 0, 16). [UNRESOLVED] what the last two mean; typed from the
    // widths pushed (a byte-tested bool and a dword).
    virtual bool addListener(IDirectoryChangeListener* listener,
                             const std::string&        path,
                             bool                      unknown0,
                             unsigned int              unknown16) = 0;   // +0x10
    virtual void removeListener(IDirectoryChangeListener* listener) = 0; // +0x14

protected:
    ~IDirectoryChangeNotifier() {}
};

// File-change registration interface. Lifetime is managed through release; do not add a destructor slot.
class IFileChangeManager : public IBase
{
public:
    virtual std::string addListener(IFileChangeListener* listener,
                                    const std::string&   fileName,
                                    unsigned int         userData)      = 0;  // +0x10
    virtual bool        removeListener(IFileChangeListener* listener,
                                       const std::string&   fileName)   = 0;  // +0x14
    virtual bool        removeListener(IFileChangeListener* listener)   = 0;  // +0x18
    virtual void        setEnable(bool enable)                          = 0;  // +0x1C
    virtual bool        getEnable()                                     = 0;  // +0x20
    virtual void        setDelayTime(float seconds)                     = 0;  // +0x24
    virtual float       getDelayTime()                                  = 0;  // +0x28
    virtual void        update()                                        = 0;  // +0x2C

protected:
    ~IFileChangeManager() {}
};

// Debounces directory changes and notifies registered file listeners.
// Preserve virtual order: destructor, directory callback, then allowFileChange.
class FileChangeManager : public IFileChangeManager, public IDirectoryChangeListener
{
public:
    // One watched file and its counted listener reference. The original x86 record is 20 bytes.
    struct File
    {
        IFileChangeListener* listener;   // +0x00  counted; released by removeListener/removeAll
        std::string          name;       // +0x04  extractFileName of the resolved path - leaf only
        unsigned int         userData;   // +0x08  the caller's third addListener argument, passed
                                         //        back verbatim on notification
        long long            time;       // +0x0C  64-bit; see the getFileTime finding at the top
    };

    // 8 bytes - the stride in onDirectoryChange (`dword30 + 8`) and update (`dword2C += 2`).
    struct DirChange
    {
        std::string dir;    // +0x00
        float       time;   // +0x04  System::getExactTime() when the notification arrived
    };

    typedef std::map<std::string, std::vector<File>, NoCaseStringCompare> Files;

    FileChangeManager();

    // IBase
    virtual void  addRef();                                     // +0x00
    virtual int   getRef();                                     // +0x04
    virtual int   release();                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;       // +0x0C

    // IFileChangeManager
    // Returns the resolved full path that was registered, or "" on any failure - same convention
    // as FileManager::fileExistsEx, which is where the path comes from.
    virtual std::string addListener(IFileChangeListener* listener,
                                    const std::string&   fileName,
                                    unsigned int         userData);        // +0x10
    virtual bool        removeListener(IFileChangeListener* listener,
                                       const std::string&   fileName);     // +0x14
    virtual bool        removeListener(IFileChangeListener* listener);     // +0x18
    virtual void        setEnable(bool enable);                            // +0x1C
    virtual bool        getEnable();                                       // +0x20
    virtual void        setDelayTime(float seconds);                       // +0x24
    virtual float       getDelayTime();                                    // +0x28
    virtual void        update();                                          // +0x2C

    // Own virtuals, in declaration order
    virtual ~FileChangeManager();                                          // +0x30 / +0x34

    virtual void onDirectoryChange(const std::string& dir,
                                   bool               unknownB,
                                   unsigned int       unknownJ);           // +0x38

    // A veto hook. The shipped body is `return true` and nothing overrides it in this binary, but
    // it is a real virtual slot and processChange consults it before every notification.
    virtual bool allowFileChange(const std::string& fullName);             // +0x3C

    // non-virtual
    void      init();                              // sets m_initialised only
    bool      addPath(const std::string& path);
    long long getFileTime(const std::string& name);
    void      removeAll();
    void      processChange(const std::string& dir);

    // The ClassManager registry entry. Same shape as io::UnixFileSystem::create.
    static IBase* create(unsigned int iid, IBase* outer);

private:
    // Native STL sizes vary by toolchain; these reference offsets do not define the rebuilt object size.
    int                      m_refCount;      // +0x08  starts at 1
    bool                     m_initialised;   // +0x0C  [INFERRED] name; set by init(), tested by
                                              //        addListener and by nothing else
    Files                    m_files;         // +0x10  tree ptr, +0x14 node count, +0x18 comparator
    std::vector<std::string> m_paths;         // +0x1C / +0x20 / +0x24 - directories already handed
                                              //        to the IDirectoryChangeNotifier
    bool                     m_dirty;         // +0x28  [INFERRED] name. Set by addListener,
                                              //        removeListener and removeAll; cleared and
                                              //        re-tested by processChange, which restarts
                                              //        its scan whenever it sees it - i.e. it means
                                              //        "a listener callback mutated m_files, your
                                              //        iterators are dead".
    std::vector<DirChange>   m_dirChanges;    // +0x2C / +0x30 / +0x34 - the pending queue
    float                    m_delayTime;     // +0x38  0.1f (0x3DCCCCCD) from the constructor
    bool                     m_enable;        // +0x3C  false; BF2Engine::init sets it true
};

// ds:0x8AC8C84 = _ZN4dice3hfe2io17fileChangeManagerE (attested). defined in Core/Globals.cpp -
// do not define it here.
extern FileChangeManager* fileChangeManager;

// The filesystem-change IDs (IID_IFileChangeManager, IID_IFileChangeListener,
// IID_IDirectoryChangeListener, IID_IDirectoryChangeNotifier, CID_FileChangeManager) are the
// dice::hfe::io constants in Core/Ids.h, included above. They used to be declared extern here,
// which gave Ids.h's definitions external linkage in every TU that included this header first:
// FileChangeManager.cpp and the client's BF2Engine.cpp then defined them twice.

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_FILE_FILECHANGEMANAGER_H
