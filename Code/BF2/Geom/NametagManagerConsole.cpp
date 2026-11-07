// Native nametag console bindings. [INFERRED] companion source placement.
// Availability requires NametagManager; each executor resolves the singleton again.
// Preserve registration order and the host's ConsoleObject/string ABI.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "NametagManager.h"
#include "../Core/StringUtil.h"              // equalsIgnoreCase (sub_10075080)

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Math/Vec3.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace dice {
namespace hfe {
namespace geom {


// Enum stream operators used by the console bindings through ADL.


// the enumerator's name; an unnamed value writes nothing.
// PE 0x100B0F83
std::ostream& operator<<(std::ostream& out, NametagItemIDs id)
{
    switch (id)
    {
    case ICRank:              out << "ICRank"; break;
    case ICRevive:            out << "ICRevive"; break;
    case ICMedic:             out << "ICMedic"; break;
    case ICRepair:            out << "ICRepair"; break;
    case ICAmmo:              out << "ICAmmo"; break;
    case ICVehicleMedic:      out << "ICVehicleMedic"; break;
    case ICVehicleRepair:     out << "ICVehicleRepair"; break;
    case ICVehicleAmmo:       out << "ICVehicleAmmo"; break;
    case ICHealthBar:         out << "ICHealthBar"; break;
    case ICArmorBar:          out << "ICArmorBar"; break;
    case ICAmmoBar:           out << "ICAmmoBar"; break;
    case ICSquadOrder:        out << "ICSquadOrder"; break;
    case ICCommanderOrder:    out << "ICCommanderOrder"; break;
    case ICTarget:            out << "ICTarget"; break;
    case ICObject:            out << "ICObject"; break;
    case ICObjectVehicleDrop: out << "ICObjectVehicleDrop"; break;
    default:                  break;
    }
    return out;
}

namespace {

// The shared head of both readers: one whitespace-delimited token (sub_10001A40), and a ONE-
// character token is first turned into the NametagItemIDs name of its atoi value (0x100B15F3..

std::string readEnumToken(std::istream& in)
{
    std::string token;
    in >> token;
    std::ostringstream name;
    if (token.size() == 1)
    {
        name << static_cast<NametagItemIDs>(::atoi(token.c_str()));
        token = name.str();
    }
    return token;
}

} // anonymous namespace

// 0x100B15BE, 957 B. An unknown name leaves the target unchanged. The test order is the shipped one.
std::istream& operator>>(std::istream& in, NametagItemIDs& id)
{
    const std::string token = readEnumToken(in);
    if      (equalsIgnoreCase(token, "ICRank"))              id = ICRank;
    else if (equalsIgnoreCase(token, "ICRevive"))            id = ICRevive;
    else if (equalsIgnoreCase(token, "ICMedic"))             id = ICMedic;
    else if (equalsIgnoreCase(token, "ICAmmo"))              id = ICAmmo;
    else if (equalsIgnoreCase(token, "ICRepair"))            id = ICRepair;
    else if (equalsIgnoreCase(token, "ICVehicleMedic"))      id = ICVehicleMedic;
    else if (equalsIgnoreCase(token, "ICVehicleRepair"))     id = ICVehicleRepair;
    else if (equalsIgnoreCase(token, "ICVehicleAmmo"))       id = ICVehicleAmmo;
    else if (equalsIgnoreCase(token, "ICHealthBar"))         id = ICHealthBar;
    else if (equalsIgnoreCase(token, "ICArmorBar"))          id = ICArmorBar;
    else if (equalsIgnoreCase(token, "ICAmmoBar"))           id = ICAmmoBar;
    else if (equalsIgnoreCase(token, "ICSquadOrder"))        id = ICSquadOrder;
    else if (equalsIgnoreCase(token, "ICCommanderOrder"))    id = ICCommanderOrder;
    else if (equalsIgnoreCase(token, "ICTarget"))            id = ICTarget;
    else if (equalsIgnoreCase(token, "ICObject"))            id = ICObject;
    else if (equalsIgnoreCase(token, "ICObjectVehicleDrop")) id = ICObjectVehicleDrop;
    return in;
}

// Numeric input is formatted as NametagItemIDs, so numeric TagType input leaves type unchanged.
// PE 0x100B197B
std::istream& operator>>(std::istream& in, TagType& type)
{
    const std::string token = readEnumToken(in);
    if      (equalsIgnoreCase(token, "TTPlayer")) type = TTPlayer;
    else if (equalsIgnoreCase(token, "TTOrder"))  type = TTOrder;
    else if (equalsIgnoreCase(token, "TTObject")) type = TTObject;
    return in;
}

namespace {

const char kNametagsConsoleTag[] = "geom.nametags";

// sub_100B0D0F - classManager->getSingleton("NametagManager"), asked afresh by every executor and
// by the shared availability test 0x100BA4EA. NametagManager derives from IBase first, so the
// downcast adjusts nothing.
NametagManager* nametagSingleton()
{
    return static_cast<NametagManager*>(
        classManager->getSingleton(renddx9abi::HostString("NametagManager")));
}

bool nametagsActive()
{
    return nametagSingleton() != 0;
}

// the method targets
void consoleCreateBar(NametagItemIDs id, int barStart, int barEnd)
{
    nametagSingleton()->createBar(id, barStart, barEnd);
}

void consoleCreateIcon(NametagItemIDs id)
{
    nametagSingleton()->createIcon(id);
}

// PE 0x100B954B (executor body): the path is copied into a local and then again into the by-value
// parameter, both through the std::string copy constructor (IAT 0x101DB198), so embedded NULs
// survive. The console boundary is also where it changes representation (SkyDomeConsole.cpp's toRetail).
void consoleSetTexture(int count, const std::string& path, int x, int y, int width, int height)
{
    const std::string local(path);
    nametagSingleton()->setTexture(count, std::string(local), x, y, width, height);
}

Vec2 consoleGetCullDistance(TagType type)
{
    return nametagSingleton()->getCullDistance(type);
}

void consoleSetCullDistance(TagType type, const Vec2& distance)
{
    const Vec2 local = distance;
    nametagSingleton()->setCullDistance(type, local);
}

// the property setters (the getters are expressions in the bindings)
void setEnemyDotLimit(float v)              { nametagSingleton()->setEnemyDotLimit(v); }
void setMaxCallDistance(float v)            { nametagSingleton()->setMaxCallDistance(v); }
void setVehicleNametagOffset(float v)       { nametagSingleton()->setVehicleNametagOffset(v); }
void setVehicleNametagOffsetFactor(float v) { nametagSingleton()->setVehicleNametagOffsetFactor(v); }
void setMaxFriendlyDistance(float v)        { nametagSingleton()->setMaxFriendlyDistance(v); }
void setEnemyTagDelayTime(double v)         { nametagSingleton()->setEnemyTagDelayTime(v); }
void setEnemyTagFadeInTime(double v)        { nametagSingleton()->setEnemyTagFadeInTime(v); }
void setEnemyTagFadeOutTime(double v)       { nametagSingleton()->setEnemyTagFadeOutTime(v); }
void setDeathFadeOutTime(float v)           { nametagSingleton()->setDeathFadeOutTime(v); }
void setManDownFadeOutTime(float v)         { nametagSingleton()->setManDownFadeOutTime(v); }
void setDeployableDistance(double v)        { nametagSingleton()->setDeployableDistance(v); }

} // anonymous namespace


// Local six-argument static console binding for setTexture: constructor PE 0x100B9448, executor
// PE 0x100B954B (requires six arguments), parser PE 0x100B95B8 (six persistent per-argument values).
// Requires exactly six arguments; only the second argument is a string.

#define NT_CONSOLE_METHOD_6_STATIC_ACTIVE(fnName, target, activeExpr, A0, A1, A2, A3, A4, A5, listTag) \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 6, 6, "void", false)                                                    \
            m_argTypes[0] = #A0;                                                                   \
            m_argTypes[1] = #A1;                                                                   \
            m_argTypes[2] = #A2;                                                                   \
            m_argTypes[3] = #A3;                                                                   \
            m_argTypes[4] = #A4;                                                                   \
            m_argTypes[5] = #A5;                                                                   \
    BF2_CC_TAIL(6, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 6) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2),                            \
               BF2_CC_ARG(A3, 3), BF2_CC_ARG(A4, 4), BF2_CC_ARG(A5, 5));                           \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
        BF2_CC_CONVERT(A3, 4)                                                                      \
        BF2_CC_CONVERT(A4, 5)                                                                      \
        BF2_CC_CONVERT(A5, 6)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// The type spellings below are the shipped ones, read from the ctors' stores into m_argTypes /
// m_returnType: "NametagItemIDs" 0x102115F4, "int" 0x101E3298, "std::string" 0x101E3010 (setTexture
// slot 2 - the spelling the base's raw-string branch keys on), "TagType" 0x102117B8, "Vec2"
// 0x101E6320, "float" 0x101E4270, "double" 0x101E4618, "void" 0x101E345C.
BF2_CONSOLE_OBJECT("nametags", 2, 0)

// PE 0x100B91B9, 0x100B92A9, 0x100B9266: constructor, argument conversion, result formatting.
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(createBar, consoleCreateBar, nametagsActive(),
                                   NametagItemIDs, int, int, kNametagsConsoleTag)
// PE 0x100B9368, 0x100B93FB: constructor, argument conversion.
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(createIcon, consoleCreateIcon, nametagsActive(),
                                   NametagItemIDs, kNametagsConsoleTag)
// PE 0x100B9448, 0x100B954B, 0x100B95B8: constructor, executor, argument conversion.
NT_CONSOLE_METHOD_6_STATIC_ACTIVE(setTexture, consoleSetTexture, nametagsActive(),
                                  int, std::string, int, int, int, int, kNametagsConsoleTag)
// PE 0x100B9745, 0x100B9801, 0x100B97C7: constructor, argument conversion, executor.
BF2_CONSOLE_METHOD_1_RET_ACTIVE(getCullDistance, consoleGetCullDistance, Vec2, nametagsActive(),
                                TagType, kNametagsConsoleTag)
// PE 0x100B984E, 0x100B9919, 0x100B98DF: constructor, argument conversion, executor.
BF2_CONSOLE_METHOD_2_STATIC_ACTIVE(setCullDistance, consoleSetCullDistance, nametagsActive(),
                                   TagType, Vec2, kNametagsConsoleTag)
// PE 0x100B99BE, 0x100B9A2F: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(enemyDotLimit, float, nametagsActive(),
                        nametagSingleton()->getEnemyDotLimit(), setEnemyDotLimit, kNametagsConsoleTag)
