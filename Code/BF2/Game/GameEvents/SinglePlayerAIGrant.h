#ifndef GAME_GAMEEVENTS_SINGLEPLAYERAIGRANT_H
#define GAME_GAMEEVENTS_SINGLEPLAYERAIGRANT_H

#include "../IGame.h"
#include "../Main/Game.h"
#include "../../Core/StringUtil.h"
#include "../../Scene/Object/PlayerManager.h"

namespace dice {
namespace hfe {

// [CHOSEN, not recovered] Retail fix "AI asset grants work in single player".
// Uses retail's Singleplayer mode classification with a sole-human listen-host guard.
// SwiffPlayer.dll 0x10053755..0x10053803 identifies sp1/sp2/sp3, case-insensitively;
// BF2.exe 0x5BF759..0x5BF773 distinguishes a non-dedicated game with one human.
inline bool allowsSinglePlayerAIGrant()
{
    if (isDedicated() || !game || !world::playerManager ||
        world::playerManager->getNumberOfHumanPlayers() != 1)
        return false;

    const std::string& mode = game->getCurrentMapGameMode();
    return compareIgnoreCase(mode, "sp1") == 0 || compareIgnoreCase(mode, "sp2") == 0 ||
           compareIgnoreCase(mode, "sp3") == 0;
}

} // namespace hfe
} // namespace dice

#endif
