// [TEMP] [CHOSEN, not recovered] Query protocol index checks, compiled for each image.
#include "../BF2/Game/MatchMaking/Server/ServerQueryCache.h"

using namespace dice::hfe;

static_assert(SQK_coopBotRatio == 41, "GameSpy key 79 must retain cache slot 41");
static_assert(SQK_coopBotCount == 42, "GameSpy key 80 must retain cache slot 42");
static_assert(SQK_coopBotDifficulty == 43, "GameSpy key 81 must retain cache slot 43");
static_assert(SQK_noVehicles == 44, "The following query field must keep its index");
static_assert(NUM_SERVER_KEYS == 45, "Bot publication must not grow the query cache");
