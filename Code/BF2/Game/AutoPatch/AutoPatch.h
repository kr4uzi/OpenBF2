// Path attested by BF2_r.exe 0x52951D. BF2.exe table 0x8A2EA8.
#ifndef GAME_AUTOPATCH_AUTOPATCH_H
#define GAME_AUTOPATCH_AUTOPATCH_H
#include "../../Core/IBase.h"
#if !DEDICATED
namespace dice { namespace hfe {

// BF2_r.exe 0x529545 names PatchDesc. No inspected body accesses its contents.
struct PatchDesc;

class IAutoPatch : public IBase {
public:
    // Always refuses in retail, including for a null descriptor; does not modify it.
    virtual bool checkForPatch(PatchDesc* desc) = 0; // PE +10 0x455400; _r 0x529500
    virtual ~IAutoPatch() = default; // PE +14
};

class AutoPatch : public IAutoPatch {
public:
    AutoPatch();
    ~AutoPatch() override;
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    bool checkForPatch(PatchDesc* desc) override;
private:
    int m_refCount; // PE +04; table +00, total size 08
};
} }
#endif
#endif
