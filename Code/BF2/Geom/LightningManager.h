// Lightning bolts for the weather system: strike scheduling, the bolt vertex strips, the draw and
// the thunder sounds. WeatherManager decides when and where a strike happens; this class turns the
// request into a bolt that flashes, lights the sky dome and plays its thunder after a delay.
// [INFERRED] file placement and every method name: RendDX9.dll carries no __FILE__ string or symbol
// for this class. The console names the properties (WeatherManagerConsole.cpp).

#ifndef BF2_GEOM_LIGHTNINGMANAGER_H
#define BF2_GEOM_LIGHTNINGMANAGER_H

#ifdef BF2_IMAGE_RENDDX9

#include "../Core/IBase.h"
#include "../Math/Vec3.h"     // Vec2, Vec3, Vec4

#include <cstddef>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

class IBfSound;
class ISound;
namespace world { class IObjectTemplate; }

namespace dx9rend {
class Effect;
class ShaderParameter;
struct EngineTexture;
}

namespace geom {

// The lightning interface, vtable 0x1021E328 (33 slots, all pure but the destructor 0x10156F80).
// IID 0xB801DD06, class id 822323012, singleton "LightningManager" (dword_1024B928).
class ILightningManager : public IBase
{
public:
    virtual ~ILightningManager() {}                                       // +0x10

    // Drops pending and live strikes and clears the sky-dome flash.
    virtual void reset() = 0;                                             // +0x14
    // Advances the lightning clock by `deltaTime` seconds, turns pending requests into strikes,
    // fades the sky-dome flash, rebuilds new bolts and plays due thunder.
    virtual void update(double deltaTime) = 0;                            // +0x18
    // Draws every live strike.
    virtual void render() = 0;                                            // +0x1C
    // Queues a strike at a horizontal offset (x, z) from the camera; update() places it.
    virtual void addLightning(const Vec2& offset) = 0;                    // +0x20

    // Console properties (weatherManager.*). Setters store; getters return the stored value.
    virtual void         setLightningDuration(float seconds) = 0;         // +0x24
    virtual float        getLightningDuration() const = 0;                // +0x28
    virtual void         setLightningScale(const Vec2& scale) = 0;        // +0x2C
    virtual const Vec2&  getLightningScale() const = 0;                   // +0x30
    virtual void         setLightningPertubation(float amount) = 0;       // +0x34
    virtual float        getLightningPertubation() const = 0;             // +0x38
    virtual void         setLightningColor(const Vec4& color) = 0;        // +0x3C
    virtual const Vec4&  getLightningColor() const = 0;                   // +0x40
    virtual void         setLightningBlendOutTime(float seconds) = 0;     // +0x44
    virtual float        getLightningBlendOutTime() const = 0;            // +0x48
    virtual void         setSkyBlendOutTime(float seconds) = 0;           // +0x4C
    virtual float        getSkyBlendOutTime() const = 0;                  // +0x50
    virtual void         setLightningSFXDelay(float seconds) = 0;         // +0x54
    virtual float        getLightningSFXDelay() const = 0;                // +0x58
    // Releases the previous thunder, then looks up the Sound template `name` and creates one
    // sound instance per bolt buffer.
    virtual void         setLightningSFXName(const std::string& name) = 0; // +0x5C
    virtual const std::string& getLightningSFXName() const = 0;           // +0x60
    // Clamped to at most 100 (the bolt strip has 100 segments).
    virtual void         setLightningSegments(int segments) = 0;          // +0x64
    virtual int          getLightningSegments() const = 0;                // +0x68
    virtual void         setLightningGroundPenetration(float depth) = 0;  // +0x6C
    virtual float        getLightningGroundPenetration() const = 0;       // +0x70
    virtual void         setInFadeSegments(float segments) = 0;           // +0x74
    virtual float        getInFadeSegments() const = 0;                   // +0x78
    virtual void         setOutFadeSegments(float segments) = 0;          // +0x7C
    virtual float        getOutFadeSegments() const = 0;                  // +0x80
};

// One live strike, 0x28 bytes on x86 (copied member-wise: 0x10157070 skips the padding).
struct LightningStrike
{
    Vec3          position;      // +0x00  camera-relative x/z; y is the ground height minus
                                 //        lightningGroundPenetration
    double        flashEndTime;  // +0x10  lightning clock + lightningDuration
    double        soundTime;     // +0x18  lightning clock + lightningSFXDelay; the strike is
                                 //        removed when its thunder plays
    bool          built;         // +0x20  the bolt buffer was rebuilt for this strike
    int           buffer;        // +0x24  bolt vertex buffer and thunder sound, 0..2 round robin
};

// LightningManager - 0xD0 bytes, vtable 0x1021E3F0, constructor 0x10156A80, factory 0x100B0BA4.
class LightningManager : public ILightningManager
{
public:
    LightningManager();                                                   // 0x10156A80
    virtual ~LightningManager();                                          // 0x10156B80

    virtual void  addRef();                                               // 0x1000CB20 (folded)
    virtual int   getRef();                                               // 0x101A9A10 (folded)
    virtual int   release();                                              // 0x1015A4B0 (folded)
    virtual void* queryInterface(unsigned int iid) const;                 // 0x10157530

    virtual void reset();                                                 // 0x10156BF0
    virtual void update(double deltaTime);                                // 0x10156DB0
    virtual void render();                                                // 0x10156A30
    virtual void addLightning(const Vec2& offset);                        // 0x10156C50

