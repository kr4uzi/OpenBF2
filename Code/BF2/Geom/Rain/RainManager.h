// Camera-following rain: 216 particle cells (a 6 x 6 x 6 grid) centred on the camera, drawn with
// Shaders/Rain as point sprites, streak lines or debug cell boxes. Each particle belongs to one
// cell and one of 16 wind deviation groups; the shader adds the cell position and the group's
// deviation to the particle's cell-local position. The renderer resets it before each level load,
// updates it every frame and draws it after the weather in the transparent pass.
// [INFERRED] header placement beside the attested Geom/Rain/RainManager.cpp, and every method
// name: RendDX9.dll has no symbol or console string for the class, and no shipped image calls
// any slot past +0x1C, so rain stays disabled in a 1.50 session.

#ifndef BF2_GEOM_RAIN_RAINMANAGER_H
#define BF2_GEOM_RAIN_RAINMANAGER_H

#ifdef BF2_IMAGE_RENDDX9

#include "../../Core/IBase.h"
#include "../../Math/Vec3.h"     // Vec2, Vec3, Vec4

#include <string>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
class Effect;
class ShaderParameter;
struct EngineTexture;
}

namespace geom {

// The rain interface, vtable 0x1021E5A8 (45 slots, all pure but the destructor 0x10157B00).
// IID 0xB801DD07, class id 822323013, singleton "RainManager" (dword_1024B92C).
class IRainManager : public IBase
{
public:
    virtual ~IRainManager() {}                                            // +0x10

    // Drops every particle and disables the rain (Renderer::handleLevelPreLoad).
    virtual void reset() = 0;                                             // +0x14
    // Moves the cell grid to the camera and, while enabled, advances the rain by `deltaTime`
    // seconds (Renderer::update).
    virtual void update(double deltaTime) = 0;                            // +0x18
    // Draws the rain while enabled (the renderer's transparent pass).
    virtual void render() = 0;                                            // +0x1C

