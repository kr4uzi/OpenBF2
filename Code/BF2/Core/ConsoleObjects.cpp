// Console binding registration and lookup. [INFERRED] Source placement.

// static init: ELF 0x4872D0 (__tcf_0 0x487330, _GLOBAL__I 0x487360): <iostream>'s Init object only.
#include <iostream>

#include "ConsoleObjects.h"

#include <string.h>     // strncasecmp / _strnicmp - see prefixMatchesNoCase below

namespace dice {
namespace hfe {

namespace {

// [CHOSEN, not recovered] Compares n characters case-insensitively using the host C runtime.
inline bool prefixMatchesNoCase(const char* s, const char* prefix, size_t n)
{
#ifdef _MSC_VER
    return _strnicmp(s, prefix, n) == 0;
#else
    return strncasecmp(s, prefix, n) == 0;
#endif
}

// Concatenates object and method names without a separator; the parser already removed the dot.
// The registry comparator supplies case-insensitive matching.
inline std::string registryKey(const ConsoleObject* object)
{
    return std::string(object->getObjectName()) + std::string(object->getFunctionName());
}

} // anonymous namespace

// Default-constructs the lookup hash, binding map and registration-order list.
// RendDX9.dll copy PE 0x1007B7E0, unreached there (no code or data reference): Hash (stack functor
// byte, 3011 = 0xBC3 buckets) via 0x1007B840, the multimap head (0x1007BAF0; nil byte +0x2D set,
// links to itself, size 0), the list head (0x1007B890, size 0), in member order.
ConsoleObjects::ConsoleObjects()
{
}

// Registers bindings for lookup and ordered enumeration. Duplicate object.method keys are retained.
bool ConsoleObjects::registerConsoleObjects(ConsoleObjectList& objects)
{
    for (ConsoleObjectList::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        ConsoleObject* object = *it;

        m_consoleObjectMap.insert(std::make_pair(registryKey(object), object));
        m_consoleObjects.insert(m_consoleObjects.end(), object);
    }

    return true;   // `mov eax, 1` - no caller in the engine tests it
}

// Removes all lookup entries sharing each binding's key, but only its exact pointer from the ordered list.
// A same-key binding from another module can therefore remain enumerable but cease to be found.
bool ConsoleObjects::unregisterConsoleObjects(ConsoleObjectList& objects)
{
    for (ConsoleObjectList::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        ConsoleObject* object = *it;

        m_consoleObjectMap.erase(registryKey(object));

        // The binary open-codes this scan rather than calling std::find - it walks the sentinel
        // chain comparing each node's data pointer against `object`, stops at the sentinel, and
        // only calls list::erase if it did not reach it.
        ConsoleObjectList::iterator found = m_consoleObjects.begin();
        while (found != m_consoleObjects.end() && *found != object)
            ++found;

        if (found != m_consoleObjects.end())
            m_consoleObjects.erase(found);
    }

    return true;
}

// - `lea eax, [this+24]; ret`.
const ConsoleObjects::ConsoleObjectList& ConsoleObjects::getConsoleObjects() const
{
    return m_consoleObjects;
}

// Returns the first active binding in the key's equal range, or null.
// Inactive bindings stay registered and allow later duplicates to match.
// RendDX9.dll copy PE 0x1007B320 (lower_bound 0x101C0A60, upper_bound 0x1007BA80, vslot +0x2C per
// entry); unreached there (only its dead caller 0x1007B3B0 refers to it).
ConsoleObject* ConsoleObjects::findInMultiMap(const std::string& key)
{
    std::multimap<std::string, ConsoleObject*, NoCaseStringCompare>::iterator it =
        m_consoleObjectMap.lower_bound(key);
    const std::multimap<std::string, ConsoleObject*, NoCaseStringCompare>::iterator end =
        m_consoleObjectMap.upper_bound(key);

    for (; it != end; ++it)
    {
        ConsoleObject* object = it->second;
        if (object->isActive())
            return object;
    }

    return 0;
}

// Resolves a console binding by exact name, then case-insensitive get/set prefixes.
// The fallback accepts property types 1/3 for get and 1/2 for set. This differs from the command
// dispatcher's read/write polarity; preserve those constants until both call paths justify a change.
// RendDX9.dll copy PE 0x1007B3B0 (`retn 8`; _strnicmp, substr(3, size - 3), vslot +0x10 getType),
// unreached there (no code or data reference).
ConsoleObject* ConsoleObjects::getConsoleObject(const std::string& objectName,
                                                const std::string& methodName)
{
    ConsoleObject* object = findInMultiMap(objectName + methodName);

    if (object == 0 && prefixMatchesNoCase(methodName.c_str(), "get", 3))
    {
        // The binary guards this substr with an explicit `size() <= 2 -> __throw_out_of_range`.
        // That branch is unreachable: strncasecmp cannot match "get" over three characters unless
        // the string is at least three long, because a shorter one hits its NUL first. Kept as a
        // note rather than as code.
        const std::string bare = methodName.substr(3);

        object = findInMultiMap(objectName + bare);
        if (object != 0 && object->getType() != 1 && object->getType() != 3)
            object = 0;
    }

    if (object == 0 && prefixMatchesNoCase(methodName.c_str(), "set", 3))
    {
        const std::string bare = methodName.substr(3);

        object = findInMultiMap(objectName + bare);
        if (object != 0 && object->getType() != 1 && object->getType() != 2)
            object = 0;
    }

    return object;
}

// Registers a case-insensitive conversion key. Returns false for an existing key without replacing it.
// RendDX9.dll copy: the wrapper PE 0x1007B310 tail-jumps to the Hash member (Hash::insertUnique,
// cited in Core/Hash.h). Unreached in RendDX9 (no code or data reference).
bool ConsoleObjects::registerTypeConversion(const std::string& from, const std::string& to)
{
    return m_typeConversions.insertUnique(from, to);
}

// typeAsString -

// A lookup with the key as its own default: an unregistered type name is returned unchanged. The
// leading `_InterlockedAdd(&_S_empty_rep_storage.refcount, 1)` in the decompilation is libstdc++
// constructing an empty std::string, i.e. compiler scaffolding, not a member being touched.
// RendDX9.dll copy PE 0x1007B2C0 over the Hash::get copy PE 0x1007BA20; unreached there.
std::string ConsoleObjects::typeAsString(const std::string& type) const
{
    std::string converted;

    if (m_typeConversions.get(type, converted))
        return converted;

    return type;
}

// Engine-owned console registry, constructed before module initialization.
// Its registry entry uses the existing instance rather than asking ClassManager to construct one.
// [CHOSEN, not recovered] placement in this translation unit.
// Side DLLs borrow the host registry through initDll; they must not construct a second instance.
// Their pointer is zero-initialized in DllGlobals.cpp until that handshake completes.
#if !defined(BF2_IMAGE_AIDLL)
namespace {
ConsoleObjects g_mainConsoleObjects;      // ds:0x8AC85C0 - built by static init, atexit-destroyed
}

ConsoleObjects* mainConsoleObjects = &g_mainConsoleObjects;   // ds:0x8990140, .data relocation
#endif // !BF2_IMAGE_AIDLL

} // namespace hfe
} // namespace dice