// PE 0x100B9AC7, 0x100B9B38: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(maxCallDistance, float, nametagsActive(),
                        nametagSingleton()->getMaxCallDistance(), setMaxCallDistance, kNametagsConsoleTag)
// PE 0x100B9BD0, 0x100B9C41: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(vehicleNametagOffset, float, nametagsActive(),
                        nametagSingleton()->getVehicleNametagOffset(), setVehicleNametagOffset,
                        kNametagsConsoleTag)
// PE 0x100B9CD9, 0x100B9D4A: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(vehicleNametagOffsetFactor, float, nametagsActive(),
                        nametagSingleton()->getVehicleNametagOffsetFactor(),
                        setVehicleNametagOffsetFactor, kNametagsConsoleTag)
// PE 0x100B9DE2, 0x100B9E53: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(maxFriendlyDistance, float, nametagsActive(),
                        nametagSingleton()->getMaxFriendlyDistance(), setMaxFriendlyDistance,
                        kNametagsConsoleTag)
// PE 0x100B9EEB, 0x100B9FBA, 0x100B9F5C: constructor, argument conversion, result formatting.
BF2_CONSOLE_PROPERTY_FN(enemyTagDelayTime, double, nametagsActive(),
                        nametagSingleton()->getEnemyTagDelayTime(), setEnemyTagDelayTime,
                        kNametagsConsoleTag)
