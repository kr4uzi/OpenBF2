// Path:     attested in both ports - "Game/Objects/Base/BundleTemplate.cpp".
// Contents: bounded core reconstruction of BundleTemplate's lifecycle, interfaces, binary wire
// format, child-entry ownership, template-list editing, and transform accessors.

#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"
#include "../../../Core/StringUtil.h"
#include "BundleTemplate.h"
#include "Bundle.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Sound/ISoundObjectTemplate.h"
#if !DEDICATED
#include "../../Main/Game.h"
#endif

#include <sstream>

namespace dice {
namespace hfe {
namespace world {

int BundleTemplate::mActiveMemberId = -1;

namespace {

const unsigned int IID_ISoundObjectTemplate = 0x00985FD2u;

IObjectTemplate* resolveTemplate(const BundleTemplate* owner,
                                 BundleTemplate::TemplateInfo* info)
{
    IObjectTemplate* objectTemplate = info->m_template.get();
    if (!objectTemplate)
    {
        objectTemplate = objectTemplateManager->getTemplate(info->m_templateName);   // +0x24 by name
        WeakPtr<IObjectTemplate> cached = objectTemplate
            ? objectTemplate->getWeakPtr()
            : const_cast<BundleTemplate*>(owner)->getWeakPtr();
        info->m_template = cached;
        objectTemplate = info->m_template.get();
    }
    return objectTemplate == owner ? 0 : objectTemplate;
}

} // unnamed namespace

BundleTemplate::TemplateInfo::TemplateInfo(const std::string& templateName)
    : m_templateName(templateName)
    , m_template()
    , m_inputId(-1)
    , m_unknown50(false)
{
    for (int i = 0; i != 16; ++i)
        m_transformation.m[i] = 0.0f;
    m_transformation.m[0] = 1.0f;
    m_transformation.m[5] = 1.0f;
    m_transformation.m[10] = 1.0f;
    m_transformation.m[15] = 1.0f;
}

BundleTemplate::TemplateInfo::~TemplateInfo()
{
}

BundleTemplate::BundleTemplate()
    : m_refCount2(1)
    , m_templateInfos()
    , m_explodePartAtDestroy(false)
    , m_killHeatSeekingMissile(false)
{
}

BundleTemplate::~BundleTemplate()
{
    for (std::vector<TemplateInfo*>::iterator it = m_templateInfos.begin();
         it != m_templateInfos.end(); ++it)
    {
        delete *it;
    }
}

void BundleTemplate::addRef()
{
    ++m_refCount2;
}

int BundleTemplate::getRef()
{
    return m_refCount2;
}

int BundleTemplate::release()
{
    const int refs = --m_refCount2;
    if (refs == 0)
        delete this;
    return refs;
}

void* BundleTemplate::queryInterface(unsigned int iid) const
{
    BundleTemplate* self = const_cast<BundleTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplate)
        return static_cast<IObjectTemplate*>(self);
    if (iid == IID_ISimpleObjectTemplate)
        return static_cast<ISimpleObjectTemplate*>(self);
    if (iid == IID_IBundleTemplate)
        return static_cast<IBundleTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int BundleTemplate::getClassID() const
{
    return CID_BundleTemplate;
}

// Writes the base template, then each member through virtual makeMemberScript.
// Returns false on base failure. Nonempty member lists are bracketed by separators whose write counts are ignored.
bool BundleTemplate::makeScript(io::IStream* stream)
{
    if (!SimpleObjectTemplate::makeScript(stream))
        return false;

    const bool hasMembers = !m_templateInfos.empty();

    std::ostringstream separator;
    separator << "rem -------------------------------------" << std::endl;
    const std::string text = separator.str();

    if (hasMembers)
        stream->write(text.c_str(), static_cast<unsigned int>(text.length()));

    for (std::vector<TemplateInfo*>::const_iterator it = m_templateInfos.begin();
         it != m_templateInfos.end(); ++it)
    {
        makeMemberScript(*it, stream);
    }

    if (hasMembers)
        stream->write(text.c_str(), static_cast<unsigned int>(text.length()));

    return true;
}

// ELF81D8EC0 / PE5EE710. The return is bool: both materialize the write-count comparison,
// and EffectBundleTemplate tests AL after the base call (ELF81FC5AB / PE5F9284).
// The existing makeScript caller intentionally ignores this per-member result.
bool BundleTemplate::makeMemberScript(const TemplateInfo* info, io::IStream* stream)
{
    std::ostringstream out;
    IObjectTemplate* objectTemplate = objectTemplateManager->getTemplate(info->m_templateName);
    if (!objectTemplate)
        return false;
    // Three separate virtual reads, in this order. The first tests AL's sign (bit7),
    // not the sign of the full flags word; the decompiler's signed-int test is misleading.
    if ((objectTemplate->getFlags() & 0x80u) == 0 &&
        (objectTemplate->getFlags() & 1u) == 0 &&
        (objectTemplate->getFlags() & 0x400u) == 0)
        return false;

    out << "ObjectTemplate.addTemplate " << info->m_templateName << std::endl;
    const Mat4& transform = info->m_transformation;
    if (transform.m[12] < -0.001f || transform.m[12] > 0.001f ||
        transform.m[13] < -0.001f || transform.m[13] > 0.001f ||
        transform.m[14] < -0.001f || transform.m[14] > 0.001f)
    {
        out << "ObjectTemplate.setPosition " << transform.m[12] << '/'
            << transform.m[13] << '/' << transform.m[14] << std::endl;
    }
    float z, x, y;
    getRotationZXY(transform, z, x, y);
    const Vec3 rotation = {-y, -x, -z};
    if (rotation.x < -0.01f || rotation.x > 0.01f ||
        rotation.y < -0.01f || rotation.y > 0.01f ||
        rotation.z < -0.01f || rotation.z > 0.01f)
    {
#if defined(_MSC_VER)
        // [PLATFORM] PE5EE917 recomputes before inserting the prefix; ELF81D91CF after it.
        getRotationZXY(transform, z, x, y);
        out << "ObjectTemplate.setRotation ";
#else
        out << "ObjectTemplate.setRotation ";
        getRotationZXY(transform, z, x, y);
#endif
        out << -y << '/' << -x << '/' << -z << std::endl;
    }
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

IObject* BundleTemplate::createObject() const { return new Bundle(this); }

void BundleTemplate::addBundleChilds(IObject* object) const
{
    char childId = 0;
    for (std::vector<TemplateInfo*>::const_iterator it = m_templateInfos.begin();
         it != m_templateInfos.end(); ++it, ++childId)
    {
        TemplateInfo* info = *it;
        IObjectTemplate* objectTemplate = resolveTemplate(this, info);
        if (!objectTemplate)
        {
#if !DEDICATED
            // BF2.exe 0x0062892C..0x006289DA: missing/self references report
            // through Debug, except for the optional S_ sound templates.
            if (!isDedicated() && compareIgnoreCase(info->m_templateName.c_str(), "S_", 2) != 0)
                Debug(DebugTypeError, "Game/Objects/Base/BundleTemplate.cpp", 213)
                    << getName() << " addBundleChilds() template not found:"
                    << info->m_templateName;
#endif
            continue;
        }

        ISoundObjectTemplate* soundTemplate =
            static_cast<ISoundObjectTemplate*>(
                objectTemplate->queryInterface(IID_ISoundObjectTemplate));
        if (soundTemplate)
            soundTemplate->setRelativeTransformation(&info->m_transformation);

        IObject* child = objectTemplate->createObject();
        if (!child)
            continue;

        child->setChildId(childId);
        object->addChild(child);

        const IObjectTemplate* childObjectTemplate = child->getTemplate();
        ISimpleObjectTemplate* simpleTemplate = static_cast<ISimpleObjectTemplate*>(
            childObjectTemplate->queryInterface(IID_ISimpleObjectTemplate));
        if (simpleTemplate &&
            (childObjectTemplate->getFlags() & 0x1000u) != 0 &&
            !info->m_unknown50)
        {
            const Vec3 offset = simpleTemplate->getRelativePositionOffset();
            info->m_transformation.m[12] += offset.x;
            info->m_transformation.m[13] += offset.y;
            info->m_transformation.m[14] += offset.z;
            child->setRelativeTransformation(info->m_transformation);
            info->m_unknown50 = true;
        }
        else
            child->setRelativeTransformation(info->m_transformation);
        child->init();
    }
}

void BundleTemplate::addDriverDecals(IObject* object) const
{
    for (std::vector<TemplateInfo*>::const_iterator it = m_templateInfos.begin();
         it != m_templateInfos.end(); ++it)
    {
        TemplateInfo* info = *it;
        IObjectTemplate* objectTemplate = resolveTemplate(this, info);
        if (!objectTemplate)
            continue;

        const unsigned int classId = objectTemplate->getClassID();
        if (classId != CID_DecalTemplate || objectTemplate->getClassID() != classId)
            continue;

        if ((m_physicsFlags & 0x04u) == 0)
        {
            Debug(DEBUGTYPE_6, "Game/Objects/Base/BundleTemplate.cpp", 124)
                << "Can only add static decals to objects with collision";
        }

        IPlayerControlObject* playerControlObject = 0;
        for (IObject* parent = object; parent && !playerControlObject;
             parent = parent->getParent())
        {
            playerControlObject = static_cast<IPlayerControlObject*>(
                parent->queryInterface(IID_IPlayerControlObject));
        }

        if (playerControlObject)
        {
            playerControlObject->setDriverDecalObject(object);
            playerControlObject->addDriverDecalTransformation(info->m_transformation);
        }
    }
}

bool BundleTemplate::writeBinary(io::IStream* stream) const
{
    if (!SimpleObjectTemplate::writeBinary(stream))
        return false;

    // Both ports ignore every raw write result after the base succeeds.
    stream->write(&m_explodePartAtDestroy, 1);

    const int count = static_cast<int>(m_templateInfos.size());
    stream->write(&count, 4);
    for (std::vector<TemplateInfo*>::const_iterator it = m_templateInfos.begin();
         it != m_templateInfos.end(); ++it)
    {
        const TemplateInfo* info = *it;
        const int length = static_cast<int>(info->m_templateName.size());
        stream->write(&length, 4);
        stream->write(info->m_templateName.data(), static_cast<unsigned int>(length));
        stream->write(&info->m_inputId, 4);
        stream->write(&info->m_transformation, 64);
    }
    return true;
}

bool BundleTemplate::readBinary(io::IStream* stream)
{
    if (!SimpleObjectTemplate::readBinary(stream))
        return false;

    // No read below is checked, and existing entries are deliberately retained. In particular,
    // addTemplate may select an existing S_ child; the following fields then overwrite that entry.
    stream->read(&m_explodePartAtDestroy, 1);

    int count = 0;
    stream->read(&count, 4);
    for (int i = 0; i < count; ++i)
    {
        std::string name;
        io::read(stream, name);
        addTemplate(name);

        TemplateInfo* info = m_templateInfos[mActiveMemberId];
        stream->read(&info->m_inputId, 4);
        stream->read(&info->m_transformation, 64);
    }
    return true;
}

void* BundleTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_BundleTemplate ||
        classId == CID_SimpleObjectTemplate ||
        classId == CID_ObjectTemplate)
    {
        return this;
    }
    return 0;
}

BundleTemplate::TemplateInfo*
BundleTemplate::createTemplateInfo(const std::string& templateName)
{
    return new TemplateInfo(templateName);
}

void BundleTemplate::addTemplate(const std::string& templateName)
{
    if (templateName.substr(0, 2) == "S_")
    {
        for (unsigned int i = 0; i != m_templateInfos.size(); ++i)
        {
            if (compareIgnoreCase(m_templateInfos[i]->m_templateName, templateName) == 0)
            {
                mActiveMemberId = static_cast<int>(i);
                return;
            }
        }
    }

    m_templateInfos.push_back(createTemplateInfo(templateName));
    mActiveMemberId = static_cast<int>(m_templateInfos.size()) - 1;
}

void BundleTemplate::removeTemplate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return;

