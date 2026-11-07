// Path:     [INFERRED] - see ConsoleObjectBase.h header block.
// Contents: recovered from bf2.i64 (ELF, symbolised), 0x83be630 - 0x83beab6.

// static init: ELF 0x4863B0 (__tcf_0 0x486410, _GLOBAL__I 0x486440): <iostream>'s Init object only.
#include <iostream>

#include "ConsoleObjectBase.h"
#include "ConsoleObjectMacro.h"   // consoleBindingList / the three default accessors
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

namespace dice {
namespace hfe {

// Converts typed arguments or stores raw string pointers, executes the binding and formats its result.
// setArgFromString uses one-based indices; argument storage belongs to the generated binding.
// ELF 0x486320; engine 0x83BE630. Slots +0x68, +0x7C, +0x60, +0x64 are attested in both images.
// RendDX9.dll copy PE 0x1007BFD0 (`retn 0Ch`, hidden result first; the MSVC table's slots +0x64,
// +0x78, +0x5C, +0x60). The trace block below is the [TEMP] harness, not part of the copy.
std::string ConsoleObjectBaseImpl::execute(std::string* args, int argCount)
{
    // [TEMP] [CHOSEN, not recovered] BF2_TRACE_CONSOLE logs each invoked binding.
    // [RETIRE-WHEN: hud-console-invokers] Remove after all HUD console invokers are implemented and validated.
    {
        static int traceArmed = -1;
        if (traceArmed < 0) traceArmed = std::getenv("BF2_TRACE_CONSOLE") ? 1 : 0;
        if (traceArmed) {
            std::fprintf(stderr, "[CONSOLE] %s.%s\n",
                         m_objectName ? m_objectName : "?",
                         m_functionName ? m_functionName : "?");
            std::fflush(stderr);
        }
    }

    m_argCount = argCount;

    for (int i = 0; i < argCount; ++i)
    {
        if (m_argNeedsConversion[i])
            setArgFromString(i + 1, args[i]);   // vtable +0x68, one-based
        else
            m_args[i] = &args[i];               // vtable not involved - keep the raw string
    }

    // ELF83BE66D..83BE679 and clientPE4C455D..4C4563 both pass true.
    // The earlier false was a reconstruction error, not a platform difference.
    checkObjectRange(true);
    executeObjectMethod();                      // vtable +0x60   calls the real C++ function
    return getReturnValueAsString();            // vtable +0x64   formats the result back to text
}

// The metadata accessors are all plain field reads; see the offsets in ConsoleObjectBase.h.
// Two are worth spelling out because they are not simple reads:

// Not a stored flag - type 1 is the set/get property kind.
// cmp dword ptr [this+4], 1 ; setz al
bool ConsoleObjectBaseImpl::isSetGet() const
{
    return m_type == 1;
}

// Public accessors forward to the corresponding generated binding hooks.
bool ConsoleObjectBaseImpl::isActive() const
{
    return isObjectActive();                    // ELF 0x83be820 -> virtual call through vtbl +0
}

// Base hooks provide the original no-op/default behavior; generated subclasses supply conversions
// and execution for their concrete signatures.

} // namespace hfe
} // namespace dice

