// [INFERRED] path; BF2.exe factory 0x410400 and table 0x8A2E7C attest the class.
#ifndef GAME_PERSISTENCE_CLANMANAGER_H
#define GAME_PERSISTENCE_CLANMANAGER_H
#include "../../Core/IBase.h"
#if !DEDICATED
namespace dice { namespace hfe {

// The shipped clan interface has no operations beyond lifetime and interface lookup.
class IClanManager : public IBase {
public:
    virtual ~IClanManager() = default; // PE +10
};

class ClanManager : public IClanManager {
public:
    ClanManager();
    ~ClanManager() override;
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
private:
    int m_refCount; // PE +04; table +00, total size 08
};
} }
#endif
#endif
