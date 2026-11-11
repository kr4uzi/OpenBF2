// Path: [INFERRED] from the namespace map - dice::hfe::world -> Game/Objects/.
// Complete HelpHud template and runtime-component ABI recovered from BF2.exe 1.50
// and cross-checked against the symbolized Linux image.  Proven retail allocations
// are PE 0x1DC and ELF 0x5C; the modern rebuild STL intentionally has neither size.

#ifndef GAME_OBJECTS_HELPHUDCOMPTEMPLATE_H
#define GAME_OBJECTS_HELPHUDCOMPTEMPLATE_H

#include <string>

#include "CompTemplateInterfaces.h"
#include "IObjectComponent.h"
#include "../../IO/Console/IConsoleSaveable.h"

namespace dice {
namespace hfe {
struct PlayerVOHelp;
namespace world {

// Exact field order.  The close-to group is three strings followed by three sounds;
// it is not laid out as three interleaved string/sound pairs.
struct HelpHudInfo
{
    std::string helpStringKey;
    std::string helpSoundKey;
    std::string altHelpStringKey;
    std::string altHelpSoundKey;
    std::string closeToHelpStringKeyNeutral;
    std::string closeToHelpStringKeyFriendly;
    std::string closeToHelpStringKeyEnemy;
    std::string closeToHelpSoundKeyNeutral;
    std::string closeToHelpSoundKeyFriendly;
    std::string closeToHelpSoundKeyEnemy;
    std::string lowArmorHelpStringKey;
    std::string lowArmorHelpSoundKey;
    std::string lowAmmoHelpStringKey;
    std::string lowAmmoHelpSoundKey;
};

class IHelpHudComp : public IObjectComponent
{
public:
    virtual HelpHudInfo* getHudInfo() = 0;
    virtual void setIsActive(bool active) = 0;
    virtual void setAltIsActive(bool active) = 0;
    virtual void setLowArmorIsActive(bool active) = 0;
    virtual void setLowAmmoIsActive(bool active) = 0;
    virtual void setCloseToIsActive(int referenceTeam, int otherTeam, bool active) = 0;
    virtual bool getIsCloseToActive(int referenceTeam, int otherTeam) = 0;
    // [CHOSEN source spelling] The bodies prove an address return.  Reference versus
    // pointer is not encoded; const references retain the established HUD caller idiom.
    virtual const std::string& getCloseToHelpStringKey(int referenceTeam, int otherTeam) = 0;
    virtual const std::string& getCloseToHelpSoundKey(int referenceTeam, int otherTeam) = 0;

protected:
    ~IHelpHudComp() {}
};

class HelpHudCompTemplate;

class HelpHudComp : public IHelpHudComp
{
public:
    explicit HelpHudComp(HelpHudCompTemplate* componentTemplate);

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int id) const;
    virtual unsigned int getInterfaceID() const;
    virtual void init(IObject* object);
    virtual void setObject(IObject* object);
    virtual IObject* getObject() const;
    virtual bool hasUpdate() const;
    virtual void handleUpdate(float deltaTime);
    virtual HelpHudInfo* getHudInfo();
    virtual void setIsActive(bool active);
    virtual void setAltIsActive(bool active);
    virtual void setLowArmorIsActive(bool active);
    virtual void setLowAmmoIsActive(bool active);
    virtual void setCloseToIsActive(int referenceTeam, int otherTeam, bool active);
    virtual bool getIsCloseToActive(int referenceTeam, int otherTeam);
    virtual const std::string& getCloseToHelpStringKey(int referenceTeam, int otherTeam);
    virtual const std::string& getCloseToHelpSoundKey(int referenceTeam, int otherTeam);
    virtual ~HelpHudComp();

private:
    int m_refCount;
    HelpHudCompTemplate* m_componentTemplate;
    IObject* m_object;
};

// Accessor declaration order is vtable order, which deliberately differs from HelpHudInfo:
// low-armor/low-ammo precede close-to, and their Sound accessors precede String accessors.
class HelpHudCompTemplate : public IHelpHudCompTemplate, public io::IConsoleSaveable
{
public:
    HelpHudCompTemplate();

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int id) const;
    virtual unsigned int getClassID() const;
    virtual unsigned int getInterfaceID();
    virtual IObjectComponent* createComponent();
    virtual void objectCreated(IObject* object);
    virtual void objectDestroyed(IObject* object);
    virtual void setTemplate(IObjectTemplate* objectTemplate);
    virtual void* queryImplementation(unsigned int id);
    virtual void setHelpStringKey(std::string key);
    virtual std::string getHelpStringKey();
    virtual void setHelpSoundKey(std::string key);
    virtual std::string getHelpSoundKey();
    virtual void setAltHelpStringKey(std::string key);
    virtual std::string getAltHelpStringKey();
    virtual void setAltHelpSoundKey(std::string key);
    virtual std::string getAltHelpSoundKey();
    virtual void setLowArmorHelpSoundKey(std::string key);
    virtual std::string getLowArmorHelpSoundKey();
    virtual void setLowArmorHelpStringKey(std::string key);
    virtual std::string getLowArmorHelpStringKey();
    virtual void setLowAmmoHelpSoundKey(std::string key);
    virtual std::string getLowAmmoHelpSoundKey();
    virtual void setLowAmmoHelpStringKey(std::string key);
    virtual std::string getLowAmmoHelpStringKey();
    virtual void setCloseToHelpStringKeyNeutral(std::string key);
    virtual std::string getCloseToHelpStringKeyNeutral();
    virtual void setCloseToHelpSoundKeyNeutral(std::string key);
    virtual std::string getCloseToHelpSoundKeyNeutral();
    virtual void setCloseToHelpStringKeyFriendly(std::string key);
    virtual std::string getCloseToHelpStringKeyFriendly();
    virtual void setCloseToHelpSoundKeyFriendly(std::string key);
    virtual std::string getCloseToHelpSoundKeyFriendly();
    virtual void setCloseToHelpStringKeyEnemy(std::string key);
    virtual std::string getCloseToHelpStringKeyEnemy();
    virtual void setCloseToHelpSoundKeyEnemy(std::string key);
    virtual std::string getCloseToHelpSoundKeyEnemy();
    virtual void setCloseToDistance(int distance);
    virtual int getCloseToDistance();
    virtual ~HelpHudCompTemplate();
    virtual bool makeScript(io::IStream* stream);

private:
    friend class HelpHudComp;
    friend struct ::dice::hfe::PlayerVOHelp;

    int m_refCount;
    bool m_firstComponentPending;
    HelpHudInfo m_helpHudInfo;

    bool m_isActive;
    bool m_altIsActive;
    bool m_lowArmorIsActive;
    bool m_lowAmmoIsActive;
    bool m_closeToNeutralIsActive;
    bool m_closeToFriendlyIsActive;
    bool m_closeToEnemyIsActive;
    unsigned char m_padding4F;
    int m_closeToDistance;

    // Real std::strings, proven by ctor/dtor coverage.  Their semantic use remains
    // unresolved because none of the recovered HelpHud methods references them.
    std::string m_unresolvedString54;
    std::string m_unresolvedString58;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_HELPHUDCOMPTEMPLATE_H
