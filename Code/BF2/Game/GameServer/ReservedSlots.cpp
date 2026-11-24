// Reserved-player-name list and configuration file loading.
// The singleton is constructed before engine parameter parsing; loading executes the
// configuration through the console, so populating the list requires its command bindings.

#include "ServerSettings.h"
#include "../../IO/Console/Console.h"    // io::mainConsole, io::Console::ignoredString_
#include "../../IO/File/FileManager.h"   // io::fileManager - load()'s existence gate
#include "../../IO/Stream/IStream.h"
#include <algorithm>
#include <sstream>

namespace dice {
namespace hfe {

ReservedSlots::ReservedSlots()
{
}

// Member destruction releases reserved nicknames before the backing filename.
ReservedSlots::~ReservedSlots()
{
}

// instance - ELF 0x80B9B60, 72 bytes. The guarded-function-local-static pattern: guard byte
// object 0x8AAB778, __cxa_atexit(__tcf_0_91) - a Meyers singleton, transcribed as one.
// Identical in shape to MapList::instance and ServerSettings::instance.
ReservedSlots* ReservedSlots::instance()
{
    static ReservedSlots s;
    return &s;
}

// getConfigFile - ELF 0x80B9D90, eight bytes: `mov eax,[ebp+this]; ret`. It returns `this`, which
// is the address of m_configFile because that member sits at +0x00 - and the caller in
// BF2Engine::parseParameters immediately does `mov eax,[eax]; mov eax,[eax-0Ch]` on the result to
// read the string's length (0x81334AC), which only type-checks for a std::string reference.
const std::string& ReservedSlots::getConfigFile() const
{
    return m_configFile;
}

// setConfigFile - ELF 0x80B9DA0, nine bytes, and the whole body is a tail jump:
// `push ebp; mov ebp,esp; pop ebp; jmp std::string::assign(const std::string&)` with `this`
// unchanged - so it assigns straight into the +0x00 member. No validation, no dirty flag.
void ReservedSlots::setConfigFile(const std::string& configFile)
{
    m_configFile.assign(configFile);
}

// Loads the reserved-player console script. Returns false for an empty or missing
// path; otherwise executes the script without clearing the current list.
bool ReservedSlots::load()
{
    if (m_configFile.empty()
        || !io::fileManager->fileExists(m_configFile))    // vt +0x1C on ds:0x8AC8C64
        return false;

    const bool useRelative = io::mainConsole->getUseRelativePaths();
    io::mainConsole->setUseRelativePaths(false);

    io::mainConsole->run(m_configFile, "", "", "", "", "", "", "", "",
                         io::Console::ignoredString_);    // vt +0x28

    io::mainConsole->setUseRelativePaths(useRelative);
    return true;                                          // the run() result is NOT consulted
}

// BF2.exe 0x45D330; ELF 0x80BA0C0. The stream is released before comparing the write count.
bool ReservedSlots::save()
{
    if (m_configFile.empty())
        return false;
    io::IStream* stream = io::fileManager->open(m_configFile,
        static_cast<io::FileMode>(0), io::FILEACCESS_2, 0);
    if (!stream)
        return false;
    std::ostringstream script;
    save(script);
    const std::string text = script.str();
    const unsigned int written = stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
    stream->release();
    return written == text.size();
}

// BF2.exe 0x45D1B0; ELF 0x80BA4E0. Names are quoted verbatim, without escaping.
void ReservedSlots::save(std::ostream& stream)
{
    for (const std::string& nick : m_nicks)
        stream << "reservedSlots.addNick \"" << nick << '"' << std::endl;
}

// BF2.exe 0x45D480; ELF 0x80BAA20.
void ReservedSlots::clear()
{
    m_nicks.clear();
}

// BF2.exe 0x45D2C0; ELF 0x80BAA60. Includes a trailing newline for every name.
std::string ReservedSlots::list()
{
    std::string result;
    for (const std::string& nick : m_nicks)
    {
        result += nick;
        result += "\n";
    }
    return result;
}

// BF2.exe 0x45D490; ELF 0x80BA560. Strip the first prefix, then deduplicate case-sensitively.
void ReservedSlots::addNick(const std::string& nick)
{
    std::string name = nick;
    const std::string::size_type space = name.find(" ");
    if (space != std::string::npos)
        name = name.substr(space + 1);
    if (std::find(m_nicks.begin(), m_nicks.end(), name) == m_nicks.end())
        m_nicks.push_back(name);
}

// BF2.exe 0x45D430; ELF 0x80BA790. Unlike addNick, removal does not strip a prefix.
void ReservedSlots::removeNick(const std::string& nick)
{
    const std::vector<std::string>::iterator it = std::find(m_nicks.begin(), m_nicks.end(), nick);
    if (it != m_nicks.end())
        m_nicks.erase(it);
}

// Strips the clan tag through the first space, then checks the entire remaining nickname case-sensitively.
bool ReservedSlots::isNickReserved(const std::string& nick) const
{
    for (std::vector<std::string>::const_iterator it = m_nicks.begin(); it != m_nicks.end(); ++it)
    {
        std::string name = nick;
        const std::string::size_type space = name.find(" ");
        if (space != std::string::npos)
            name = name.substr(space + 1);               // drop the clan tag and its trailing space
        if (name == *it)
            return true;
    }
    return false;
}

// Publish the singleton pointer before console bindings test its availability.
ReservedSlots* reservedSlots = ReservedSlots::instance();   // ds:0x8AAB768

} // namespace hfe
} // namespace dice
