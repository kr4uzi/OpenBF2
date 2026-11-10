// [INFERRED] Client tutorial owner and source placement beside Player.
#ifndef GAME_GAMELOGIC_PLAYERVOHELP_H
#define GAME_GAMELOGIC_PLAYERVOHELP_H

#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include <string>
#include "../../Math/Vec3.h"

namespace dice { namespace hfe {
namespace world { class IObject; class IHelpHudComp; class HelpHudCompTemplate; }
namespace hud { class HudAmmo; }

// BF2.exe 0x5ED220 initializes this state; native STL determines string stride.
// Class and method names are [INFERRED]. No virtual surface is published.
struct PlayerVOHelp
{
    PlayerVOHelp();
    ~PlayerVOHelp() = default; // BF2.exe 0x504720: reverse destruction of 82 strings.
    void enable(bool value);
    void disableMessage(int id, bool value);
    bool messagePlayed(int id) const;
    void markPlayed(int id, bool save);
    void loadPlayed();
    bool tickMessage(int id, float dt);
    void update(float dt);
    bool tryMessage(int id, float dt);

    static void savePlayed(std::string key);
    static bool wasPlayed(std::string key);
    static void playVoice(std::string key);
    static bool nearObject(const Vec3& origin, float radius, unsigned int classId);
    static world::IHelpHudComp* helpComponent(world::IObject* object);
    static bool hasAbilityHud(world::IObject* object);
    static bool helpActive(world::IHelpHudComp* help, int kind);
    static int helpDistance(world::IHelpHudComp* help);
    static const world::HelpHudCompTemplate* helpTemplate(world::IHelpHudComp* help);
    static float helpPrimaryAmmo(hud::HudAmmo* ammo);

    bool played[82];
    bool disabled[82];
    int minimumExperience[82]; // Total played seconds, not score.
    bool messageActive;
    float messageDuration;
    float messageElapsed;
    float nextMessageDelay;
    bool enabled;
    bool onFoot; // Retail +0x1FD: the vehicle template is Soldier.
    std::string labels[82];
    struct Timer { float interval, remaining; } timers[82];
};

} }
#endif
#endif