    // Enabling scatters the particles over the cells, rolls new deviation groups and rebuilds
    // the vertex buffer for the current render mode.
    virtual void        setEnabled(bool enabled) = 0;                     // +0x20
    virtual bool        getEnabled() const = 0;                           // +0x24
    // Releases the current texture; a non-empty name loads the new one.
    virtual void        setTexture(const std::string& name) = 0;          // +0x28
    // Recreates the vertex buffer for `count` particles and resizes the particle list; a failed
    // buffer creation frees the list instead.
    virtual void        setParticleCount(unsigned int count) = 0;         // +0x2C
    virtual unsigned int getParticleCount() const = 0;                    // +0x30
    // Places the grid explicitly and stops update() from following the camera.
    virtual void        setCellCenter(const Vec4& center) = 0;            // +0x34
    virtual const Vec4& getCellCenter() const = 0;                        // +0x38
    // Added to the camera position when the grid follows the camera.
    virtual void        setCameraOffset(const Vec3& offset) = 0;          // +0x3C
    virtual const Vec3& getCameraOffset() const = 0;                      // +0x40
    // Half the size of the rain volume; one cell spans 0.4 times it on each axis.
    virtual void        setCellExtents(const Vec3& extents) = 0;          // +0x44
    virtual const Vec3& getCellExtents() const = 0;                       // +0x48
    virtual void        setPointScale(const Vec3& scale) = 0;             // +0x4C (POINTSCALE)
    virtual const Vec3& getPointScale() const = 0;                        // +0x50
    virtual void        setParticleColor(const Vec4& color) = 0;          // +0x54 (PARTICLECOLOR)
    virtual const Vec4& getParticleColor() const = 0;                     // +0x58
    virtual void        setParticleSize(float size) = 0;                  // +0x5C (PARTICLESIZE)
    virtual float       getParticleSize() const = 0;                      // +0x60
    virtual void        setParticleMaxSize(float size) = 0;               // +0x64 (PARTICLEMAXSIZE)
    virtual float       getParticleMaxSize() const = 0;                   // +0x68
    // Stored normalized.
    virtual void        setFallDirection(const Vec3& direction) = 0;      // +0x6C
    virtual const Vec3& getFallDirection() const = 0;                     // +0x70
    virtual void        setFallSpeed(float speed) = 0;                    // +0x74
    virtual float       getFallSpeed() const = 0;                         // +0x78
    // The fraction of the extents where the fade-out starts (FADEOUTRANGE / FADEOUTDELTA).
    virtual void        setFadeOutFactor(float factor) = 0;               // +0x7C
    virtual float       getFadeOutFactor() const = 0;                     // +0x80
    // Rolls new deviation groups with the new amplitude.
    virtual void        setDeviationAmplitude(const Vec3& amplitude) = 0; // +0x84
    virtual const Vec3& getDeviationAmplitude() const = 0;                // +0x88
    // Degrees per second of the two deviation angles.
    virtual void        setDeviationSpeed(const Vec2& speed) = 0;         // +0x8C
    virtual const Vec2& getDeviationSpeed() const = 0;                    // +0x90
    virtual void        setCellCenterFrozen(bool frozen) = 0;             // +0x94
    virtual bool        getCellCenterFrozen() const = 0;                  // +0x98
    // true selects the cell boxes (mode 2), false the points (mode 0).
    virtual void        setDrawCells(bool draw) = 0;                      // +0x9C
    virtual bool        getDrawCells() const = 0;                         // +0xA0
    // true selects the streak lines (mode 1), false the points (mode 0).
    virtual void        setDrawLines(bool draw) = 0;                      // +0xA4
    virtual bool        getDrawLines() const = 0;                         // +0xA8
    // The length of a streak line along the fall direction.
    virtual void        setLineLength(float length) = 0;                  // +0xAC
    virtual float       getLineLength() const = 0;                        // +0xB0
};

// One particle, 20 bytes on x86: its position inside its cell, the cell and the deviation group.
struct RainParticle
{
    // [CHOSEN, not recovered] The shipped resize value (split tail 0x10159F43) zeroes only the
    // position and leaves cell and group as stack garbage, which buildPointBuffer would use as an
    // index into m_cellPrimitiveCounts. They are zeroed here.
    RainParticle() : cell(0), group(0) { position.x = position.y = position.z = 0.0f; }

    Vec3 position;      // +0x00
    int  cell;          // +0x0C  0..215
    int  group;         // +0x10  0..15, the deviation group
};

// One deviation group, 20 bytes: two angles in degrees and the per-axis amplitude.
struct RainDeviation
{
    float angle[2];     // +0x00
    Vec3  amplitude;    // +0x08  per axis: a random fraction of the deviation amplitude
};

// RainManager - 0x1408 bytes, vtable 0x1021E6F0, constructor 0x10159D30, factory 0x100B0C25.
class RainManager : public IRainManager
{
public:
    enum { kCellCount = 216, kDeviationCount = 16, kCellsPerDraw = 32 };

    RainManager();                                                        // 0x10159D30
    virtual ~RainManager();                                               // 0x10159EB0 / 0x1015A580

    virtual void  addRef();                                               // 0x1000CB20 (folded)
    virtual int   getRef();                                               // 0x101A9A10 (folded)
    virtual int   release();                                              // 0x1015A4B0 (folded)
    virtual void* queryInterface(unsigned int iid) const;                 // 0x1015A480

    virtual void reset();                                                 // 0x10157B30
    virtual void update(double deltaTime);                                // 0x10158DA0
    virtual void render();                                                // 0x10159030

