// Sky-dome console properties, available while the singleton exists.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "SkyDome.h"

#include "../../RendDX9Abi/HfeAbi.h"
#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kSkyDomeConsoleTag[] = "geom.skyDome";

// g_SkyDome is published as an IBase* by ModuleGeom.cpp.  SkyDome derives from IBase first, so the
// interface subobject sits at offset zero and this downcast adjusts nothing - the same reason the
// renderer can hand the object straight to its three guarded calls.
inline SkyDome* skyDome()
{
    return static_cast<SkyDome*>(g_SkyDome);
}

// The console boundary copies the object's string for the host console (both are std::string).
inline std::string toConsole(const std::string& value)
{
    return std::string(value.c_str());
}

inline std::string toRetail(const std::string& value)
{
    return std::string(value.c_str());
}

// Small named adapters keep the generated bindings readable.  The retail slot each one used to
// reach by offset is noted beside it.
// PE 0x100B7767: primary +0x18 reads, +0x14 writes.
Vec3 getDomePosition() { return skyDome()->getPosition(); }
void setDomePosition(const Vec3& value) { skyDome()->setPosition(value); }

// PE 0x100B7895: primary +0x20 reads, +0x1C writes.
float getDomeSize() { return skyDome()->getDomeScale(); }
void setDomeSize(float value) { skyDome()->setDomeScale(value); }

float getDomeRotation() { return skyDome()->getDomeRotation(); }                     // +0x28
void setDomeRotation(float value) { skyDome()->setDomeRotation(value); }             // +0x24

std::string getSkyTemplate() { return toConsole(skyDome()->getSkyTemplate()); }      // +0x30
void setSkyTemplate(const std::string& value)                                        // +0x2C
{
    skyDome()->setSkyTemplate(toRetail(value));
}

std::string getFlareTemplate() { return toConsole(skyDome()->getFlareTemplate()); }  // +0x88
void setFlareTemplate(const std::string& value)                                      // +0x84
{
    skyDome()->setFlareTemplate(toRetail(value));
}

// The getter is NOT const in retail and neither is it here: SkyDome::getFlareTexture stores 1
// into the flare's occlusion reference before returning the name, so reading skyDome.flareTexture
// from the console resets the flare to "fully visible".  Calling the real method preserves that.
std::string getFlareTexture() { return toConsole(skyDome()->getFlareTexture()); }    // +0x90
void setFlareTexture(const std::string& value)                                       // +0x8C
{
    skyDome()->setFlareTexture(toRetail(value));
}

// The three direct-field properties.  Retail has no vtable slot for any of them; the class exposes
// typed accessors so this file does not have to know the layout.
// PE 0x100B7F99
Vec3 getFlareDirection() { return skyDome()->flareDirection(); }                     // field +0x1A0
// PE 0x100B7F7B
void setFlareDirection(const Vec3& value) { skyDome()->setFlareDirectionField(value); }

float getFlareFadeAdd() { return skyDome()->flareFadeAdd(); }                        // field +0x16C
void setFlareFadeAdd(float value) { skyDome()->setFlareFadeAddField(value); }

float getFlareFadeMul() { return skyDome()->flareFadeMul(); }                        // field +0x170
void setFlareFadeMul(float value) { skyDome()->setFlareFadeMulField(value); }

std::string getSkyTexture() { return toConsole(skyDome()->getSkyTexture()); }        // +0x38
void setSkyTexture(const std::string& value)                                         // +0x34
{
    skyDome()->setSkyTexture(toRetail(value));
}

bool getHasCloudLayer() { return skyDome()->getHasCloudLayer(); }                    // +0x40
void setHasCloudLayer(bool value) { skyDome()->setHasCloudLayer(value); }            // +0x3C

bool getHasCloudLayer2() { return skyDome()->getHasCloudLayer2(); }                  // +0x48
void setHasCloudLayer2(bool value) { skyDome()->setHasCloudLayer2(value); }          // +0x44