    std::vector<TemplateInfo*>::iterator it = m_templateInfos.begin() + index;
    delete *it;
    m_templateInfos.erase(it);
}

std::string BundleTemplate::getTemplate(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return std::string();
    return m_templateInfos[index]->m_templateName;
}

int BundleTemplate::getNoTemplates() const
{
    return static_cast<int>(m_templateInfos.size());
}

void BundleTemplate::setTransformation(const Mat4& transformation, int index)
{
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return;
    m_templateInfos[index]->m_transformation = transformation;
}

const Mat4& BundleTemplate::getTransformation(int index) const
{
    // Deliberately unchecked in both shipped ports.
    return m_templateInfos[index]->m_transformation;
}

void BundleTemplate::renameChildTemplate(
    const std::string& from,
    const std::string& to)
{
    for (std::vector<TemplateInfo*>::iterator it = m_templateInfos.begin();
         it != m_templateInfos.end(); ++it)
    {
        TemplateInfo* info = *it;
        if (compareIgnoreCase(info->m_templateName, from) == 0)
        {
            info->m_templateName = to;
            info->m_template = WeakPtr<IObjectTemplate>();
        }
    }
}

void BundleTemplate::setInputId(int inputId, int index)
{
    if (index >= 0 && index < static_cast<int>(m_templateInfos.size()))
        m_templateInfos[index]->m_inputId = inputId;
}

void BundleTemplate::setPosition(const Vec3& position, int index)
{
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return;

    Mat4& transform = m_templateInfos[index]->m_transformation;
    transform.m[12] = position.x;
    transform.m[13] = position.y;
    transform.m[14] = position.z;
}

void BundleTemplate::setRotation(const Vec3& rotation, int index)
{
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return;
    setRotationZXY(
        m_templateInfos[index]->m_transformation,
        -rotation.z,
        -rotation.y,
        -rotation.x);
}

std::string BundleTemplate::listTemplates() const
{
    std::string result;
    for (unsigned int i = 0; i != m_templateInfos.size(); ++i)
    {
        std::ostringstream line;
        line << i << " " << m_templateInfos[i]->m_templateName << std::endl;
        result += line.str();
    }
    return result;
}

void BundleTemplate::clearTemplateList()
{
    // Deliberately does not delete the pointed-to TemplateInfo records. Both shipped ports drop
    // only the pointer container here; normal removal and destruction are the owning paths.
    m_templateInfos.clear();
}

Vec3 BundleTemplate::getRotation(int index) const
{
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return result;

    float a;
    float b;
    float c;
    getRotationZXY(m_templateInfos[index]->m_transformation, a, b, c);
    result.x = -c;
    result.y = -b;
    result.z = -a;
    return result;
}

Vec3 BundleTemplate::getPosition(int index) const
{
    Vec3 result = {0.0f, 0.0f, 0.0f};
    if (index < 0 || index >= static_cast<int>(m_templateInfos.size()))
        return result;

    const Mat4& transform = m_templateInfos[index]->m_transformation;
    result.x = transform.m[12];
    result.y = transform.m[13];
    result.z = transform.m[14];
    return result;
}

void BundleTemplate::setActiveTemplate(int index)
{
    if (index >= 0 && index < static_cast<int>(m_templateInfos.size()))
        mActiveMemberId = index;
}

// Creates a sound template, attaches it as a bundle child, and returns it.
ObjectTemplate* BundleTemplate::addSoundTemplate(const std::string& name)
{
    ObjectTemplate* soundTemplate = SimpleObjectTemplate::addSoundTemplate(name);
    addTemplate(makeSoundTemplateName(name));    // the virtual - a leaf's override would apply
    return soundTemplate;
}

} // namespace world
} // namespace hfe
} // namespace dice