    virtual void        setEnabled(bool enabled);                         // 0x10159D00
    virtual bool        getEnabled() const;                               // 0x10157F70
    virtual void        setTexture(const std::string& name);              // 0x101581B0
    virtual void        setParticleCount(unsigned int count);             // 0x10159F00
    virtual unsigned int getParticleCount() const;                        // 0x101582A0
    virtual void        setCellCenter(const Vec4& center);                // 0x10159240
    virtual const Vec4& getCellCenter() const;                            // 0x10158080
    virtual void        setCameraOffset(const Vec3& offset);              // 0x10159220
    virtual const Vec3& getCameraOffset() const;                          // 0x10158070
    virtual void        setCellExtents(const Vec3& extents);              // 0x10159140
    virtual const Vec3& getCellExtents() const;                           // 0x10157F80
    virtual void        setPointScale(const Vec3& scale);                 // 0x101591C0
    virtual const Vec3& getPointScale() const;                            // 0x10158050
    virtual void        setParticleColor(const Vec4& color);              // 0x10159170
    virtual const Vec4& getParticleColor() const;                         // 0x10157F90
    virtual void        setParticleSize(float size);                      // 0x10157FA0
    virtual float       getParticleSize() const;                          // 0x10157FB0
    virtual void        setParticleMaxSize(float size);                   // 0x101580B0
    virtual float       getParticleMaxSize() const;                       // 0x101580C0
    virtual void        setFallDirection(const Vec3& direction);          // 0x101591F0
    virtual const Vec3& getFallDirection() const;                         // 0x10158060
    virtual void        setFallSpeed(float speed);                        // 0x10157FC0
    virtual float       getFallSpeed() const;                             // 0x10157FD0
    virtual void        setFadeOutFactor(float factor);                   // 0x10157FE0
    virtual float       getFadeOutFactor() const;                         // 0x10157FF0
    virtual void        setDeviationAmplitude(const Vec3& amplitude);     // 0x10158B50
    virtual const Vec3& getDeviationAmplitude() const;                    // 0x10157DA0
    virtual void        setDeviationSpeed(const Vec2& speed);             // 0x101591A0
    virtual const Vec2& getDeviationSpeed() const;                        // 0x10158000
    virtual void        setCellCenterFrozen(bool frozen);                 // 0x10158010
    virtual bool        getCellCenterFrozen() const;                      // 0x10158020
    virtual void        setDrawCells(bool draw);                          // 0x10159CC0
    virtual bool        getDrawCells() const;                             // 0x10158030
    virtual void        setDrawLines(bool draw);                          // 0x10159CE0
    virtual bool        getDrawLines() const;                             // 0x10158040
    virtual void        setLineLength(float length);                      // 0x10158090
    virtual float       getLineLength() const;                            // 0x101580A0

    // The engine heap: the factory allocates with engineAlloc and the deleting destructor frees
    // with engineFree (0x1015A580 -> sub_1005F360).
    static void operator delete(void* memory);

private:
    RainManager(const RainManager&);
    RainManager& operator=(const RainManager&);

    bool initResources();                                                 // 0x101580D0
    void lookupShaderParameters();                                        // 0x10157B40
    void releaseShaderParameters();                                       // 0x10157CF0
    void releaseResources();                                              // 0x10158140
    bool createVertexBuffer(unsigned int count);                          // 0x10158220
    Vec3 getCellMin() const;                                              // 0x101582D0
    Vec3 getCellMax() const;                                              // 0x10158330
    Vec3 randomPointInBox(const Vec3& min, const Vec3& max) const;        // 0x10158390
    void initParticle(RainParticle& particle, int cell);                  // 0x10158490
    void initParticles();                                                 // 0x10159270
    void initDeviations();                                                // 0x10157DB0
    void updateDeviations();                                              // 0x10157E70
    void moveCells();                                                     // 0x10158580
    void wrapCells();                                                     // 0x10158A00
    void setCellPositions(int first, int count);                          // 0x10157F40
    bool setShaderConstants();                                            // 0x10158E30
    void setRenderMode(int mode);                                         // 0x10159BC0
    void buildPointBuffer();                                              // 0x10159960
    void buildLineBuffer();                                               // 0x10159A50
    void buildCellBuffer();                                               // 0x10158B80

