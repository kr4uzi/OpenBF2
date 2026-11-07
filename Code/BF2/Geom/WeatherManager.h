// The storm: while stormEnabled is set, schedules lightning strikes at random intervals and random
// positions around the camera, with occasional double strikes, and drives LightningManager.
// The renderer resets it before each level load, updates it every frame and lets it draw after
// the transparent pass. [INFERRED] file placement and every method name: RendDX9.dll carries no
// __FILE__ string or symbol for the class. The console names the properties.

#ifndef BF2_GEOM_WEATHERMANAGER_H
#define BF2_GEOM_WEATHERMANAGER_H

#ifdef BF2_IMAGE_RENDDX9

#include "../Core/IBase.h"
#include "../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace geom {

class ILightningManager;

// The weather interface, vtable 0x1021E498 (24 slots, all pure but the destructor 0x10157AD0).
// IID 0xB801DD05, class id 822323011, singleton "WeatherManager" (dword_1024B924).
class IWeatherManager : public IBase
{
public:
    virtual ~IWeatherManager() {}                                         // +0x10

    // Resets the lightning and disables the storm (Renderer::handleLevelPreLoad).
    virtual void reset() = 0;                                             // +0x14
    // Advances the storm clock by `deltaTime` seconds (Renderer::update).
    virtual void update(double deltaTime) = 0;                            // +0x18
    // Draws the lightning while the storm is enabled (the renderer's transparent pass).
    virtual void render() = 0;                                            // +0x1C

    // Console properties (weatherManager.*). Setters store; getters return the stored value.
    virtual void        setLightningMinInterval(float seconds) = 0;       // +0x20
    virtual float       getLightningMinInterval() const = 0;              // +0x24
    virtual void        setLightningMaxInterval(float seconds) = 0;       // +0x28
    virtual float       getLightningMaxInterval() const = 0;              // +0x2C
    virtual void        setDoubleStrikeProbability(float probability) = 0; // +0x30
    virtual float       getDoubleStrikeProbability() const = 0;           // +0x34
    virtual void        setDoubleStrikeMinDelay(float seconds) = 0;       // +0x38
    virtual float       getDoubleStrikeMinDelay() const = 0;              // +0x3C
    virtual void        setDoubleStrikeMaxDelay(float seconds) = 0;       // +0x40
    virtual float       getDoubleStrikeMaxDelay() const = 0;              // +0x44
    virtual void        setLightningMinPos(const Vec3& position) = 0;     // +0x48
    virtual const Vec3& getLightningMinPos() const = 0;                   // +0x4C
    virtual void        setLightningMaxPos(const Vec3& position) = 0;     // +0x50
    virtual const Vec3& getLightningMaxPos() const = 0;                   // +0x54
    virtual void        setStormEnabled(bool enabled) = 0;                // +0x58
    virtual bool        getStormEnabled() const = 0;                      // +0x5C
};

// WeatherManager - 0x58 bytes, vtable 0x1021E4F8, constructor 0x10157A00, factory 0x100B0BE6.
class WeatherManager : public IWeatherManager
{
public:
    WeatherManager();                                                     // 0x10157A00
    virtual ~WeatherManager();                                            // 0x10157AD0 (the
                                                                          // interface's body)

    virtual void  addRef();                                               // 0x1000CB20 (folded)
    virtual int   getRef();                                               // 0x101A9A10 (folded)
    virtual int   release();                                              // 0x1015A4B0 (folded)
    virtual void* queryInterface(unsigned int iid) const;                 // 0x10157AA0

    virtual void reset();                                                 // 0x10157680
    virtual void update(double deltaTime);                                // 0x10157960
    virtual void render();                                                // 0x101576A0

    virtual void        setLightningMinInterval(float seconds);           // 0x101576B0
    virtual float       getLightningMinInterval() const;                  // 0x101576C0
    virtual void        setLightningMaxInterval(float seconds);           // 0x101576D0
    virtual float       getLightningMaxInterval() const;                  // 0x101576E0
    virtual void        setDoubleStrikeProbability(float probability);    // 0x10157730
    virtual float       getDoubleStrikeProbability() const;               // 0x10157740
    virtual void        setDoubleStrikeMinDelay(float seconds);           // 0x10157750
    virtual float       getDoubleStrikeMinDelay() const;                  // 0x10157760
    virtual void        setDoubleStrikeMaxDelay(float seconds);           // 0x10157770
    virtual float       getDoubleStrikeMaxDelay() const;                  // 0x10157780
    virtual void        setLightningMinPos(const Vec3& position);         // 0x101579C0
    virtual const Vec3& getLightningMinPos() const;                       // 0x101576F0
    virtual void        setLightningMaxPos(const Vec3& position);         // 0x101579E0
    virtual const Vec3& getLightningMaxPos() const;                       // 0x10157700
    virtual void        setStormEnabled(bool enabled);                    // 0x10157710
    virtual bool        getStormEnabled() const;                          // 0x10157720

    // The engine heap: the factory allocates with engineAlloc and the deleting destructor frees
    // with engineFree (0x10157AD0 -> sub_1005F360).
    static void operator delete(void* memory);

private:
    WeatherManager(const WeatherManager&);
    WeatherManager& operator=(const WeatherManager&);

    void scheduleLightning();                                             // 0x101577A0
    bool acquireLightningManager();                                       // 0x10157630 [INFERRED] name, unreachable in 1.50

    // data, in shipped order
    int                 m_refCount;                 // +0x04
    float               m_lightningMinInterval;     // +0x08  1
    float               m_lightningMaxInterval;     // +0x0C  30
    float               m_doubleStrikeProbability;  // +0x10  0
    float               m_doubleStrikeMinDelay;     // +0x14  0
    float               m_doubleStrikeMaxDelay;     // +0x18  0
    Vec3                m_lightningMinPos;          // +0x1C  (-300, 0, 200)
    Vec3                m_lightningMaxPos;          // +0x28  (-400, 0, 300)
    bool                m_stormEnabled;             // +0x34  false
    int                 m_doubleStrikeCount;        // +0x38  strikes since the last normal one
    double              m_time;                     // +0x40  the storm clock
    double              m_nextStrikeTime;           // +0x48
    ILightningManager*  m_lightningManager;         // +0x50  the "LightningManager" singleton
};

// dword_1024B924. ModuleGeom.cpp owns the definition; the renderer and the console rows read it.
extern IBase* g_WeatherManager;

// The factory registered under "dice.hfe.geom.WeatherManager" (0x100B0BE6).
IBase* __fastcall createWeatherManager(unsigned int iid, IBase* outer);

// The renderer caches the "WeatherManager" singleton as an untyped pointer (Renderer +0x5D4).
// The interface derives from IBase first, so the conversion adjusts nothing.
inline IWeatherManager* asWeatherManager(void* singleton)
{
    return static_cast<IWeatherManager*>(static_cast<IBase*>(singleton));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // BF2_GEOM_WEATHERMANAGER_H
