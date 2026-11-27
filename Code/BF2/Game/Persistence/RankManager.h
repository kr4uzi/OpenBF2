// [INFERRED] path; BF2.exe factory 0x4103C1 and table 0x8A2E68 attest the class.
#ifndef GAME_PERSISTENCE_RANKMANAGER_H
#define GAME_PERSISTENCE_RANKMANAGER_H
#include "../../Core/IBase.h"
#if !DEDICATED
namespace dice { namespace hfe {

// The shipped rank interface has no operations beyond lifetime and interface lookup.
class IRankManager : public IBase {
public:
    virtual ~IRankManager() = default; // PE +10
};

class RankManager : public IRankManager {
public:
    RankManager();
    ~RankManager() override;
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
