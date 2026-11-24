// Scene light with attenuation, color, projection, flicker and message-controlled activation.
// Registers with LightManager when available; dedicated images have no renderer manager.

#ifndef GAME_OBJECTS_VISUAL_LIGHTSOURCE_H     // [INFERRED] guard style
#define GAME_OBJECTS_VISUAL_LIGHTSOURCE_H

#include "../Object.h"
#include "../../../Math/Frustum.h"

namespace dice {
namespace hfe {

namespace geom { class LightManager; }

namespace world {

class LightSourceTemplate;

class LightSource : public Object
{
public:
    explicit LightSource(const LightSourceTemplate* objectTemplate);
    virtual ~LightSource();                                            // / 0x82B0550 / 0x82B0580 - the vptrs, ~Object

    virtual unsigned int getClassID() const;                           // CID_LightSource
    virtual void  init();
    virtual void  destroy(bool force);
    virtual bool  makeScript(io::IStream* stream);                     // Object's
    virtual void  handleMessage(TemplateMessage message, IPlayer* player, bool);
    virtual void  handleVisualUpdate(float, float) {}
    virtual void  handleFrameUpdate(float deltaTime);
    virtual float getVisualBoundingRadius() const;
    virtual const Mat4& getAbsoluteTransformation() const;
    virtual void  setAbsoluteTransformation(const Mat4& m);
    virtual void  setRelativeTransformation(const Mat4& m);

    // the 29 own slots, +0x180..+0x1F0
    virtual void  setDirection(const Vec3& direction);                 // normalized in place
    virtual const Vec3& getDirection() const;
    virtual const Vec3& getAbsoluteDirection() const;                  // lazily rotated
    virtual void  setAttenuationRange1(float range);                   // capped at 100 (warning)
    virtual float getAttenuationRange1() const;
    virtual void  setAttenuationRange2(float range);                   // capped at 100
    virtual float getAttenuationRange2() const;
    virtual void  setColor(const Vec3& color);
    virtual const Vec3& getColor() const;
    virtual void  setSpecColor(const Vec3& color);
    virtual const Vec3& getSpecColor() const;
    virtual void  setHDRIntensity(float intensity);
    virtual float getHDRIntensity() const;
    virtual void  setConeAngle1(float degrees);                        // capped at 180
    virtual float getConeAngle1() const;
    virtual float getConeAngleRads1() const;                           // (+0x170 - written by the client's spotlight path)
    virtual void  setConeAngle2(float degrees);                        // capped at 180
    virtual float getConeAngle2() const;
    virtual float getConeAngleRads2() const;                           // (+0x178)
    virtual void  setEnabled(bool enabled);
    virtual bool  getEnabled() const;
    virtual void  setFov(float degrees);                               // capped at 180
    virtual float getFov() const;
    virtual const Vec3& getScale() const;
    virtual void  setScale(const Vec3& scale);
    virtual Frustum* getSpotlightFrustum();
    virtual Vec4 getProjectorMask();                                   // hidden 16-byte result
    virtual void  setAsSkyLight();
    virtual void  copySettings(const LightSource& other);              // [FRONTIER]

    // non-virtual
    void  setAttenuationIntensity(float intensity);                    // clamped 0..1
    float getAttenuationIntensity() const;

protected:
    /* 0x12C */ const LightSourceTemplate* m_template;
    /* 0x130 */ Vec3    m_direction;
    /* 0x13C */ Vec3    m_absoluteDirection;
    /* 0x148 */ float   m_attenuationRange1;
    /* 0x14C */ float   m_attenuationRange2;
    /* 0x150 */ Vec3    m_color;
    /* 0x15C */ Vec3    m_specColor;
    /* 0x168 */ float   m_hdrIntensity;
    /* 0x16C */ float   m_coneAngle1;
    /* 0x170 */ float   m_coneAngleRads1;         // client setter's radians cache
    /* 0x174 */ float   m_coneAngle2;
    /* 0x178 */ float   m_coneAngleRads2;
    /* 0x17C */ bool    m_enabled;
    /* 0x180 */ int     m_frameCounter;           // handleFrameUpdate's "second frame" count for affectingType 1
    /* 0x184 */ float   m_fov;
    /* 0x188 */ Vec3    m_scale;
    /* 0x194 */ float   m_attenuationIntensity;   // ctor 1.0; handleFrameUpdate multiplies both ranges by it
    /* 0x198 */ Frustum m_frustum;
    /* 0x380 */ bool    m_lightDirty;             // setDirection / setScale / setAttenuationIntensity raise it
    /* 0x381 */ bool    m_transformDirty;         // set/relative transformation and a moving parent raise it
    /* 0x382 */ bool    m_absoluteDirectionDirty; // getAbsoluteDirection lowers it
    /* 0x384 */ geom::LightManager* m_lightManager; // renderer-owned singleton, or null
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_VISUAL_LIGHTSOURCE_H
