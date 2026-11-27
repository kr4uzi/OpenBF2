// [TEMP] Real native declarations; expose manager storage only to inspect rejection cleanup.
#pragma once
#include "bf_game_counter_runtime.h"
#include "../../BF2/AI/AIDLL/AI/AIStrategicEntity/AIStrategicArea.h"
#include "../../BF2/AI/AIDLL/AI/AIStrategicEntity/SAWayPoint.h"
#include "../../BF2/AI/AIDLL/AI/Main/IAIPathfinding.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIDLL/AI/Main/AISettingsInterfaces.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include "../../BF2/AI/AIDLL/AI/AIStrategic/SAI.h"
#include "../../BF2/AI/AIDLL/AI/AIUtils/checkPointer.h"
#include "../../BF2/AI/AIDLL/AI/AIUtils/DebugRendering/AIRenderUtil.h"
#include <algorithm>
#define private public
#include "../../BF2/AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.h"
#undef private
