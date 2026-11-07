// Native undergrowth console bindings, available while the system singleton exists.
// These ConsoleObject instances must not cross the VC7.1 host boundary.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "UndergrowthSystem.h"

#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kUndergrowthConsoleTag[] = "geom.undergrowth";

// The console metadata spells this 32-bit unsigned type "uint".
typedef unsigned int uint;

// g_UndergrowthSystem is published as an IBase*; UndergrowthSystem derives from IBase alone, so the
// downcast adjusts nothing.
inline UndergrowthSystem* undergrowth()
{
    return static_cast<UndergrowthSystem*>(g_UndergrowthSystem);
}

void undergrowthReload() { undergrowth()->reload(); }                                // +0x18

void setEnabled(bool value) { undergrowth()->setEnabled(value); }                    // +0x30
void setDynamicShadowsEnable(bool value) { undergrowth()->setDynamicShadowsEnable(value); } // +0x38
void setSimpleShaderEnable(bool value) { undergrowth()->setSimpleShaderEnable(value); }     // +0x40
void setZPassPatchCount(unsigned int value) { undergrowth()->setZPassPatchCount(value); }   // +0x48
void setTest(unsigned int value) { undergrowth()->setTestValue(value); }             // field +0x08
void setTest2(float value) { undergrowth()->setTest2Value(value); }                  // field +0x0C
void setViewDistanceScale(float value) { undergrowth()->setViewDistanceScale(value); } // +0x68

// sub_1000ED40 as the row inlines the call: fcmovnb against the high bound, then fcmovb against the
// low one - an unordered (NaN) value takes the LOW bound. See UndergrowthSystem.cpp's clampRange.
void setViewDistanceFade(float value)                                                // field +0x58C
{
    float v = value;
    if (v >= 1.0f)
        v = 1.0f;
    if (!(v >= 0.0f))
        v = 0.0f;
    undergrowth()->setViewDistanceFadeField(v);
}

BF2_CONSOLE_OBJECT("undergrowth", 2, 0)

// [INFERRED] local name for the retail load binding.
class UndergrowthLoadBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100C4981
    UndergrowthLoadBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "load";
        m_type = 0;
        m_minNoArgs = m_maxNoArgs = 1;
        m_returnType = "void";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argNeedsConversion[0] = 0;
        consoleBindingList(kUndergrowthConsoleTag).push_back(this);
    }

    virtual bool isObjectActive() const { return undergrowth() != 0; }

    // PE 0x100C49FE
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1)
            return;
        const std::string levelPath(BF2_CC_ARG(std::string, 0));
        m_hasReturnValue = false;
        undergrowth()->load(levelPath);
    }

    // PE 0x100C4A3D
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
    }

    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};

UndergrowthLoadBinding s_undergrowthLoadBinding;

// PE 0x100C4AB2: constructor.
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(reload, undergrowthReload, undergrowth() != 0,
                                   kUndergrowthConsoleTag)
// PE 0x100C4B1F, 0x100C4B90, 0x100C4BDD: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(enabled, bool, undergrowth() != 0,
                        undergrowth()->getEnabled(), setEnabled, kUndergrowthConsoleTag)
// PE 0x100C4C2E, 0x100C4C9F, 0x100C4CEC: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(dynamicShadowsEnable, bool, undergrowth() != 0,
                        undergrowth()->getDynamicShadowsEnable(), setDynamicShadowsEnable,
                        kUndergrowthConsoleTag)
// PE 0x100C4D5D, 0x100C4DCE, 0x100C4E1B: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(simpleShaderEnable, bool, undergrowth() != 0,
                        undergrowth()->getSimpleShaderEnable(), setSimpleShaderEnable,
                        kUndergrowthConsoleTag)
// PE 0x100C4E6C, 0x100C4EDD: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(zPassPatchCount, unsigned int, undergrowth() != 0,
                        undergrowth()->getZPassPatchCount(), setZPassPatchCount,
                        kUndergrowthConsoleTag)
// PE 0x100C4F70, 0x100C4FE1: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(test, uint, undergrowth() != 0,
                        undergrowth()->testValue(), setTest, kUndergrowthConsoleTag)
// PE 0x100C506B, 0x100C50DC: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(test2, float, undergrowth() != 0,
                        undergrowth()->test2Value(), setTest2, kUndergrowthConsoleTag)
// PE 0x100C5169, 0x100C51DA: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(viewDistanceScale, float, undergrowth() != 0,
                        undergrowth()->getViewDistanceScale(), setViewDistanceScale,
                        kUndergrowthConsoleTag)
// PE 0x100C5270, 0x100C52E1: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(viewDistanceFade, float, undergrowth() != 0,
                        undergrowth()->viewDistanceFade(), setViewDistanceFade,
                        kUndergrowthConsoleTag)

} // anonymous namespace

bool registerUndergrowthConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kUndergrowthConsoleTag));
}

bool unregisterUndergrowthConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kUndergrowthConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
