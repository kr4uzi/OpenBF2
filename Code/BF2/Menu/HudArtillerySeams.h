// [INFERRED] split beside the attested Menu/BF2HudManager.cpp family.
#ifndef MENU_HUDARTILLERYSEAMS_H
#define MENU_HUDARTILLERYSEAMS_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Math/Vec3.h"
namespace dice { namespace hfe { namespace ai {
class IAIEventCourier;
// Artillery request dispatch with team and target position: IAIEventCourier::event_incomingArtillery.
// [INFERRED] helper name; defined in BF2HudManagerAbilities.cpp.
void dispatchArtilleryRequest(IAIEventCourier*, int team, Vec2 target);
} } }
#endif
#endif
