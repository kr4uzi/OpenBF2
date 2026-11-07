// [TEMP] [CHOSEN, not recovered] Native ABI fixture; no content or engine startup.
#include "../BF2/External/BFMeme/NativeSavedDispatch.h"
#include "../BF2/External/BFMeme/Data.h"
#include <new>
#include <type_traits>

using dice::meme::System;
using dice::meme::Data;
using dice::meme::native_dispatch::SavedVirtual;
using dice::meme::native_dispatch::VirtualMember;
static_assert(std::is_same<decltype(&Data::setFloat), bool (Data::*)(System*, float)>::value,
              "fixture must cover the production saved-dispatch signature");

namespace {
struct Receiver
{
    virtual ~Receiver() {}
    virtual bool setFloat(System*, float) = 0;
    System* system = nullptr;
    float value = 0;
    int implementation = 0;
};
struct Old : Receiver
{
    bool setFloat(System* s, float f) override
    { system = s; value = f; implementation = 1; return false; }
};
struct New : Receiver
{
    bool setFloat(System* s, float f) override
    { system = s; value = f; implementation = 2; return true; }
};
struct Source
{
    Receiver*& destination;
    Receiver* replacement;
    float getFloat(System*) { destination = replacement; return 3.25f; }
};

// The converted member pointer has a nonzero adjustment to Receiver's secondary
// address point. The captured entry is itself a thunk back to JoinedOld.
struct Primary { virtual ~Primary() {} virtual void other() {} int padding = 9; };
struct Joined : Primary, Receiver {};
struct JoinedOld : Joined
{
    bool setFloat(System* s, float f) override
    { system = s; value = f; implementation = 3; return false; }
};
struct JoinedNew : Joined
{
    bool setFloat(System* s, float f) override
    { system = s; value = f; implementation = 4; return true; }
};
}

int main(int argc, char**)
{
    if (argc > 1) {
        // Death mode: an invalid selector must report and abort, even in Release.
        VirtualMember::decode(static_cast<bool (Receiver::*)(System*, float)>(nullptr));
        return 99;
    }
    const auto data = VirtualMember::decode(&Data::setFloat);
    if (data.slot() != 20 || data.adjustment != 0) return 1;

    Old old;
    New replacement;
    Receiver* destination = &old;
    Source source{destination, &replacement};
    System* system = reinterpret_cast<System*>(&source); // opaque forwarded token
    SavedVirtual<decltype(&Receiver::setFloat)> saved(destination, &Receiver::setFloat);
    const float value = source.getFloat(system);
    if (saved(destination, system, value) || replacement.implementation != 1 ||
        replacement.system != system || replacement.value != 3.25f || old.implementation != 0) return 2;
    if (!destination->setFloat(system, 7.5f) || replacement.implementation != 2) return 3;
    SavedVirtual<decltype(&Receiver::setFloat)> reverse(&replacement, &Receiver::setFloat);
    if (!reverse(&old, system, 6.75f) || old.implementation != 2 ||
        old.system != system || old.value != 6.75f) return 9;

    // Replace the object at the same address between capture and call. Tables are
    // compiler-owned, so the original table survives destruction of its receiver.
    using Storage = typename std::aligned_union<0, Old, New>::type;
    Storage storage;
    Old* initial = new (&storage) Old;
    SavedVirtual<decltype(&Receiver::setFloat)> inPlace(initial, &Receiver::setFloat);
    initial->~Old();
    New* current = new (&storage) New;
    if (inPlace(current, system, -2.5f) || current->implementation != 1 ||
        current->system != system || current->value != -2.5f) return 4;
    if (!current->setFloat(system, 1) || current->implementation != 2) return 5;
    current->~New();

    using SecondaryMember = bool (Joined::*)(System*, float);
    const auto secondary = static_cast<SecondaryMember>(&Receiver::setFloat);
    const auto decoded = VirtualMember::decode(secondary);
    JoinedOld joinedOld;
    JoinedNew joinedNew;
    const auto expected = reinterpret_cast<char*>(static_cast<Receiver*>(&joinedOld)) -
                          reinterpret_cast<char*>(static_cast<Joined*>(&joinedOld));
    if (expected == 0 || decoded.adjustment != expected) return 6;
    SavedVirtual<SecondaryMember> secondarySaved(&joinedOld, secondary);
    if (secondarySaved(&joinedNew, system, 9.5f) || joinedNew.implementation != 3 ||
        joinedNew.system != system || joinedNew.value != 9.5f || joinedOld.implementation != 0 ||
        joinedNew.padding != 9) return 7;
    if (!(joinedNew.*secondary)(system, 4) || joinedNew.implementation != 4) return 8;
    std::puts("native BFMeme saved dispatch: bool(System*, float), receiver replacement, in-place table replacement, secondary adjustment/thunk passed");
}
