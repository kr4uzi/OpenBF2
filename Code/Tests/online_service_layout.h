// [TEMP] Layout checks shared by the native runtime and the MSVC x86 compile probe.
#pragma once
#include <cstddef>
#define private public
#include "../BF2/Game/Persistence/RankManager.h"
#include "../BF2/Game/Persistence/ClanManager.h"
#include "../BF2/Game/Persistence/NewsManager.h"
#include "../BF2/Game/AutoPatch/AutoPatch.h"
#include "../BF2/Game/Persistence/Persistence.h"
#undef private

// BF2.exe factories allocate 08; constructors write only the table and +04 counter.
// [CHOSEN, not recovered] The native layout follows the compiler's pointer width.
#define CHECK_ONLINE_LAYOUT(Type) \
    static_assert(sizeof(dice::hfe::Type) == (sizeof(void*) == 4 ? 8 : 16), #Type " size"); \
    static_assert(offsetof(dice::hfe::Type, m_refCount) == sizeof(void*), #Type " counter")
CHECK_ONLINE_LAYOUT(RankManager);
CHECK_ONLINE_LAYOUT(ClanManager);
CHECK_ONLINE_LAYOUT(NewsManager);
CHECK_ONLINE_LAYOUT(AutoPatch);
CHECK_ONLINE_LAYOUT(Persistence);
#undef CHECK_ONLINE_LAYOUT
