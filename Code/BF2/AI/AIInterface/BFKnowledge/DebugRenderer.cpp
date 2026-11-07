// [INFERRED] Source placement.
// The AI's debug renderer forwards to the engine's debug drawers (RenderSingletonServer, filled by
// its updateStaticMethods). bf2 (ia-32, dedicated) and BF2.exe (client, vtable 0x947AE8) have the
// same bodies: drawAll and reset are empty, the draw calls forward without a null check.

#include "DebugRenderer.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../SingletonServer.h"     // (!) its roster static is BFSingletonServer's, not Common's
#include "../../../Core/ClassManager.h"
#include "../../../Core/IBase.h"
#include "../../../GeomMisc/DebugCircleDrawer.h"
#include "../../../GeomMisc/DebugLineDrawer.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

namespace {

// The line and circle drawers use the GeomMisc interfaces recovered with RendDX9. The text writer's
// slots are declared locally, in table order ([INFERRED] interface name). Slot offsets and
// argument order are BF2.exe's (0x818080..0x818170); the geom::DebugTextWriter slots are the ones
// GeomMisc/DebugTextWriter.cpp implements. bf2 (ia-32) calls the text writer at +0x38/+0x40
// instead - the Linux engine's writer table is ordered differently - with the same arguments.

// "DebugTextWriter" (geom::DebugTextWriter, IID_IDebugTextWriter 80043).
class IDebugTextWriter : public IBase
{
public:
    virtual bool setFont(const std::string& path) = 0;                            // +0x10
    virtual bool isEnabled() const = 0;                                           // +0x14
    virtual void setEnabled(bool enabled) = 0;                                    // +0x18
    virtual unsigned int getColour() const = 0;                                   // +0x1C
    virtual void setColour(unsigned int colour) = 0;                              // +0x20
    virtual void queueWorldGroupColoured(const Vec3& position, unsigned int colour,
                                         const std::vector<std::string>& text, int spacing,
                                         int alignment, int bottomAligned) = 0;   // +0x24
    virtual void queueWorldTextColoured(const Vec3& position, unsigned int colour,
                                        const std::string& text, int alignment) = 0;   // +0x28
    virtual void queueGroupColoured(int x, int y, unsigned int colour,
                                    const std::vector<std::string>& text, int spacing,
                                    int alignment, int bottomAligned) = 0;        // +0x2C

protected:
    ~IDebugTextWriter() {}
};

// The line drawer's table is recovered with RendDX9 (geom::IDebugLineDrawer, PE 0x1020C758).
inline geom::IDebugLineDrawer* lineDrawer()
{
    return static_cast<geom::IDebugLineDrawer*>(RenderSingletonServer::m_debugLineDrawer);
}

// The circle drawer's table is recovered with RendDX9 (geom::IDebugCircleDrawer, PE 0x1020CB98).
inline geom::IDebugCircleDrawer* circleDrawer()
{
    return static_cast<geom::IDebugCircleDrawer*>(RenderSingletonServer::m_debugCircleDrawer);
}

inline IDebugTextWriter* textWriter()
{
    return static_cast<IDebugTextWriter*>(RenderSingletonServer::m_debugTextWriter);
}

} // namespace

// First wins in, exact match out.
IDebugRenderer::IDebugRenderer()
{
    if (!BFSingletonServer::m_iDebugRenderer)
        classManager->registerSimpleSingleton(std::string("IDebugRenderer"), this);
}

IDebugRenderer::~IDebugRenderer()
{
    if (BFSingletonServer::m_iDebugRenderer == this)
        classManager->unregisterSimpleSingleton(std::string("IDebugRenderer"));
}

DebugRenderer::DebugRenderer()
{
}

DebugRenderer::~DebugRenderer()
{
}

// ELF 0x842E780, PE 0x83D270 (a shared `ret`): empty in both images.
void DebugRenderer::drawAll()
{
}

// ELF 0x842E790, PE 0x83D270 (a shared `ret`): empty in both images.
void DebugRenderer::reset()
{
}

// ELF 0x842E7A0, PE 0x818080: the line drawer's +0x20 (drawRibbon) with depth true. Retail hands
// the Vec3 vector to the D3DX-vector slot as it is; both element types are three packed floats.
// [CHOSEN, not recovered] Returns 0: the slot is void (no caller reads the result).
int DebugRenderer::drawLine(const std::vector<Vec3>& points, unsigned int colour, float width) const
{
    lineDrawer()->drawRibbon(reinterpret_cast<const std::vector<D3DXVECTOR3>&>(points), colour, width, true);
    return 0;
}

// ELF 0x842E7E0, PE 0x8180A0 (a tail jump): the circle drawer's +0x10, a void slot. The Boolean
// selects terrain heights for the vertices; the count goes through unchanged.
// [CHOSEN, not recovered] Returns 0: the tail jump leaves the drawer's register (no caller reads it).
int DebugRenderer::drawCircle(const Vec3& normal, const Vec3& centre, float radius, unsigned int colour, bool terrain, int segments) const
{
    circleDrawer()->drawCircle(normal, centre, radius, colour, terrain, static_cast<unsigned int>(segments));
    return 0;
}

// ELF 0x842E830, PE 0x8180B0: the text writer's queueGroupColoured at the truncated screen
// position; `size` is the line spacing, right alignment is AITextAlignmentX 1, bottom alignment
// any non-zero AITextAlignmentY.
// [CHOSEN, not recovered] Both images return the register the void call leaves; 0 here (no caller
// reads it).
int DebugRenderer::drawScreenText(const Vec2& position, const std::vector<std::string>& lines, int size, unsigned int colour, AITextAlignmentX ax, AITextAlignmentY ay) const
{
    textWriter()->queueGroupColoured(static_cast<int>(position.x), static_cast<int>(position.y), colour,
                                     lines, size, ax == 1, ay != 0);
    return 0;
}

// ELF 0x842E8A0, PE 0x818120: as drawScreenText, through queueWorldGroupColoured.
// [CHOSEN, not recovered] Returns 0 for the same reason.
int DebugRenderer::draw3dText(const Vec3& position, const std::vector<std::string>& lines, int size, unsigned int colour, AITextAlignmentX ax, AITextAlignmentY ay) const
{
    textWriter()->queueWorldGroupColoured(position, colour, lines, size, ax == 1, ay != 0);
    return 0;
}

// ELF 0x842E8F0, PE 0x818160 (a tail jump): the line drawer's +0x2C (drawTriangles, per-triangle
// colours). [CHOSEN, not recovered] Returns 0: the slot is void.
int DebugRenderer::drawTriangle(const std::vector<Vec3>& vertices, const std::vector<unsigned int>& colours, bool depth) const
{
    lineDrawer()->drawTriangles(vertices, colours, depth);
    return 0;
}

// ELF 0x842E920, PE 0x818170 (a tail jump): the line drawer's +0x30. [CHOSEN, not recovered]
// Returns 0: the slot is void.
int DebugRenderer::drawMesh(Vec3* vertices, unsigned int* indices, int count, unsigned int* colours, bool depth) const
{
    lineDrawer()->drawMesh(vertices, indices, count, colours, depth);
    return 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
