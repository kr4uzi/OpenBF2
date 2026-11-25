#ifndef GAME_GAMEEVENTS_SINGLEPLAYERAIGRANT_H
#define GAME_GAMEEVENTS_SINGLEPLAYERAIGRANT_H

#include "../IGame.h"
#include "../../Core/StringUtil.h"

namespace dice {
namespace hfe {

// [CHOSEN, not recovered] Retail fix "AI asset grants work in single player".
// Uses retail's Singleplayer mode classification on any host, regardless of human count.
// SwiffPlayer.dll 0x10053755..0x10053844 identifies sp1/sp2/sp3, case-insensitively.
inline bool allowsSinglePlayerAIGrant()
{
    if (!game)
        return false;

    const std::string& mode = game->getCurrentMapGameMode();
    return compareIgnoreCase(mode, "sp1") == 0 || compareIgnoreCase(mode, "sp2") == 0 ||
           compareIgnoreCase(mode, "sp3") == 0;
}

} // namespace hfe
} // namespace dice

#endif
