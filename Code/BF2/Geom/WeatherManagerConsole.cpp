// weatherManager console properties (Operation Harvest's Weather.con). The first twelve rows
// forward to the LightningManager singleton, the last eight to WeatherManager; each row is active
// while its singleton exists, and the executors use it without a second test.
// Retail registers the object from the static initialiser at 0x101D191E (SetConsoleClassDefaults
// "weatherManager", access 2, category 0) and the twenty rows that follow it, binding constructors
// 0x100C00D9..0x100C1631. [INFERRED] file placement.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"

#include "LightningManager.h"
#include "WeatherManager.h"

#include "../../RendDX9Abi/HfeAbi.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kWeatherManagerConsoleTag[] = "geom.weatherManager";

// dword_1024B928 / dword_1024B924, read afresh by every executor. Both interfaces derive from
// IBase first, so the downcasts adjust nothing.
inline ILightningManager* lightning()
{
    return static_cast<ILightningManager*>(g_LightningManager);
}

inline IWeatherManager* weather()
{
    return static_cast<IWeatherManager*>(g_WeatherManager);
}

// The activity tests: 0x100C0260 for the lightning rows, 0x100C0EA3 for the weather rows.
inline bool lightningActive() { return g_LightningManager != 0; }
inline bool weatherActive() { return g_WeatherManager != 0; }

// The executors' targets, with the slot each one calls.
float getLightningDuration() { return lightning()->getLightningDuration(); }                 // +0x28
void setLightningDuration(float v) { lightning()->setLightningDuration(v); }                 // +0x24
Vec2 getLightningScale() { return lightning()->getLightningScale(); }                        // +0x30
void setLightningScale(const Vec2& v) { lightning()->setLightningScale(v); }                 // +0x2C
float getLightningPertubation() { return lightning()->getLightningPertubation(); }           // +0x38
void setLightningPertubation(float v) { lightning()->setLightningPertubation(v); }           // +0x34
Vec4 getLightningColor() { return lightning()->getLightningColor(); }                        // +0x40
void setLightningColor(const Vec4& v) { lightning()->setLightningColor(v); }                 // +0x3C
float getLightningSFXDelay() { return lightning()->getLightningSFXDelay(); }                 // +0x58
void setLightningSFXDelay(float v) { lightning()->setLightningSFXDelay(v); }                 // +0x54
std::string getLightningSFXName() { return lightning()->getLightningSFXName(); }             // +0x60
void setLightningSFXName(const std::string& v) { lightning()->setLightningSFXName(v); }      // +0x5C
int getLightningSegments() { return lightning()->getLightningSegments(); }                   // +0x68
void setLightningSegments(int v) { lightning()->setLightningSegments(v); }                   // +0x64
float getLightningGroundPenetration() { return lightning()->getLightningGroundPenetration(); } // +0x70
void setLightningGroundPenetration(float v) { lightning()->setLightningGroundPenetration(v); } // +0x6C
float getLightningBlendOutTime() { return lightning()->getLightningBlendOutTime(); }         // +0x48
void setLightningBlendOutTime(float v) { lightning()->setLightningBlendOutTime(v); }         // +0x44
float getSkyBlendOutTime() { return lightning()->getSkyBlendOutTime(); }                     // +0x50
void setSkyBlendOutTime(float v) { lightning()->setSkyBlendOutTime(v); }                     // +0x4C
float getInFadeSegments() { return lightning()->getInFadeSegments(); }                       // +0x78
void setInFadeSegments(float v) { lightning()->setInFadeSegments(v); }                       // +0x74
float getOutFadeSegments() { return lightning()->getOutFadeSegments(); }                     // +0x80
void setOutFadeSegments(float v) { lightning()->setOutFadeSegments(v); }                     // +0x7C

float getLightningMinInterval() { return weather()->getLightningMinInterval(); }             // +0x24
void setLightningMinInterval(float v) { weather()->setLightningMinInterval(v); }             // +0x20
float getLightningMaxInterval() { return weather()->getLightningMaxInterval(); }             // +0x2C
void setLightningMaxInterval(float v) { weather()->setLightningMaxInterval(v); }             // +0x28
float getDoubleStrikeProbability() { return weather()->getDoubleStrikeProbability(); }       // +0x34
void setDoubleStrikeProbability(float v) { weather()->setDoubleStrikeProbability(v); }       // +0x30
float getDoubleStrikeMinDelay() { return weather()->getDoubleStrikeMinDelay(); }             // +0x3C
void setDoubleStrikeMinDelay(float v) { weather()->setDoubleStrikeMinDelay(v); }             // +0x38
float getDoubleStrikeMaxDelay() { return weather()->getDoubleStrikeMaxDelay(); }             // +0x44
void setDoubleStrikeMaxDelay(float v) { weather()->setDoubleStrikeMaxDelay(v); }             // +0x40
Vec3 getLightningMinPos() { return weather()->getLightningMinPos(); }                        // +0x4C
void setLightningMinPos(const Vec3& v) { weather()->setLightningMinPos(v); }                 // +0x48
Vec3 getLightningMaxPos() { return weather()->getLightningMaxPos(); }                        // +0x54
void setLightningMaxPos(const Vec3& v) { weather()->setLightningMaxPos(v); }                 // +0x50
bool getStormEnabled() { return weather()->getStormEnabled(); }                              // +0x5C
void setStormEnabled(bool v) { weather()->setStormEnabled(v); }                              // +0x58