std::string getCloudTemplate() { return toConsole(skyDome()->getCloudTemplate()); }  // +0x50
void setCloudTemplate(const std::string& value)                                      // +0x4C
{
    skyDome()->setCloudTemplate(toRetail(value));
}

std::string getCloudTexture() { return toConsole(skyDome()->getCloudTexture()); }    // +0x58
void setCloudTexture(const std::string& value)                                       // +0x54
{
    skyDome()->setCloudTexture(toRetail(value));
}

std::string getCloudTexture2() { return toConsole(skyDome()->getCloudTexture2()); }  // +0x60
void setCloudTexture2(const std::string& value)                                      // +0x5C
{
    skyDome()->setCloudTexture2(toRetail(value));
}

Vec2 getScrollDirection() { return skyDome()->getScrollDirection(); }                // +0x68
void setScrollDirection(const Vec2& value) { skyDome()->setScrollDirection(value); } // +0x64

Vec2 getScrollDirection2() { return skyDome()->getScrollDirection2(); }              // +0x70
void setScrollDirection2(const Vec2& value) { skyDome()->setScrollDirection2(value); } // +0x6C

Vec2 getFadeCloudsDistances() { return skyDome()->getFadeCloudsDistances(); }        // +0x78
void setFadeCloudsDistances(const Vec2& value)                                       // +0x74
{
    skyDome()->setFadeCloudsDistances(value);
}

Vec2 getCloudLerpFactors() { return skyDome()->getCloudLerpFactors(); }              // +0x80
void setCloudLerpFactors(const Vec2& value) { skyDome()->setCloudLerpFactors(value); } // +0x7C

float getLightingBlend() { return skyDome()->getLightingBlend(); }                   // +0x9C
void setLightingBlend(float value) { skyDome()->setLightingBlend(value); }           // +0x98

Vec3 getLightingColor() { return skyDome()->getLightingColor(); }                    // +0xA4
void setLightingColor(const Vec3& value) { skyDome()->setLightingColor(value); }     // +0xA0

BF2_CONSOLE_OBJECT("skyDome", 2, 0)

// PE 0x100B766F, 0x100B76F4
BF2_CONSOLE_PROPERTY_FN(domePosition, Vec3, skyDome() != 0,
                        getDomePosition(), setDomePosition, kSkyDomeConsoleTag)
// PE 0x100B77D7, 0x100B7848
BF2_CONSOLE_PROPERTY_FN(domeSize, float, skyDome() != 0,
                        getDomeSize(), setDomeSize, kSkyDomeConsoleTag)
// PE 0x100B78DE, 0x100B794F
BF2_CONSOLE_PROPERTY_FN(domeRotation, float, skyDome() != 0,
                        getDomeRotation(), setDomeRotation, kSkyDomeConsoleTag)
// PE 0x100B79E5, 0x100B7ACC, 0x100B7A62, 0x100B877E, 0x100C184A, 0x100B8B79: constructor, argument conversion, executor, result formatting, range pair (shared base body), range strings (shared base body).
BF2_CONSOLE_PROPERTY_FN(skyTemplate, std::string, skyDome() != 0,
                        getSkyTemplate(), setSkyTemplate, kSkyDomeConsoleTag)
// PE 0x100B7B41, 0x100B7C2E, 0x100B7BBE: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(flareTemplate, std::string, skyDome() != 0,
                        getFlareTemplate(), setFlareTemplate, kSkyDomeConsoleTag)
// PE 0x100B7CA3, 0x100B7D90, 0x100B7D20: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(flareTexture, std::string, skyDome() != 0,
                        getFlareTexture(), setFlareTexture, kSkyDomeConsoleTag)
// PE 0x100B7E05, 0x100B7E8A, 0x100B7EFD, 0x100BD635: constructor, argument conversion, executor, result formatting.
BF2_CONSOLE_PROPERTY_FN(flareDirection, Vec3, skyDome() != 0,
                        getFlareDirection(), setFlareDirection, kSkyDomeConsoleTag)