// PE 0x100BA053, 0x100BA0C4: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(enemyTagFadeInTime, double, nametagsActive(),
                        nametagSingleton()->getEnemyTagFadeInTime(), setEnemyTagFadeInTime,
                        kNametagsConsoleTag)
// PE 0x100BA15D, 0x100BA1CE: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(enemyTagFadeOutTime, double, nametagsActive(),
                        nametagSingleton()->getEnemyTagFadeOutTime(), setEnemyTagFadeOutTime,
                        kNametagsConsoleTag)
// PE 0x100BA267, 0x100BA2D8: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(deathFadeOutTime, float, nametagsActive(),
                        nametagSingleton()->getDeathFadeOutTime(), setDeathFadeOutTime,
                        kNametagsConsoleTag)
// PE 0x100BA370, 0x100BA3E1: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(manDownFadeOutTime, float, nametagsActive(),
                        nametagSingleton()->getManDownFadeOutTime(), setManDownFadeOutTime,
                        kNametagsConsoleTag)
// PE 0x100BA479, 0x100BA4F6: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(deployableDistance, double, nametagsActive(),
                        nametagSingleton()->getDeployableDistance(), setDeployableDistance,
                        kNametagsConsoleTag)

#if defined(_MSC_VER) && defined(_M_IX86)
#define NAMETAG_CONSOLE_STDCALL __stdcall
#else
#define NAMETAG_CONSOLE_STDCALL   // [CHOSEN, not recovered] stdcall is an x86-only convention
#endif

// PE 0x1000D920. [INFERRED] name and placement: the one-argument float square root (stdcall,
// retn 4: fld the argument, fsqrt, answered in st0). Its only caller is the dynamic initialiser of
// g_inverseSquareRootTwo below (0x101D05D6); placed with that global, which the image keeps
// directly after this TU's createIcon binding object (0x1024F550..0x1024F5F3).
float NAMETAG_CONSOLE_STDCALL squareRootStdcall(float value)
{
    return std::sqrt(value);
}

// [INFERRED] name. 1 / sqrt(2), computed at load (PE 0x101D05CC: fld 2.0, the call, fdivr 1.0,
// fstp 0x1024F5F4); no instruction reads it.
float g_inverseSquareRootTwo = 1.0f / squareRootStdcall(2.0f);

bool registerNametagManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(consoleBindingList(kNametagsConsoleTag));
}

bool unregisterNametagManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kNametagsConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
