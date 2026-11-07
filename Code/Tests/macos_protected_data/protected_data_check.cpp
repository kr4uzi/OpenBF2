// [TEMP] [CHOSEN, not recovered] Checks the macOS DPAPI counterpart (Platform/macOS/ProtectedData)
// through the production PasswordCodec: round trip, the Profile.con hex form, tampering, a wrong
// key, a missing key file, a foreign (DPAPI-shaped) blob, the key file's permissions and the CD-key
// translation (CdKeyStore). Run it
// with HOME pointing at a scratch directory (build.sh does): it creates the key file there.
// [RETIRE-WHEN: native-protected-data-tests] Fold into the tree's unit-test runner.
#include "../../BF2/Game/PasswordCodec.h"
#include "../../BF2/Game/MatchMaking/Client/CdKeyStore.h"
#include "../../BF2/Platform/macOS/ProtectedData.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace dice::hfe;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main()
{
    const char* home = std::getenv("HOME");
    CHECK(home && std::strstr(home, "/out/"));          // never the user's real home
    if (!home || !std::strstr(home, "/out/"))
        return 1;
    const std::string keyPath = macos::defaultProtectedDataKeyPath();
    unlink(keyPath.c_str());

    // Missing key file: nothing decodes, and decode leaves the text unchanged.
    std::string text("4f42463201aabbccdd");
    CHECK(!PasswordCodec::decode(text) && text == "4f42463201aabbccdd");

    // Encode creates the key; the hex written to Profile.con decodes back to the password.
    std::string hex;
    {
        PasswordCodec codec;
        CHECK(!codec.encode(""));
        CHECK(codec.encode("s3cret pass"));
        std::stringstream out;
        codec.write(out);
        hex = out.str();
    }
    CHECK(hex.size() > 2 * (4 + 1 + 16 + 16 + 32) - 1 && hex.find_first_not_of("0123456789abcdef") == std::string::npos);
    CHECK(hex.compare(0, 10, "4f42463201") == 0);
    std::string decoded(hex);
    CHECK(PasswordCodec::decode(decoded) && decoded == "s3cret pass");

    // Two encodings differ (random IV) and both decode.
    std::string hex2;
    { PasswordCodec codec; codec.encode("s3cret pass"); std::stringstream out; codec.write(out); hex2 = out.str(); }
    CHECK(hex2 != hex);

    // Key file: regular, 64 bytes, mode 0600.
    struct stat st;
    CHECK(stat(keyPath.c_str(), &st) == 0 && st.st_size == 64 && (st.st_mode & 0777) == 0600);

    // A flipped ciphertext nibble fails the MAC and leaves the text unchanged.
    std::string tampered(hex);
    tampered[2 * 30] = tampered[2 * 30] == '0' ? '1' : '0';
    std::string t2(tampered);
    CHECK(!PasswordCodec::decode(t2) && t2 == tampered);

    // A DPAPI-shaped blob (starts 01000000d08c9ddf) is not this format.
    std::string dpapi("01000000d08c9ddf0115d1118c7a00c04fc297eb0100000000000000");
    std::string d2(dpapi);
    CHECK(!PasswordCodec::decode(d2) && d2 == dpapi);

    // The CD key: an unmarked key comes back marked ("x9392" + hex), and the marked form
    // translates back to the key; a marked payload that does not decode is returned verbatim.
    const std::string marked = translateCdKey("ABCD-EFGH-IJKL-MNOP");
    CHECK(marked.compare(0, 5, "x9392") == 0 && marked.size() > 5 + 2 * 64);
    CHECK(translateCdKey(marked) == "ABCD-EFGH-IJKL-MNOP");
    CHECK(translateCdKey("x9392abcdef") == "abcdef");

    // Another key file: the blob does not decode.
    unlink(keyPath.c_str());
    { PasswordCodec codec; codec.encode("other"); }      // makes a fresh key
    std::string d3(hex);
    CHECK(!PasswordCodec::decode(d3) && d3 == hex);

    std::printf(failures ? "protected_data_check: %d failure(s)\n" : "protected_data_check: all passed\n", failures);
    return failures ? 1 : 0;
}
