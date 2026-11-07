// Path:     [INFERRED] beside the attested weapon-component families.
// Contents: server-reachable DefaultZoom runtime and complete template leaf recovered from both
// shipped x86 ports, including the client-side WeaponHud notification in changeZoom.

#ifndef GAME_OBJECTS_WEAPONS_ZOOMCOMPONENTS_DEFAULTZOOMCOMP_H
#define GAME_OBJECTS_WEAPONS_ZOOMCOMPONENTS_DEFAULTZOOMCOMP_H

#include <vector>

#include "ZoomComp.h"

namespace dice {
namespace hfe {
namespace world {

class DefaultZoomComp;

class DefaultZoomCompTemplate : public ZoomCompTemplate
{
public:
    DefaultZoomCompTemplate();

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual unsigned int getClassID() const;
    virtual IObjectComponent* createComponent();
    virtual void* queryImplementation(unsigned int classId);
    virtual bool makeScript(io::IStream* stream);
    virtual ~DefaultZoomCompTemplate();

    void addZoomFactor(float value);
    void setZoomDelay(float value);
    void setChangeFovDelay(float value);

    float getZoomDelay() const { return m_zoomDelay; }
    int getZoomDelayInTicks() const { return m_zoomDelayInTicks; }
    void setZoomLod(int value) { m_zoomLod = value; }
    int getZoomLod() const { return m_zoomLod; }
    void setZoomOutAfterFire(bool value) { m_zoomOutAfterFire = value; }
    bool getZoomOutAfterFire() const { return m_zoomOutAfterFire; }
    float getChangeFovDelay() const { return m_changeFovDelay; }
    int getChangeFovDelayInTicks() const { return m_changeFovDelayInTicks; }
    void setMouseSpeedMod(float value) { m_mouseSpeedMod = value; }
    float getMouseSpeedMod() const { return m_mouseSpeedMod; }
    void setDisableMuzzleWhenZoomed(bool value) { m_disableMuzzleWhenZoomed = value; }
    bool getDisableMuzzleWhenZoomed() const { return m_disableMuzzleWhenZoomed; }
    const std::vector<float>& getZoomFactors() const { return m_zoomFactors; }

private:
    /* ELF +0x10 */ int m_refCount;
    /* ELF +0x14 */ std::vector<float> m_zoomFactors;
    /* ELF +0x20 */ float m_zoomDelay;
    /* ELF +0x24 */ int m_zoomDelayInTicks;
    /* ELF +0x28 */ int m_zoomLod;
    /* ELF +0x2C */ bool m_zoomOutAfterFire;
    /* ELF +0x30 */ float m_changeFovDelay;
    /* ELF +0x34 */ int m_changeFovDelayInTicks;
    /* ELF +0x38 */ float m_mouseSpeedMod;
    /* ELF +0x3C */ bool m_disableMuzzleWhenZoomed;
};

// Shipped size is 0x34 in both ports.  The leaf owns a second reference count at +0x18; the
// ZoomComp/WeaponComp count at +0x04 is initialized but deliberately not used by these overrides.
class DefaultZoomComp : public ZoomComp
{
public:
    explicit DefaultZoomComp(DefaultZoomCompTemplate* objectTemplate);
    virtual ~DefaultZoomComp();

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual unsigned int getInterfaceID() const;

    virtual void updateComponent(float, unsigned int);
    virtual bool requestFire(bool, bool);
    virtual void postFire(WeaponFireEvent&);
    virtual void fireComplete(unsigned int);
    virtual void reload(ReloadState);
    virtual bool busy();
    virtual void reset();
    virtual bool getSignalToggle();
    virtual void toggle(TemplateMessage);
    virtual void changeTarget(int);
    virtual void setCameraNr(int);
    virtual int getCameraNr() const;
    virtual void changeViewMode();
    virtual bool allowEffects() const;

    // These three append to DefaultZoomComp's primary table in this exact order.  The last two
    // also override the typed IWeaponZoomComp defaults on the secondary face.
    virtual void setZoomLod(int lod);
    virtual float getMouseSpeedMod();
    virtual float getZoomFactor(unsigned int index) const;

private:
    void changeZoom(int camera, bool immediate);
    int getZoomLod() const;
    bool is1P() const;

    /* +0x18 */ int m_refCount;
    /* +0x1C */ int m_currentZoom;
    /* +0x20 */ unsigned int m_changeTick;
    /* +0x24 */ bool m_pendingZoomLod;
    /* +0x25 */ bool m_signalToggle;
    /* +0x26 */ bool m_pendingFov;
                  // +0x27 padding deliberately untouched
    /* +0x28 */ int m_dword28;
    /* +0x2C */ bool m_waitingForBusy;
                  // +0x2D..0x2F padding deliberately untouched
    /* +0x30 */ DefaultZoomCompTemplate* m_defaultTemplate;
};

#if defined(_MSC_VER) && defined(_M_IX86)
static_assert(sizeof(DefaultZoomComp) == 0x34, "DefaultZoomComp x86 ABI drift");
#endif

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_WEAPONS_ZOOMCOMPONENTS_DEFAULTZOOMCOMP_H