    // data, in shipped order
    int                       m_refCount;                 // +0x04
    dx9rend::Effect*          m_effect;                   // +0x08  "Shaders/Rain"
    void*                     m_vertexDeclaration;        // +0x0C
    void*                     m_vertexBuffer;             // +0x10  16-byte vertices
    unsigned int              m_vertexBufferCount;        // +0x14
    dx9rend::EngineTexture*   m_texture;                  // +0x18
    dx9rend::ShaderParameter* m_paramWorldViewProj;       // +0x1C  WORLDVIEWPROJ
    dx9rend::ShaderParameter* m_paramParticleColor;       // +0x20  PARTICLECOLOR
    dx9rend::ShaderParameter* m_paramTexture;             // +0x24  TEXTURE
    dx9rend::ShaderParameter* m_paramCameraPos;           // +0x28  CAMERAPOS
    dx9rend::ShaderParameter* m_paramFadeOutRange;        // +0x2C  FADEOUTRANGE
    dx9rend::ShaderParameter* m_paramFadeOutDelta;        // +0x30  FADEOUTDELTA
    dx9rend::ShaderParameter* m_paramCellPositions;       // +0x34  CELLPOSITIONS
    dx9rend::ShaderParameter* m_paramDeviationGroups;     // +0x38  DEVIATIONGROUPS
    dx9rend::ShaderParameter* m_paramParticleSize;        // +0x3C  PARTICLESIZE
    dx9rend::ShaderParameter* m_paramParticleMaxSize;     // +0x40  PARTICLEMAXSIZE
    dx9rend::ShaderParameter* m_paramPointScale;          // +0x44  POINTSCALE
    double                    m_time;                     // +0x48  the rain clock
    double                    m_frameTime;                // +0x50  the last update's deltaTime;
                                                          //        written before every read
    bool                      m_enabled;                  // +0x58
    bool                      m_cellCenterFrozen;         // +0x59
    int                       m_renderMode;               // +0x5C  0 points, 1 lines, 2 cells
    Vec4                      m_cellCenter;               // +0x60  CAMERAPOS
    Vec3                      m_cameraOffset;             // +0x70
    Vec3                      m_cellExtents;              // +0x7C  (40, 20, 40)
    Vec3                      m_pointScale;               // +0x88  (1, 0, 0)
    Vec4                      m_particleColor;            // +0x94  (1, 1, 1, 0.5)
    float                     m_particleSize;             // +0xA4  8
    float                     m_particleMaxSize;          // +0xA8  12
    Vec3                      m_fallDirection;            // +0xAC  (0, -1, 0)
    float                     m_fallSpeed;                // +0xB8  3.5
    float                     m_fadeOutFactor;            // +0xBC  0.875
    Vec4                      m_cells[kCellCount];        // +0xC0  CELLPOSITIONS
    int                       m_cellPrimitiveCounts[kCellCount]; // +0xE40
    float                     m_lineLength;               // +0x11A0 3
    Vec2                      m_deviationSpeed;           // +0x11A4 (0, 0)
    Vec3                      m_deviationAmplitude;       // +0x11AC (0, 0, 0)
    RainDeviation             m_deviations[kDeviationCount];      // +0x11B8
    Vec4                      m_deviationOffsets[kDeviationCount]; // +0x12F8 DEVIATIONGROUPS
    std::vector<RainParticle> m_particles;                // +0x13F8
};

// dword_1024B92C. ModuleGeom.cpp owns the definition.
extern IBase* g_RainManager;

// The factory registered under "dice.hfe.geom.RainManager" (0x100B0C25).
IBase* __fastcall createRainManager(unsigned int iid, IBase* outer);

// The renderer caches the "RainManager" singleton as an untyped pointer (Renderer +0x5D8).
// The interface derives from IBase first, so the conversion adjusts nothing.
inline IRainManager* asRainManager(void* singleton)
{
    return static_cast<IRainManager*>(static_cast<IBase*>(singleton));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // BF2_GEOM_RAIN_RAINMANAGER_H
