// Selectable-item template defaults and class lookup.
#include "ItemTemplate.h"
#include "../../Core/Ids.h"

namespace dice {
namespace hfe {
namespace world {

// ELF C1 0x8182C30 / C2 0x8182BB0, client PE 0x5F17C0 / server PE 0x58F440.
// BundleTemplate constructs first. The three -1 sentinels and zero rank are explicit stores;
// Neither image writes the trailing teamChange byte or its padding.
ItemTemplate::ItemTemplate()
    : m_refCount3(1)
    , m_itemIndex(-1)
    , m_itemType(-1)
    , m_team(-1)
    , m_rank(0)
{
}

// Client PE 0x5ECF10 / 0x4BDE80 / 0x5F9EC0; ELF 0x8183180 / 0x81831E0 / 0x8183230.
void ItemTemplate::addRef() { ++m_refCount3; }
int ItemTemplate::getRef() { return m_refCount3; }
int ItemTemplate::release()
{
    const int refs = --m_refCount3;
    if (refs == 0)
        delete this;
    return refs;
}

// Client PE 0x596540, server PE 0x58F700, ELF 0x81832B0.
void* ItemTemplate::queryInterface(unsigned int iid) const
{
    ItemTemplate* self = const_cast<ItemTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplate)
        return static_cast<IObjectTemplate*>(self);
    if (iid == IID_ISimpleObjectTemplate)
        return static_cast<ISimpleObjectTemplate*>(self);
    if (iid == IID_IBundleTemplate)
        return static_cast<IBundleTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    if (iid == IID_IItemTemplate)
        return static_cast<IItemTemplate*>(self);
    return 0;
}

// Client PE secondary table 0x8EFFBC (+0x22C), ELF 0x8899868 (+0x17C):
// IBase followed by set/getItemIndex and set/getItemType, with no destructor slot.
void ItemTemplate::setItemIndex(int itemIndex) { m_itemIndex = itemIndex; }
int ItemTemplate::getItemIndex() const { return m_itemIndex; }
void ItemTemplate::setItemType(int itemType) { m_itemType = itemType; }
int ItemTemplate::getItemType() const { return m_itemType; }

// . Vtable +0x10. by the getClassID sweep - see
// SoldierTemplate::getClassID in Soldier.cpp for what a missing one of these actually costs.
unsigned int ItemTemplate::getClassID() const
{
    return CID_ItemTemplate;
}

// Recognizes ItemTemplate, delegating other class IDs to BundleTemplate.
void* ItemTemplate::queryImplementation(unsigned int cid)
{
    if (cid == CID_ItemTemplate)
        return this;
    return BundleTemplate::queryImplementation(cid);
}

} // namespace world
} // namespace hfe
} // namespace dice