namespace dice {
namespace hfe {

// Metadata accessors and default hooks for generated bindings; empty defaults match the original base.

void*         ConsoleObjectBaseImpl::getObject() const       { return 0; }
const char*   ConsoleObjectBaseImpl::getObjectName() const   { return m_objectName; }  // -> +0x08
const char*   ConsoleObjectBaseImpl::getFunctionName() const { return m_functionName; }// -> +0x0C
int           ConsoleObjectBaseImpl::getType() const         { return m_type; }        // -> +0x04
int           ConsoleObjectBaseImpl::getAccess() const       { return m_access; }      // -> +0x10
unsigned char ConsoleObjectBaseImpl::getCategory() const     { return m_category; }    // -> +0x14
const char*   ConsoleObjectBaseImpl::getReturnType() const   { return m_returnType; }  // -> +0x4C
int           ConsoleObjectBaseImpl::getMinNoArgs() const    { return m_minNoArgs; }   // -> +0x18
int           ConsoleObjectBaseImpl::getMaxNoArgs() const    { return m_maxNoArgs; }   // -> +0x1C
bool          ConsoleObjectBaseImpl::hasReturnValue() const  { return m_hasReturnValue; }
bool          ConsoleObjectBaseImpl::isDynamic() const       { return m_isDynamic; }

// -> +0x50 + index*4. no bounds check in the original - the array is ten wide and the caller is
// trusted to stay inside it. execute() iterates its supplied count without clamping;
// ordinary console callers perform the command's arity check before dispatch.
// PE 0x10051D60 (the RendDX9.dll copy: mov eax, [ecx + 4*eax + 0x50]; retn 4).
const char* ConsoleObjectBaseImpl::getArgType(int index) const { return m_argTypes[index]; }

// Range access forwards to the generated object's virtual hooks.
// Range endpoints are pointers to mutable members. Preserve hook slot order without swapping the forwarder targets.
bool ConsoleObjectBaseImpl::hasRange() const { return objectHasRange(); }
// PE 0x10051DB0 (RendDX9.dll copy: tail call through vtable +0x6C, the result slot passed straight through).
std::pair<const void*, const void*> ConsoleObjectBaseImpl::getRange() const
{
    return getObjectRange();
}
// PE 0x10051E80 (RendDX9.dll copy: forwards through vtable +0x74).
std::pair<std::string, std::string> ConsoleObjectBaseImpl::getRangeAsString() const
{
    return getObjectRangeAsString();            // sret passed straight through
}
// PE 0x10051DD0 (RendDX9.dll copy: forwards through vtable +0x70).
std::string ConsoleObjectBaseImpl::getDeltaAsString() const   { return getObjectDeltaAsString(); }
bool        ConsoleObjectBaseImpl::hasBundledType() const     { return objectHasBundledType(); }
// PE 0x10051E20 (RendDX9.dll copy: forwards through vtable +0x80).
std::string ConsoleObjectBaseImpl::getTypeDescription() const { return getObjectTypeDescription(); }

// The eight object-level hooks. Stubs here by design - this is the half the macro overrides, and
// the split is the whole point of the class: the base knows about the binding, the generated
// subclass knows the types.
bool        ConsoleObjectBaseImpl::isObjectActive() const          { return false; }
void        ConsoleObjectBaseImpl::executeObjectMethod()           { }
std::string ConsoleObjectBaseImpl::getReturnValueAsString() const  { return std::string(); }
void        ConsoleObjectBaseImpl::setArgFromString(int, const std::string&) { }   // empty, 5 bytes
bool        ConsoleObjectBaseImpl::objectHasRange() const          { return false; }
// PE 0x100525C0 (RendDX9.dll copy, the vtable +0x6C default of the renderer bindings: stores two zero words
// through its result pointer; retn 4).
std::pair<const void*, const void*> ConsoleObjectBaseImpl::getObjectRange() const
{
    return std::pair<const void*, const void*>(0, 0); // zeroes the two pointer out-fields
}
// PE 0x10051E40 (RendDX9.dll copy), BF2.exe 0x410C61 and ELF 0x83BE930 (ConsoleObjectBaseImpl::getObjectDeltaAsString):
// every image's default builds the literal "No Delta" (BF2.exe .rdata 0x881164, ELF .rodata 0x88D0C70); 2703 BF2.exe
// binding vtables point at 0x410C61. The earlier empty-string default was a reconstruction error.
std::string ConsoleObjectBaseImpl::getObjectDeltaAsString() const  { return std::string("No Delta"); }
std::pair<std::string, std::string> ConsoleObjectBaseImpl::getObjectRangeAsString() const
{
    return std::pair<std::string, std::string>(); // two empty strings into the sret
}
void        ConsoleObjectBaseImpl::checkObjectRange(bool)          { }
bool        ConsoleObjectBaseImpl::objectHasBundledType() const    { return false; }
// PE 0x10051E60 (RendDX9.dll copy, vtable +0x80 default: constructs an empty string).
std::string ConsoleObjectBaseImpl::getObjectTypeDescription() const { return std::string(); }

ConsoleObjectBaseImpl::~ConsoleObjectBaseImpl() { }

// Empty interface destructor, required by static generated bindings during process teardown.
ConsoleObject::~ConsoleObject() { }

// Defaults for subsequent generated bindings: object name, access and category.
// Only the first character of the copied name is lowercased; names must fit the 32-byte allocation.
// The RendDX9 image defines its own copy (Code/RendDX9Abi/NativeConsoleObjects.cpp, PE 0x1007BF60).
#if !defined(BF2_IMAGE_RENDDX9)
namespace {
const char*   g_defaultNameLower = "";
unsigned int  g_defaultAccess    = 0;
unsigned char g_defaultCategory  = 0;
}

const char*   consoleDefaultName()     { return g_defaultNameLower; }
unsigned int  consoleDefaultAccess()   { return g_defaultAccess; }
unsigned char consoleDefaultCategory() { return g_defaultCategory; }

SetConsoleClassDefaults::SetConsoleClassDefaults(const char* objectName, unsigned int access,
                                                 unsigned char category)
{
    char* lowered = new char[0x20];              // the original's exact allocation
    int i = 0;
    for (; objectName[i] && i < 0x1F; ++i)
        lowered[i] = objectName[i];
    lowered[i] = '\0';
    if (lowered[0] >= 'A' && lowered[0] <= 'Z')  // first character only
        lowered[0] = static_cast<char>(lowered[0] - 'A' + 'a');

    g_defaultNameLower = lowered;                // leaked, as in the original
    g_defaultAccess    = access;
    g_defaultCategory  = category;
}
#endif // !BF2_IMAGE_RENDDX9

// One list per translation-unit tag. Function-local static: the generated constructors run during
// static initialisation and this must already exist when the first of them fires.
ConsoleObjectList& consoleBindingList(const char* tag)
{
    static std::map<std::string, ConsoleObjectList> lists;
    return lists[tag];
}

} // namespace hfe
} // namespace dice