// PE 0x100B7FB9, 0x100B802A: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(flareFadeAdd, float, skyDome() != 0,
                        getFlareFadeAdd(), setFlareFadeAdd, kSkyDomeConsoleTag)
// PE 0x100B80BD, 0x100B812E: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(flareFadeMul, float, skyDome() != 0,
                        getFlareFadeMul(), setFlareFadeMul, kSkyDomeConsoleTag)
// PE 0x100B81C1, 0x100B82A8, 0x100B823E: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(skyTexture, std::string, skyDome() != 0,
                        getSkyTexture(), setSkyTexture, kSkyDomeConsoleTag)
// PE 0x100B831D, 0x100B838E, 0x100B83DB, 0x100BB593: constructor, argument conversion, executor, result formatting.
BF2_CONSOLE_PROPERTY_FN(hasCloudLayer, bool, skyDome() != 0,
                        getHasCloudLayer(), setHasCloudLayer, kSkyDomeConsoleTag)
// PE 0x100B842C, 0x100B849D, 0x100B84EA: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(hasCloudLayer2, bool, skyDome() != 0,
                        getHasCloudLayer2(), setHasCloudLayer2, kSkyDomeConsoleTag)
// Retail 0x10138420 ignores this setter's argument and copies the current skyTemplate string.
// Calling SkyDome::setCloudTemplate, rather than assigning the field here, preserves that shipped
// defect - the class reproduces it, so the binding does not have to know about it.
// PE 0x100B853B, 0x100B8622, 0x100B85B8: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(cloudTemplate, std::string, skyDome() != 0,
                        getCloudTemplate(), setCloudTemplate, kSkyDomeConsoleTag)
// PE 0x100B8697, 0x100B87DA, 0x100B8714: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(cloudTexture, std::string, skyDome() != 0,
                        getCloudTexture(), setCloudTexture, kSkyDomeConsoleTag)
// PE 0x100B884F, 0x100B8936, 0x100B88CC: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(cloudTexture2, std::string, skyDome() != 0,
                        getCloudTexture2(), setCloudTexture2, kSkyDomeConsoleTag)
// PE 0x100B89AB, 0x100B8A2B, 0x100B8A96, 0x100BF69F: constructor, argument conversion, executor, result formatting.
BF2_CONSOLE_PROPERTY_FN(scrollDirection, Vec2, skyDome() != 0,
                        getScrollDirection(), setScrollDirection, kSkyDomeConsoleTag)
// PE 0x100B8AF9, 0x100B8BC5, 0x100B8C30: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(scrollDirection2, Vec2, skyDome() != 0,
                        getScrollDirection2(), setScrollDirection2, kSkyDomeConsoleTag)
// PE 0x100B8C93, 0x100B8D13, 0x100B8D7E: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(fadeCloudsDistances, Vec2, skyDome() != 0,
                        getFadeCloudsDistances(), setFadeCloudsDistances, kSkyDomeConsoleTag)
// PE 0x100B8DE1, 0x100B8E61, 0x100B8ECC: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(cloudLerpFactors, Vec2, skyDome() != 0,
                        getCloudLerpFactors(), setCloudLerpFactors, kSkyDomeConsoleTag)
// PE 0x100B8F32, 0x100B8FA3: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(lightingBlend, float, skyDome() != 0,
                        getLightingBlend(), setLightingBlend, kSkyDomeConsoleTag)
// PE 0x100B903F, 0x100B90D0, 0x100B9143: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(lightingColor, Vec3, skyDome() != 0,
                        getLightingColor(), setLightingColor, kSkyDomeConsoleTag)

} // anonymous namespace

bool registerSkyDomeConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kSkyDomeConsoleTag));
}

bool unregisterSkyDomeConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kSkyDomeConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
