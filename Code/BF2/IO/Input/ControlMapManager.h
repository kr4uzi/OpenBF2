// Indexes control maps by ID and name and tracks the active map. Header split [INFERRED].
// Name hashing ignores case but equality remains case-sensitive.
// applyActiveControlMap copies tuning values, not bindings, into defaultPlayerInputControlMap;
// the global manager and default map must exist before it is called.
// The virtual destructor follows the interface methods. ELF x86 instances occupy 40 bytes.

#ifndef IO_INPUT_CONTROLMAPMANAGER_H            // [INFERRED] guard style, per Core/IBase.h
#define IO_INPUT_CONTROLMAPMANAGER_H

#include <map>
#include <string>

#include "Input.h"
#include "../../Core/IBase.h"
#include "../../Core/Hash.h"

namespace dice {
namespace hfe {
namespace io {

// Owns maps indexed by id and name, assigns ids and applies active-map tuning.
class ControlMapManager : public IControlMapManager
{
public:
    ControlMapManager();

    // IBase quartet, +0x00..+0x0C
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;       // 0x8499660  - _ZNK..., const

    // IControlMapManager, +0x10..+0x40. order is frozen - see IO/Input/Input.h.
    virtual void         update(float deltaTime);               // 0x8497470,   90   recovered

    // virtual create, not the class factory - see the collision note in Input.h and the
    // integrator note at the bottom of the .cpp.
    virtual IControlMap* create(const std::string& name);       // 0x84978A0         [CHOSEN-STUB]

    virtual IControlMap* getControlMap(const std::string& name) const;  // 0x84974D0 [CHOSEN-STUB]
    virtual IControlMap* getControlMap(int id) const;           // 0x84975A0,   62   recovered
    virtual void         deleteControlMap(const std::string& name);     // 0x84975E0 [CHOSEN-STUB]
    virtual void         deleteControlMap(int id);              // 0x8497610,   46   recovered
    virtual void         deleteControlMap(const IControlMap* map);      // 0x8497640 [CHOSEN-STUB]
    virtual void         setActive(const std::string& name);    // 0x84977A0,   46   recovered
    virtual void         setActive(const IControlMap* map);     // 0x84977D0,   27   recovered
    virtual IControlMap* getActive() const;                     // 0x8497790,   11   recovered
    virtual int          mapNextInput(IControlMap* map, int id, bool a, bool b, bool c, bool d,
                                      std::string& out);        // 0x8497AC0, 5133   recovered
    virtual float        getRepeatRate(int id, bool b);         // 0x8498EE0,  103   recovered
    virtual void         applyActive();                         // 0x84977F0,  164   recovered

    // +0x44 / +0x48
    virtual ~ControlMapManager();

    // The factory ClassManager::createInstance calls. Overloads with the virtual create() above -
    // that collision is dice's and is preserved.
    // _ZN4dice3hfe2io17ControlMapManager6createEjPNS0_5IBaseE.
    static IBase* create(unsigned int iid, IBase* outer);

    // True when input id may share its binding with another input (the shipped tables).
    bool allowDuplicate(int id, bool playerMap);                // 0x8498F50,  101   recovered

private:
    /* 0x04 */ int                          m_refCount;
    /* 0x08 */ std::map<int, IControlMap*>  m_byId;
    /* 0x14 */ int                          m_nextId;       // [UNRESOLVED] name - ctor 0
    /* 0x18 */ Hash<std::string, const IControlMap*, StringHashFunc> m_byName;
    /* 0x24 */ IControlMap*                 m_active;
};

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_INPUT_CONTROLMAPMANAGER_H
