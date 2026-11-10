// [TEMP] [CHOSEN, not recovered] Socket-free browser parsing fixture.
// Synthetic or queried fields exercise the production SDK parser/getters used by ServerBrowser.
// This does not initialize an engine, generate a server snapshot, or join a game.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../BF2/External/GameSpy/serverbrowsing/sb_internal.h"

static int failures;
static int checks;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

static void check_values(SBServer server, double ratio, int count, double difficulty)
{
    check(SBServerGetFloatValueA(server, "bf2_coopbotratio", 0.0) == ratio, "bot ratio");
    check(SBServerGetIntValueA(server, "bf2_coopbotcount", 0) == count, "bot count");
    check(SBServerGetFloatValueA(server, "bf2_coopbotdiff", 0.0) == difficulty, "bot difficulty");
}

int main(int argc, char **argv)
{
    // QR2 full-key server section, without the five-byte response header. No
    // player/team section is requested; the parser returns at that boundary.
    char empty[] = "bf2_coopbotratio\0\0bf2_coopbotcount\0\0bf2_coopbotdiff\0\0";
    char configured[] = "bf2_coopbotratio\0" "50.000000\0"
                        "bf2_coopbotcount\0" "16\0"
                        "bf2_coopbotdiff\0" "50.000000\0";
    char changed[] = "bf2_coopbotratio\0" "30.500000\0"
                     "bf2_coopbotcount\0" "12\0"
                     "bf2_coopbotdiff\0" "70.250000\0";
    char zero[] = "bf2_coopbotratio\0" "0.000000\0"
                  "bf2_coopbotcount\0" "0\0"
                  "bf2_coopbotdiff\0" "0.000000\0";
    SBServer server = SBAllocServer(NULL, 0, 0);
    if (!server) {
        fprintf(stderr, "FAIL: allocate synthetic browser record\n");
        return 1;
    }
    check_values(server, 0.0, 0, 0.0);
    SBServerParseQR2FullKeysSingle(server, empty, (int)sizeof(empty));
    check(strcmp(SBServerGetStringValueA(server, "bf2_coopbotratio", "missing"), "") == 0,
          "an empty published key is present, rather than missing");
    check_values(server, 0.0, 0, 0.0);
    SBServerParseQR2FullKeysSingle(server, configured, (int)sizeof(configured));
    check_values(server, 50.0, 16, 50.0);
    SBServerParseQR2FullKeysSingle(server, changed, (int)sizeof(changed));
    check_values(server, 30.5, 12, 70.25);
    SBServerParseQR2FullKeysSingle(server, zero, (int)sizeof(zero));
    check_values(server, 0.0, 0, 0.0);
    if (argc == 8 && strcmp(argv[1], "--published") == 0) {
        // Partial-query replies contain values in requested key order. Rebuild
        // the full-key section for the production browser parser, preserving
        // the server's exact strings, including any missing publication.
        char published[1024];
        int size = snprintf(published, sizeof(published),
                            "bf2_coopbotratio%c%s%cbf2_coopbotcount%c%s%cbf2_coopbotdiff%c%s%c",
                            0, argv[2], 0, 0, argv[3], 0, 0, argv[4], 0);
        check(size >= 0 && size < (int)sizeof(published), "published fields fit parser input");
        if (size >= 0 && size < (int)sizeof(published)) {
            int i;
            check(argv[2][0] != '\0', "server published ratio");
            check(argv[3][0] != '\0', "server published count");
            check(argv[4][0] != '\0', "server published difficulty");
            for (i = 2; i <= 4; ++i) {
                char *end;
                if (i == 3)
                    (void)strtol(argv[i], &end, 10);
                else
                    (void)strtod(argv[i], &end);
                check(end != argv[i] && *end == '\0', "published field has numeric syntax");
            }
            SBServerParseQR2FullKeysSingle(server, published, size + 1);
            check_values(server, strtod(argv[5], NULL), (int)strtol(argv[6], NULL, 10),
                         strtod(argv[7], NULL));
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: server_info_bots [--published RATIO COUNT DIFFICULTY "
                        "EXPECTED_RATIO EXPECTED_COUNT EXPECTED_DIFFICULTY]\n");
        ++failures;
    }
    SBServerFree(&server);
    SBRefStrHashCleanup(NULL);
    printf("server_info_bots: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
