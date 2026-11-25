#include "Persistence.h"
#include "../ProfileManager.h"
#include "../MatchMaking/Server/MatchMakingServer.h"
#include "../../Core/Ids.h"
#include "../../External/GameSpy/gstats/gstats.h"
#include <cstring>
#if !DEDICATED
namespace dice { namespace hfe {
extern IBase* g_profileManager;
namespace gamespy { const char* getSecretKey(); }
Persistence::Persistence() : m_refCount(1) // BF2.exe 0x4A74D0
{
    // PE 0x972C6C -> gcd_gamename (0x9EB990), then 0x59BA80 -> gcd_secret_key (0x9EBA90).
    std::strcpy(gcd_gamename, gamespy::g_gameName);
    std::strcpy(gcd_secret_key, gamespy::getSecretKey());
}
Persistence::~Persistence() {} // BF2.exe deleting destructor 0x4A74A0
IBase* Persistence::create(unsigned int iid, IBase* outer) // BF2.exe 0x4104BD
{
    if (outer) return 0;
    Persistence* object = new Persistence;
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void Persistence::addRef() { ++m_refCount; } // BF2.exe 0x6D0970
int Persistence::getRef() { return m_refCount; } // BF2.exe 0x68AED0
int Persistence::release() // BF2.exe 0x4A7480; virtual destructor at +18
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* Persistence::queryInterface(unsigned int iid) const // BF2.exe 0x4A7430
{
    return iid == IID_IBase || iid == IID_IPersistence ? const_cast<Persistence*>(this) : 0;
}
bool Persistence::connect() // BF2.exe 0x4A7460
{
    return static_cast<IProfileManager*>(g_profileManager)->getCurrentProfile() != 0;
}
void Persistence::update() {} // BF2.exe 0x83D270: ret
} }
#endif
