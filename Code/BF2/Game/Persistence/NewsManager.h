// Path attested by BF2_r.exe 0x5293C4. BF2.exe table 0x8A2E90.
#ifndef GAME_PERSISTENCE_NEWSMANAGER_H
#define GAME_PERSISTENCE_NEWSMANAGER_H
#include "../../Core/IBase.h"
#if !DEDICATED
namespace dice { namespace hfe {
struct PatchDesc;

class INewsManager : public IBase {
public:
    // Forwards the descriptor to the named AutoPatch singleton without taking a reference.
    // [INFERRED] method name. [CHOSEN, not recovered] void return spelling under the
    // unobservable-type exception: PE 0x4A7310 and _r 0x529380 have no callers in
    // either image, and neither materializes a result after AutoPatch +10.
    virtual void checkForPatch(PatchDesc* desc) = 0; // PE +10
    virtual ~INewsManager() = default; // PE +14
};

class NewsManager : public INewsManager {
public:
    NewsManager();
    ~NewsManager() override;
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    void checkForPatch(PatchDesc* desc) override;
private:
    int m_refCount; // PE +04; table +00, total size 08
};
} }
#endif
#endif
