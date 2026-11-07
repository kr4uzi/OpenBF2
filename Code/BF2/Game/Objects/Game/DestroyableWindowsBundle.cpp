// Path: [INFERRED]. Runtime/template pairing follows the ELF TU's global constructor.
// Evidence: ELF 0x8175500..0x817693F / PE 0x58F610..0x58FFFF.
#include "DestroyableWindowsBundle.h"
#include "../GeometryPartAccess.h"   // named geometry operations (non-MSVC)
#include "../TemplateLeaves.h"
#include "../../IGame.h"
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"
#include "../../../IO/Stream/IStream.h"
#include "../../../Math/LocalBoundingBox.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectUtil.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <cmath>
#include <sstream>

namespace dice { namespace hfe { namespace world {
namespace {
// Narrow renderer seam, as in Base/Bundle.cpp: no complete IGeometry interface is recovered.
// Both dedicated images retain these calls even though normal dedicated objects have no geometry.
// Names of the secondary-geometry pair are [INFERRED]. Offsets are read on both ports, not derived
// by subtracting a destructor slot: part transform PE +0x4C / ELF +0x54, secondary getter
// PE +0xCC / ELF +0xD0, setter PE +0xC8 / ELF +0xCC. All are instance calls.
#if defined(_MSC_VER)
#define BF2_WINDOWS_CALL __thiscall
const unsigned int partSlot = 0x4C, getSecondarySlot = 0xCC, setSecondarySlot = 0xC8;
template<class F> F geometrySlot(void* geometry, unsigned int offset)
{
    return reinterpret_cast<F>((*static_cast<void***>(geometry))[offset / 4]);
}
void* secondaryGeometry(void* geometry)
{
    typedef void* (BF2_WINDOWS_CALL* F)(void*);
    return geometrySlot<F>(geometry, getSecondarySlot)(geometry);
}
void setSecondaryGeometry(void* geometry, void* secondary)
{
    typedef void (BF2_WINDOWS_CALL* F)(void*, void*);
    geometrySlot<F>(geometry, setSecondarySlot)(geometry, secondary);
}
void setPartTransformation(void* geometry, const Mat4& matrix, int part)
{
    typedef void (BF2_WINDOWS_CALL* F)(void*, const Mat4&, int);
    geometrySlot<F>(geometry, partSlot)(geometry, matrix, part);
}
#undef BF2_WINDOWS_CALL
#else
// Non-MSVC: the named geometry_part operations (ELF +0x54 / +0xD0 / +0xCC).
void* secondaryGeometry(void* geometry) { return geometry_part::getNextGeometry(geometry); }
void setSecondaryGeometry(void* geometry, void* secondary) { geometry_part::setNextGeometry(geometry, secondary); }
void setPartTransformation(void* geometry, const Mat4& matrix, int part)
{ geometry_part::setTransform(geometry, &matrix, part); }
#endif
}

DestroyableWindowsBundle::DestroyableWindowsBundle(const DestroyableWindowsBundleTemplate* objectTemplate)
    : Bundle(objectTemplate), m_refCount(1), m_states(~0u), m_previousStates(0) {}
DestroyableWindowsBundle::~DestroyableWindowsBundle()
{
    if (m_geometry && secondaryGeometry(m_geometry))
    {
        static_cast<IBase*>(secondaryGeometry(m_geometry))->release();
        setSecondaryGeometry(m_geometry, 0);
    }
}
void DestroyableWindowsBundle::addRef() { ++m_refCount; }
int DestroyableWindowsBundle::getRef() { return m_refCount; }
int DestroyableWindowsBundle::release()
{
    const int result = --m_refCount;
    if (!result) delete this;
    return result;
}
void* DestroyableWindowsBundle::queryInterface(unsigned int iid) const
{
    DestroyableWindowsBundle* self = const_cast<DestroyableWindowsBundle*>(this);
    if (iid == IID_IBase || iid == IID_IObject) return static_cast<IObject*>(self);
    if (iid == io::IID_IConsoleSaveable) return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}
void DestroyableWindowsBundle::init()
{
    Bundle::init();
    updateStates(~0u);
}
void DestroyableWindowsBundle::handleMessage(TemplateMessage message, IPlayer* player, bool network)
{
    if (message == static_cast<TemplateMessage>(29)) updateStates(~0u);
    Bundle::handleMessage(message, player, network);
}
// Retail intentionally declines root collisions (ELF 0x8176680 / PE 0x590000).
bool DestroyableWindowsBundle::handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
    int, int, CollisionFaceInfo*, CollisionFaceInfo*) { return false; }

void DestroyableWindowsBundle::updateStates(unsigned int states)
{
    if (m_previousStates != states)
    {
        // This all-intact gate, including its apparently unusual timing, is present in both ports.
        if (states == ~0u && game && game->queryInterface(IID_IGameServer))
        {
            const float delay = static_cast<const DestroyableWindowsBundleTemplate*>(m_template)->respawnDelay;
            if (delay > 0.0f) objectManager->postMessage(this, static_cast<TemplateMessage>(29), delay);
        }
        m_states = states;
        void* geometry = m_geometry;
        for (IObject* child = getChild(); child; child = child->m_sibling)
        {
            if (child->m_template->getClassID() != CID_DestroyableWindowTemplate) continue;
            const int part = child->getGeometryPart();
            if (part == -1) continue;
            // x86 shifts mask their count to five bits; avoid undefined signed/oversized C++ shifts.
            const bool intact = (m_states & (1u << (static_cast<unsigned int>(part) & 31))) != 0;
            static_cast<DestroyableWindow*>(child)->setIsBroken(!intact);
            if (geometry)
            {
                setPartTransformation(geometry, intact ? getAbsoluteTransformation() : Mat4::getZero(), part);
                if (secondaryGeometry(geometry))
                {
                    // [PLATFORM] The compilers evaluate the receiver/transform in opposite orders.
#if defined(_MSC_VER)
                    void* secondary = secondaryGeometry(geometry);
                    const Mat4& transform = intact ? Mat4::getZero() : getAbsoluteTransformation();
#else
                    const Mat4& transform = intact ? Mat4::getZero() : getAbsoluteTransformation();
                    void* secondary = secondaryGeometry(geometry);
#endif
                    setPartTransformation(secondary, transform, part);
                }
            }
        }
    }
    m_previousStates = m_states;
}
void DestroyableWindowsBundle::initNetworkableComp()
{
    if (m_template && m_template->getNetworkableInfo().compare("") != 0)
        setNetworkableComp(this, CID_DestroyableWindowsBundleNetworkable);
}
unsigned int DestroyableWindowsBundle::getNumStates() const
{
    return static_cast<const DestroyableWindowsBundleTemplate*>(m_template)->numWindows;
}
void DestroyableWindowsBundle::handleChildCollision(DestroyableWindow* child)
{
    const int part = child->getGeometryPart();
    if (part != -1) m_states &= ~(1u << (static_cast<unsigned int>(part) & 31));
    updateStates(m_states);
}

IObject* DestroyableWindowsBundleTemplate::createObject() const { return new DestroyableWindowsBundle(this); }
IObjectTemplate* DestroyableWindowsBundleTemplate::getEffectTemplate(const LocalBoundingBox& box)
{
    LocalBoundingBox& bounds = const_cast<LocalBoundingBox&>(box);
    const float x = bounds.getSize().x;
    const float z = bounds.getSize().z;
    const float width = z > x ? z : x;
    const float height = bounds.getSize().y;
    const float sizes[] = {windowSizeVector.x, windowSizeVector.y, windowSizeVector.z};
    int widthIndex = 0, heightIndex = 0;
    for (int i = 2; i >= 0; --i) if (width > sizes[i]) { widthIndex = i; break; }
    for (int i = 2; i >= 0; --i) if (height > sizes[i]) { heightIndex = i; break; }
    IObjectTemplate*& effect = m_effectTemplates[widthIndex * 3 + heightIndex];
    if (!effect)
        effect = objectTemplateManager->getTemplate(effectTemplateBaseName + formatString("%dX%d", widthIndex, heightIndex));
    return effect;
}
bool DestroyableWindowsBundleTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    out << "ObjectTemplate.respawnDelay " << respawnDelay << std::endl;
    out << "ObjectTemplate.clientHitDetection " << clientHitDetection << std::endl;
    out << "ObjectTemplate.setNumWindows " << numWindows << std::endl;
    if (!equalsIgnoreCase(effectTemplateBaseName, "e_destroyableWindow_"))
        out << "ObjectTemplate.effectTemplateBaseName " << effectTemplateBaseName << std::endl;
    if (!(std::fabs(windowSizeVector.x - 0.2f) <= 1.1920929e-7f &&
          std::fabs(windowSizeVector.y - 1.0f) <= 1.1920929e-7f &&
          std::fabs(windowSizeVector.z - 1.8f) <= 1.1920929e-7f))
        out << "ObjectTemplate.setWindowSizeVector " << windowSizeVector.x << '/'
            << windowSizeVector.y << '/' << windowSizeVector.z << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}
} } }