    virtual void         setLightningDuration(float seconds);             // 0x10155FA0
    virtual float        getLightningDuration() const;                    // 0x10155FB0
    virtual void         setLightningScale(const Vec2& scale);            // 0x101569E0
    virtual const Vec2&  getLightningScale() const;                       // 0x10155FC0
    virtual void         setLightningPertubation(float amount);           // 0x10155FD0
    virtual float        getLightningPertubation() const;                 // 0x10155FE0
    virtual void         setLightningColor(const Vec4& color);            // 0x10156A00
    virtual const Vec4&  getLightningColor() const;                       // 0x10155FF0
    virtual void         setLightningBlendOutTime(float seconds);         // 0x10156000
    virtual float        getLightningBlendOutTime() const;                // 0x10156010
    virtual void         setSkyBlendOutTime(float seconds);               // 0x10156020
    virtual float        getSkyBlendOutTime() const;                      // 0x10156030
    virtual void         setLightningSFXDelay(float seconds);             // 0x10156040
    virtual float        getLightningSFXDelay() const;                    // 0x10156050
    virtual void         setLightningSFXName(const std::string& name);    // 0x101560B0
    virtual const std::string& getLightningSFXName() const;               // 0x10156150
    virtual void         setLightningSegments(int segments);              // 0x10156160
    virtual int          getLightningSegments() const;                    // 0x10156180
    virtual void         setLightningGroundPenetration(float depth);      // 0x10156190
    virtual float        getLightningGroundPenetration() const;           // 0x101561A0
    virtual void         setInFadeSegments(float segments);               // 0x101561B0
    virtual float        getInFadeSegments() const;                       // 0x101561C0
    virtual void         setOutFadeSegments(float segments);              // 0x101561D0
    virtual float        getOutFadeSegments() const;                      // 0x101561E0

    // The engine heap: the factory allocates with engineAlloc and the deleting destructor frees
    // with engineFree (0x101575F0 -> sub_1005F360).
    static void operator delete(void* memory);

private:
    LightningManager(const LightningManager&);
    LightningManager& operator=(const LightningManager&);

    bool initResources();                                                 // 0x101561F0
    void lookupShaderParameters();                                        // 0x10155CB0
    void releaseResources();                                              // 0x10156340
    void releaseSoundEffect();                                            // 0x10156060
    void releaseShaderParameters();                                       // 0x10155D30 [INFERRED] name, unreachable in 1.50
    void releaseTexture();                                                // 0x10156310 [INFERRED] name, unreachable in 1.50
    void processPendingLightning();                                       // 0x10156D40
    void createStrike(const Vec2& offset);                                // 0x10156C60
    void buildBoltBuffer(int buffer);                                     // 0x10155DA0
    void drawStrike(const LightningStrike& strike);                       // 0x10156920
    bool setStrikeShaderConstants(const LightningStrike& strike);         // 0x10156420

    // data, in shipped order
    int                       m_refCount;           // +0x04
    dx9rend::Effect*          m_effect;             // +0x08  "Shaders/Lightning"
    void*                     m_vertexDeclaration;  // +0x0C
    void*                     m_vertexBuffers[3];   // +0x10  202 vertices of 24 bytes each
    dx9rend::ShaderParameter* m_paramWorldViewProj; // +0x1C  WORLDVIEWPROJ
    dx9rend::ShaderParameter* m_paramColor;         // +0x20  LIGHTNINGCOLOR
    dx9rend::ShaderParameter* m_paramTexture;       // +0x24  TEXTURE
    dx9rend::EngineTexture*   m_texture;            // +0x28  common/textures/lightning
    float                     m_duration;           // +0x2C  lightningDuration, 0.15
    float                     m_sfxDelay;           // +0x30  lightningSFXDelay, 5
    std::vector<LightningStrike> m_strikes;         // +0x34
    IBfSound*                 m_sound;              // +0x44  the "Sound" singleton
    std::string               m_sfxName;            // +0x48  lightningSFXName
    world::IObjectTemplate*   m_sfxTemplate;        // +0x64  referenced Sound template
    ISound*                   m_sfxSounds[3];       // +0x68  one thunder per bolt buffer
    Vec2                      m_scale;              // +0x74  lightningScale, (1, 300)
    float                     m_pertubation;        // +0x7C  lightningPertubation, 10
    Vec4                      m_color;              // +0x80  lightningColor, (1, 1, 1, 1)
    float                     m_blendOutTime;       // +0x90  lightningBlendOutTime, 0.1
    float                     m_skyBlendOutTime;    // +0x94  skyBlendOutTime, 0.1
    int                       m_segments;           // +0x98  lightningSegments, 100
    float                     m_groundPenetration;  // +0x9C  lightningGroundPenetration, 0
    float                     m_inFadeSegments;     // +0xA0  inFadeSegments, 0
    float                     m_outFadeSegments;    // +0xA4  outFadeSegments, 0
    std::vector<Vec2>         m_pending;            // +0xA8  addLightning's queue
    double                    m_time;               // +0xB8  the lightning clock
    double                    m_lastTime;           // +0xC0  m_time at the previous update
    int                       m_nextBuffer;         // +0xC8  round-robin bolt buffer index
};

// dword_1024B928. ModuleGeom.cpp owns the definition; the console rows and WeatherManager read it.
extern IBase* g_LightningManager;

// The factory registered under "dice.hfe.geom.LightningManager" (0x100B0BA4).
IBase* __fastcall createLightningManager(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // BF2_GEOM_LIGHTNINGMANAGER_H
