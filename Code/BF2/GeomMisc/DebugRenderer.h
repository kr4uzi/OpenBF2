// Renderer diagnostic geometry queue. Slot order is the PE table 0x1020CC6C.
#ifndef BF2_GEOMMISC_DEBUGRENDERER_H
#define BF2_GEOMMISC_DEBUGRENDERER_H
#include "../../RendDX9Abi/HfeAbi.h"
#include "../Core/IBase.h"
#include "../Math/Mat4.h"
#include "../Math/Frustum.h"
#include <string>
namespace dice { namespace hfe { namespace geom {
class IDebugRenderer : public IBase
{
public:
    virtual void flush() = 0; // +0x10
    virtual void queueGeometry(unsigned int classId, const Mat4& transform,
                               const std::string& technique) = 0; // +0x14
    virtual void* acquireGeometry(unsigned int classId) = 0; // +0x18
    virtual void clear() = 0; // +0x1C
    virtual IBase* createGraph() = 0; // +0x20
    virtual void* destroy(unsigned int flags) = 0; // +0x24
    virtual void queueFrustum(const Frustum& frustum, unsigned int colour,
                               unsigned char projector) = 0; // +0x28
};
extern IBase* g_DebugRenderer;
IBase* __fastcall createDebugRenderer(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugTextDrawer(unsigned int iid, IBase* outer);
void debugTextDrawerFlush(IBase* drawer);
// Queues a DebugBoxMesh named "bounding" from six min/max endpoint floats.
void debugRendererQueueBounds(const float* bounds);
// Typed adapters for the DebugBoxMesh-only extension slots, preserving virtual dispatch.
void debugBoxMeshSetColour(void* geometry, const Math::Vec4& colour);
void debugBoxMeshSetDimensions(void* geometry, const Vec3& dimensions);
} } }
#endif
