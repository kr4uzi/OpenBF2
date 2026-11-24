// Path: attested by Game/Main/BF2Log.cpp at ELF 0x8893193.
// Contents: recovered logging and diagnostic handling from the symbolized ELF.
// The dedicated initLogFiles is a shipped true-returning stub; do not add file creation there.

#include "BF2Log.h"
#include "../../Core/System.h"

#include <cstdlib>       // exit - displayErrorWindow and the window-suppressed path
#include <iostream>      // std::cerr - displayErrorWindow's "FATAL ERROR: " line
#include <set>           // sStoredCaptions - the dead caption set, 0x8AACC24
#include <sstream>       // the callback's message building

#include "../../IO/Console/Console.h"   // io::mainConsole / OldConsole::output, getWorkingFile
#include "../../IO/File/File.h"         // io::File - the two log-file members
#include "../../IO/File/FileManager.h"  // io::fileManager - renameAndReOpenPureLog's three slots
#include "../../IO/Stream/IStream.h"    // IStream::flush - flushWrite's +0x2C call
#include "../Utility/BuildNrUtil.h"     // getBuildNrString - the 3/6 block's Version line
#include "../Utility/Vars.h"            // Vars::getInt - "hack-ignore-asserts"

namespace dice {
namespace hfe {

namespace memory {
// Release-image memory instrumentation hook is intentionally empty.
void register_err_callback(void (*callback)(bool, const char*));
void register_err_callback(void (* /*callback*/)(bool, const char*)) { }
}

// Reports allocator failure as DebugTypeError2. The release-image registration
// hook is empty, so it does not invoke this callback.
void memoryDebugCallback(bool ok, const char* text)
{
    if (!ok)
        Debug(DEBUGTYPE_7, "Game/Main/BF2Log.cpp", 688) << text;
}

// Initializes log defaults and registers the allocator error callback. Construct
// early enough to receive failures from other engine-service initialization.
BF2Log::BF2Log()
    : m_debugCallbackDisabled(false)        // +0x04
    , m_doNotDisplayErrorWindow(false)      // +0x05
    , m_exitInsteadOfErrorWindow(true)      // +0x06; retained by BF2.exe 0x44969A
    , m_debugOutputOnlyToFile(false)        // +0x07
    , m_logFileName(/* path literal — see BF2Log.h */)
    , m_logFile(0)                          // +0x18
    , m_pureLogFile(0)                      // +0x1C
{
    // m_string0C / m_logName / m_pureLogName default-construct onto the shared empty rep; that is the
    // three refcount bumps in the binary and needs no code.

    m_debugOutputOnlyToFile    = false;

    // Allocator failures route here. The pointer is
    // dice::hfe::memoryDebugCallback (0x813E3AA loads its offset), defined above.
    memory::register_err_callback(&memoryDebugCallback);
#if DEDICATED
    // Dedicated PE 0x426DD6 / ELF 0x813E3B6; absent from client 0x449680.
    m_exitInsteadOfErrorWindow = false;
#endif
}

// Closes owned log files, then unhooks memory and debug callbacks in registration
// order. Explicit close precedes deletion; callback removal restores internal diagnostics.
BF2Log::~BF2Log()
{
    if (m_logFile)                                                  // +0x18
    {
        m_logFile->close();
        delete m_logFile;
    }
    if (m_pureLogFile)                                              // +0x1C
    {
        m_pureLogFile->close();
        delete m_pureLogFile;
    }

    memory::register_err_callback(0);
    setDebugCallback(static_cast<DebugType>(0), 0, 0);              // Debug
    setDebugCallback(static_cast<DebugType>(2), 0, 0);              // Warning
    setDebugCallback(static_cast<DebugType>(3), 0, 0);              // Assert
    setDebugCallback(static_cast<DebugType>(5), 0, 0);              // DxAssert
    setDebugCallback(static_cast<DebugType>(1), 0, 0);              // Info
    setDebugCallback(static_cast<DebugType>(6), 0, 0);              // Error
    setDebugCallback(static_cast<DebugType>(8), 0, 0);              // Log
}

// . A stub — see the header block. Returns true, opens nothing.
bool BF2Log::initLogFiles(const std::string& /*baseName*/)
{
    return true;
}

// setExitInsteadOfErrorWindow ignores its argument and always clears the flag,
// preserving the original setter behavior.
bool BF2Log::getDebugCallbackDisabled() const    { return m_debugCallbackDisabled; }
void BF2Log::setDebugCallbackDisabled(bool v)    { m_debugCallbackDisabled = v; }
void BF2Log::setDoNotDisplayErrorWindow(bool v)  { m_doNotDisplayErrorWindow = v; }
bool BF2Log::getDoNotDisplayErrorWindow() const  { return m_doNotDisplayErrorWindow; }
void BF2Log::setExitInsteadOfErrorWindow(bool)   { m_exitInsteadOfErrorWindow = false; }
bool BF2Log::getExitInsteadOfErrorWindow() const { return m_exitInsteadOfErrorWindow; }

// Routes the supported debug types into this logger in their original registration
// order. Fatal diagnostics are logged before the Debug destructor aborts. Returns true.
bool BF2Log::initDebugCallback()
{
    setDebugCallback(static_cast<DebugType>(0), &BF2Log::debugCallback, this);   // Debug
    setDebugCallback(static_cast<DebugType>(2), &BF2Log::debugCallback, this);   // Warning
    setDebugCallback(static_cast<DebugType>(3), &BF2Log::debugCallback, this);   // Assert
    setDebugCallback(static_cast<DebugType>(5), &BF2Log::debugCallback, this);   // DxAssert
    setDebugCallback(static_cast<DebugType>(1), &BF2Log::debugCallback, this);   // Info
    setDebugCallback(static_cast<DebugType>(6), &BF2Log::debugCallback, this);   // Error
    setDebugCallback(static_cast<DebugType>(8), &BF2Log::debugCallback, this);   // Log

    return true;
}

// Appends the entry's exact bytes when the file is open; otherwise does nothing.
// [PLATFORM] Win32 also preserves types 3/6/5 in crash history even with no open
// file. ELF ignores DebugType; neither platform prefixes the file entry with it.
void BF2Log::addPureLogEntry(const std::string& entry)
{
    if (m_pureLogFile && m_pureLogFile->isOpen())
        m_pureLogFile->write(entry);
}

void BF2Log::addLogEntry(DebugType type, const std::string& entry)
{
#if defined(_WIN32)
    if (type == DebugTypeAssert || type == DebugTypeError || type == DebugTypeDxAssert)
        addCrashDumpAssertInfo(entry);
#endif
    if (m_logFile && m_logFile->isOpen())
        m_logFile->write(entry);
}

// renameAndReOpenPureLog - ELF 0x813EBD0. Rotates pure then debug log under prefix + "Logs/".
// Only a successful move updates the base name and reopens at EOF; a failed move leaves the log closed.
// [CHOSEN, not recovered] rotateOne shares the two recovered blocks without changing their call order.
// [UNRESOLVED] Preserve numeric open-mode values; existing enum names do not establish their meaning.
namespace {

void rotateOne(io::File* file, std::string& name,
               const std::string& prefix, const std::string& suffix)
{
    if (!file || !file->isOpen())
        return;

    file->close();

    const std::string target = prefix + "Logs/" + name + suffix + ".log";
    if (io::fileManager->fileExists(target))            // vt +0x1C
        io::fileManager->deleteFile(target);            // vt +0x20 - clear the destination first

    const std::string source = prefix + "Logs/" + name + ".log";
    if (!io::fileManager->moveFile(source, target))     // vt +0x24, (from, to)
        return;                                         // failed: the File stays closed. Shipped.

    name.append(suffix);                                // the member now carries the tag
    file->open(prefix + "Logs/" + name + ".log",
               io::IFileSystem::FILEMODE_READ, io::IFileSystem::FILEACCESS_2, 0);
    file->seek(io::SEEKORIGIN_END, 0);                  // append, do not truncate
}

} // namespace

void BF2Log::renameAndReOpenPureLog(const std::string& prefix, const std::string& tag)
{
    const std::string suffix = "_" + tag;               // built once for both blocks

    rotateOne(m_pureLogFile, m_pureLogName, prefix, suffix);   // +0x1C with +0x14 - first in the ELF
    rotateOne(m_logFile,     m_logName,     prefix, suffix);   // +0x18 with +0x10
}

// Flushes logs and reports a fatal error to stderr before exiting. hack-ignore-asserts
// can suppress assertion termination, but cannot suppress an Error.
void BF2Log::displayErrorWindow(const std::string& text, DebugType type)
{
    flushWrite();

    if (type == DebugTypeError || *Vars::getInt("hack-ignore-asserts", 0) != 1)
    {
        std::cerr << "FATAL ERROR: " << text << std::endl;
        exit(1);
    }
}

// Flushes typed log then pure log when their files are present and open.
// An open File must provide a valid stream.
void BF2Log::flushWrite()
{
    if (m_logFile && m_logFile->isOpen())
        m_logFile->getStream()->flushWrite();

    if (m_pureLogFile && m_pureLogFile->isOpen())
        m_pureLogFile->getStream()->flushWrite();
}

// Inactive until armed. [CHOSEN] file-static linkage; [UNRESOLVED] original names.
static bool sStoreCaption = false;
static std::set<std::string> sStoredCaptions;

// debugCallback - ELF 0x813FC40. Disabled callbacks suppress only Debug/Info/Warning/Log; errors still report.
// Keep the distinct Assert/Error and DxAssert formatting and fatal-window behavior.
// The Assert/Error arm reads mainConsole without a null guard; Info/Warning guard it.
// DxAssert passes Assert to displayErrorWindow. The basename parameter is intentionally unused.
void BF2Log::debugCallback(DebugType type, const std::string& module,
                           const std::string& /*file - never read, see above*/, int line,
                           const std::string& func, const std::string& text,
                           const std::string& fullFileName, void* data)
{
    BF2Log* log = 0;
    if (data)
    {
        log = static_cast<BF2Log*>(data);
        if (log->getDebugCallbackDisabled())
        {
            if (type <= DebugTypeWarning || type == DebugTypeLog)
                return;                             //  / 0x813FCA0 - the partial mute
        }
    }

    std::string typeName;
    switch (type)                                   // jpt_813FCC4 - literals verbatim
    {
    case DebugTypeDebug:    typeName = "Debug";     break;
    case DebugTypeInfo:     typeName = "Info";      break;
    case DebugTypeWarning:  typeName = "Warning";   break;
    case DebugTypeAssert:   typeName = "Assert";    break;
    case DebugTypeDxAssert: typeName = "DxAssert";  break;
    case DebugTypeError:    typeName = "Error";     break;
    case DebugTypeLog:      typeName = "Log";       break;
    default:                break;                  // empty, not "(unknown)" - see the block above
    }

    std::ostringstream out;                         // the header line - built for every type
    out << fullFileName << '(' << line << "): " << typeName << ": " << module << ": ";
    if (type == DebugTypeAssert)
        out << "(" << func << ") ";
    out << text << std::endl;

    if (log)
    {
        if (type == DebugTypeLog)
        {
            std::string entry(text);                // text + '\n', NOT the header line
            entry.append(1, '\n');
            log->addPureLogEntry(entry);
        }
        else
        {
            log->addLogEntry(type, out.str());
        }
    }

    switch (type)                                   // jpt_8140009 - 7 entries; 0, 4, 7, 8 -> nothing
    {
    case DebugTypeInfo:
        if (io::mainConsole)
        {
            std::string entry(text);
            entry.append(1, '\n');
            io::mainConsole->output(entry);
        }
        break;

    case DebugTypeWarning:
    {
        std::ostringstream warning;
        warning << typeName << '[' << module << "] : " << text << std::endl;
        if (io::mainConsole)
            io::mainConsole->output(warning.str());
        break;
    }

    case DebugTypeAssert:                           // shared block, real type kept
    case DebugTypeError:
    {
        std::ostringstream caption;                 // never written - the MessageBox fossil
        std::ostringstream msg;
        msg << "Debug assertion failed!" << std::endl << std::endl;

        std::string buildDate("implement unix date here");
        std::string buildNr(BuildNrUtil::getBuildNrString());
        msg << "Version: " << buildNr << " Build date:" << buildDate << std::endl;

        msg << "Module: " << module << std::endl;
        msg << "File: " << fullFileName << std::endl;
        msg << "Line: " << line << std::endl << std::endl;

        if (!func.empty())
            msg << "Expression: " << func << std::endl;
        if (!text.empty())
            msg << "Text: " << text << std::endl;
        msg << std::endl;

        // unguarded on purpose - 0x8140C4C has no null test on mainConsole (see the block above)
        msg << "Current confile:" << io::mainConsole->getWorkingFile();
        msg << std::endl;

        if (log)
        {
            log->addPureLogEntry(msg.str());

            if (log->getDoNotDisplayErrorWindow())
            {
                if (log->getExitInsteadOfErrorWindow())
                    exit(0);                        // BF2.exe 0x44AA77; requires window suppression
            }
            else
            {
                log->displayErrorWindow(msg.str(), type);   // fatal error + exit(1)
            }

            if (sStoreCaption)                      // dead, see the statics above
            {
                sStoredCaptions.insert(caption.str());
                sStoreCaption = false;
            }
        }
        break;
    }

    case DebugTypeDxAssert:
    {
        std::ostringstream msg;
        msg << "Debug assertion failed!" << std::endl << std::endl;
        msg << "Module: " << module << std::endl;
        msg << "File: " << fullFileName << std::endl;
        msg << "Line: " << line << std::endl << std::endl;

        if (!text.empty())
            msg << "Text: " << text << std::endl;
        if (!func.empty())
        {
            std::string hres = func.substr(0, func.find('\n'));
            msg << "Hres: " << hres << std::endl;
        }
        if (!func.empty())
        {
            std::string errorString = func.substr(func.find('\n') + 1);
            msg << "ErrorString: " << errorString;  // no endl of its own
        }
        msg << std::endl;

        if (log)
        {
            log->addPureLogEntry(msg.str());

            if (log->getDoNotDisplayErrorWindow())
            {
                if (log->getExitInsteadOfErrorWindow())
                    exit(0);
            }
            else
            {
                log->displayErrorWindow(msg.str(), DebugTypeAssert);   // literal 3, not `type`
            }
        }
        break;
    }

    default:
        break;
    }
}

} // namespace hfe
} // namespace dice
