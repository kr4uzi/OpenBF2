// Path attested by BF2_r.exe 0x529676. BF2.exe table 0x8A2EC0.
#ifndef GAME_PERSISTENCE_PERSISTENCE_H
#define GAME_PERSISTENCE_PERSISTENCE_H
#include "../../Core/IBase.h"
#if !DEDICATED
namespace dice { namespace hfe {

class IPersistence : public IBase {
public:
    // Reports whether ProfileManager has a current profile; opens no connection.
    virtual bool connect() = 0; // PE +10 0x4A7460; _r 0x529650 names connect
    // [INFERRED] name. Both inspected bodies return without any effect.
    virtual void update() = 0; // PE +14 0x83D270; _r 0x5296E0
    virtual ~IPersistence() = default; // PE +18
};

// Initializes the shared statistics SDK identity when constructed.
class Persistence : public IPersistence {
public:
    Persistence();
    ~Persistence() override;
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    bool connect() override;
    void update() override;
private:
    int m_refCount; // PE +04; table +00, total size 08
};
} }
#endif
#endif
