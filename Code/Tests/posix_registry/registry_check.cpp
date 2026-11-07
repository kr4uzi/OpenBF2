// [TEMP] [CHOSEN, not recovered] Production registry backend under an isolated HOME.
#include "../../BF2/Core/Registry.h"
#if !DEDICATED
#include "../../BF2/Core/PosixRegistry.h"
#include "../../BF2/Platform/macOS/ClientPaths.h"
#include "../../BF2/Game/MatchMaking/Client/CdKeyStore.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace dice::hfe;
static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

int main()
{
#if DEDICATED
    CHECK(registry->getString(RegKeyLocalMachine, "Language", "English", "").empty());
#else
    const char* home = std::getenv("HOME");
    if (!home || std::string(home).find("/out/") == std::string::npos)
        return 2;
    const std::string directory = macos::clientDataDirectory();
    CHECK(directory == std::string(home) + "/Library/Application Support/OpenBF2");
    const std::string filename = directory + "/Registry.con";
    CHECK(access(filename.c_str(), F_OK) != 0);
    CHECK(registry->getString(RegKeyLocalMachine, "Language", "English", "") == "English");
    CHECK(access(filename.c_str(), F_OK) != 0); // Reading never creates the file.
    CHECK(setenv("BF2_MAC_DEV_CDKEY", "IGNORED-OLD-OVERRIDE", 1) == 0);
    CHECK(getRegistryCdKey("").empty());

    const std::string base = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Electronic Arts\\EA Games\\";
    const std::string bf2 = base + "Battlefield 2\\";
    const std::string contents =
        "# Fixture only: no real keys\r\n ; whole-line comment\r\n\r\n"
        "malformed line\n"
        "NOT_A_HIVE\\value = ignored\n" +
        bf2 + "Language = French\n" +
        "  hkey_local_machine\\software\\electronic arts\\ea games\\battlefield 2\\LANGUAGE\t= English\t\r\n" +
        bf2 + "ergc\\@ = FALLBACKKEY\n" +
        bf2 + "Empty =   \r\n" +
        bf2 + "Literal = a=b #literal; \\no-escapes \n" +
        bf2 + "Quoted = \"literal quotes\"\n" +
        "HKEY_CURRENT_USER\\SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2\\Language = German\n" +
        base + "Battlefield 2 Special Forces\\ergc = EXPANSIONKEY\n" +
        base + "Battlefield 2142\\InstallDir\\@ =\n" +
        bf2 + "ergc = LOOPBACKTESTKEY00000"; // EOF without newline.
    {
        std::ofstream output(filename);
        output << contents;
        CHECK(output.good());
    }
    CHECK(chmod(filename.c_str(), 0400) == 0);
    CHECK(registry->getString(RegKeyLocalMachine, "Language", "fallback", "") == "English");
    CHECK(registry->getString(RegKeyCurrentUser, "Language", "fallback", "") == "German");
    CHECK(registry->getString(RegKeyLocalMachine, "Unknown", "fallback", "") == "fallback");
    CHECK(registry->getString(RegKeyLocalMachine, "Unknown", "", "").empty());
    CHECK(registry->getString(RegKeyLocalMachine, "Empty", "fallback", "").empty());
    CHECK(registry->getString(RegKeyLocalMachine, "Literal", "", "") == "a=b #literal; \\no-escapes");
    CHECK(registry->getString(RegKeyLocalMachine, "Quoted", "", "") == "\"literal quotes\"");
    CHECK(getRegistryCdKey("") == "LOOPBACKTESTKEY00000");
    CHECK(getRegistryCdKey("Battlefield 2 Special Forces") == "EXPANSIONKEY");
    CHECK(getRegistryCdKey("Unknown Product").empty());
    CHECK(posixRegistry::keyExists(RegKeyLocalMachine,
        "SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2142\\InstallDir"));
    CHECK(!posixRegistry::keyExists(RegKeyCurrentUser,
        "SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2142\\InstallDir"));
    CHECK(!posixRegistry::keyExists(RegKeyLocalMachine,
        "SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2142\\Install"));
    std::string unchanged = "unchanged";
    CHECK(!posixRegistry::readValue(RegKeyLocalMachine, "missing", "missing", unchanged));
    CHECK(unchanged == "unchanged");
    // Exercise the challenge's existing protected CD-key representation, without a client run.
    const std::string protectedKey = translateCdKey(getRegistryCdKey(""));
    CHECK(protectedKey.compare(0, 5, "x9392") == 0);
    CHECK(translateCdKey(protectedKey) == "LOOPBACKTESTKEY00000");
    std::ifstream input(filename);
    std::ostringstream after;
    after << input.rdbuf();
    CHECK(after.str() == contents);

    CHECK(chmod(filename.c_str(), 0600) == 0);
    {
        std::ofstream output(filename);
        output << bf2 << "ergc\\@ = FALLBACKKEY\n";
    }
    CHECK(getRegistryCdKey("") == "FALLBACKKEY"); // Subkey default only when named value absent.
    CHECK(unlink(filename.c_str()) == 0);
    CHECK(getRegistryCdKey("").empty()); // No cached key after file removal.
    CHECK(!posixRegistry::keyExists(RegKeyLocalMachine,
        "SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2142\\InstallDir"));
#endif
    std::printf("registry checks: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