BF2_CONSOLE_OBJECT("weatherManager", 2, 0)

// 0x100C00D9 .. 0x100C1631, in the shipped order.
// PE 0x100C014A: argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningDuration, float, lightningActive(),
                        getLightningDuration(), setLightningDuration, kWeatherManagerConsoleTag)
// PE 0x100C01E0, 0x100C026C, 0x100C02D7: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(lightningScale, Vec2, lightningActive(),
                        getLightningScale(), setLightningScale, kWeatherManagerConsoleTag)
// PE 0x100C033A, 0x100C03AB: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningPertubation, float, lightningActive(),
                        getLightningPertubation(), setLightningPertubation,
                        kWeatherManagerConsoleTag)
// PE 0x100C0441, 0x100C04CB, 0x100C0546: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(lightningColor, Vec4, lightningActive(),
                        getLightningColor(), setLightningColor, kWeatherManagerConsoleTag)
// PE 0x100C05A5, 0x100C0616: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningSFXDelay, float, lightningActive(),
                        getLightningSFXDelay(), setLightningSFXDelay, kWeatherManagerConsoleTag)
// PE 0x100C06AC, 0x100C0793, 0x100C0729: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(lightningSFXName, std::string, lightningActive(),
                        getLightningSFXName(), setLightningSFXName, kWeatherManagerConsoleTag)
// PE 0x100C0808, 0x100C0879: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningSegments, int, lightningActive(),
                        getLightningSegments(), setLightningSegments, kWeatherManagerConsoleTag)
// PE 0x100C090C, 0x100C097D: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningGroundPenetration, float, lightningActive(),
                        getLightningGroundPenetration(), setLightningGroundPenetration,
                        kWeatherManagerConsoleTag)
// PE 0x100C0A13, 0x100C0A84: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningBlendOutTime, float, lightningActive(),
                        getLightningBlendOutTime(), setLightningBlendOutTime,
                        kWeatherManagerConsoleTag)
// PE 0x100C0B1A, 0x100C0B8B: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(skyBlendOutTime, float, lightningActive(),
                        getSkyBlendOutTime(), setSkyBlendOutTime, kWeatherManagerConsoleTag)
// PE 0x100C0C21, 0x100C0C92: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(inFadeSegments, float, lightningActive(),
                        getInFadeSegments(), setInFadeSegments, kWeatherManagerConsoleTag)
// PE 0x100C0D28, 0x100C0D99: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(outFadeSegments, float, lightningActive(),
                        getOutFadeSegments(), setOutFadeSegments, kWeatherManagerConsoleTag)
// PE 0x100C0E32, 0x100C0EAF: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningMinInterval, float, weatherActive(),
                        getLightningMinInterval(), setLightningMinInterval,
                        kWeatherManagerConsoleTag)
// PE 0x100C0F45, 0x100C0FB6: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightningMaxInterval, float, weatherActive(),
                        getLightningMaxInterval(), setLightningMaxInterval,
                        kWeatherManagerConsoleTag)
// PE 0x100C104C, 0x100C10BD: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(doubleStrikeProbability, float, weatherActive(),
                        getDoubleStrikeProbability(), setDoubleStrikeProbability,
                        kWeatherManagerConsoleTag)
// PE 0x100C1153, 0x100C11C4: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(doubleStrikeMinDelay, float, weatherActive(),
                        getDoubleStrikeMinDelay(), setDoubleStrikeMinDelay,
                        kWeatherManagerConsoleTag)
// PE 0x100C125A, 0x100C12CB: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(doubleStrikeMaxDelay, float, weatherActive(),
                        getDoubleStrikeMaxDelay(), setDoubleStrikeMaxDelay,
                        kWeatherManagerConsoleTag)
// PE 0x100C1361, 0x100C13E6, 0x100C1459: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(lightningMinPos, Vec3, weatherActive(),
                        getLightningMinPos(), setLightningMinPos, kWeatherManagerConsoleTag)
// PE 0x100C14C9, 0x100C154E, 0x100C15C1: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(lightningMaxPos, Vec3, weatherActive(),
                        getLightningMaxPos(), setLightningMaxPos, kWeatherManagerConsoleTag)
// PE 0x100C1631, 0x100C16A2, 0x100C16EF: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(stormEnabled, bool, weatherActive(),
                        getStormEnabled(), setStormEnabled, kWeatherManagerConsoleTag)

} // anonymous namespace

bool registerWeatherManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kWeatherManagerConsoleTag));
}

bool unregisterWeatherManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kWeatherManagerConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
