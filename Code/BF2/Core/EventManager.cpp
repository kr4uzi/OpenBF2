// Timed event queue with priority-ordered listeners and ownership transferred at postEvent.
// Zero-delay events dispatch immediately unless dispatch is already active; events posted during a
// dispatch wait for the next update. Categories without listeners release the posted node.
// Preserve retail quirks: category 12 passes the array guard, removing the head discards the queue,
// and destruction aborts if listeners remain. Non-sentinel dispatch calls getType twice.

#include "EventManager.h"
#include <cstdio>       // [TEMP] the leak probe
#include <cstdlib>      // [TEMP] getenv
#if defined(_WIN32)
#  include <windows.h>  // [TEMP] IsBadReadPtr - the leak probe's dangling-pointer guard
#endif

#include "Debug.h"      // dice::hfe::Debug, DEBUGTYPE_6 - the five sites above
#include "IBase.h"      // create()'s return type
#include "Ids.h"        // IID_IBase, IID_IEventManager
#if !DEDICATED
#include "../Game/Main/Game.h"
#endif


namespace dice {
namespace hfe {

// construction / destruction

// Starts with one reference, event ID 1 and empty queues, handlers and category maps.
EventManager::EventManager()
    : m_refCount(1),
      m_updateCount(0),
      m_dispatchDepth(0),
      m_pendingEvents(0),
      m_time(0.0),
      m_nextEventId(1)
{
    for (int i = 0; i <= 11; ++i)       // the ELF loop condition is `i <= 11`, twelve categories
        m_handlers[i] = 0;
}

// EventManager::~EventManager - ELF 0x83C3660 (D2) / 0x83C3800 (D1) / 0x83C39A0 (D0), 402/402/410 B.


// Removes queued events. All handlers must be unregistered before destruction;
// remaining listeners trigger a fatal diagnostic rather than being deleted here.
EventManager::~EventManager()
{
    removePendingEvents();

    bool leaked = false;
    for (int i = 0; i <= 11; ++i)
    {
        int count = 0;
        for (Handler* h = m_handlers[i]; h != 0; h = h->m_next)
            ++count;
        if (count > 0)                  // the binary really does count first, then test > 0
            leaked = true;
    }

    // [TEMP] [CHOSEN, not recovered] BF2_EVENT_LEAK_PROBE lists listeners left at shutdown before the fatal check.
    // [RETIRE-WHEN: shutdown-clean] Remove after production shutdown unregisters every listener and passes validation.
    if (leaked && std::getenv("BF2_EVENT_LEAK_PROBE"))
    {
        for (int i = 0; i <= 11; ++i)
            for (Handler* h = m_handlers[i]; h != 0; h = h->m_next)
            {
                void* const listener = static_cast<void*>(h->m_listener);
                void* vtable = 0;
#if defined(_WIN32)
                if (listener && !IsBadReadPtr(listener, sizeof(void*)))
                    vtable = *static_cast<void**>(listener);
#endif
                std::fprintf(stderr, "[event-leak] category %d listener %p priority %d vtable %p\n",
                             i, listener, h->m_priority, vtable);
            }
        std::fflush(stderr);
    }

    if (leaked)
        Debug(DEBUGTYPE_6, "Core/EventManager.cpp", 51)
            << "EventManager leaked, event listeners not unregistered by their systems. "
               "Must fix this or game/level restarts fails";
}

// the IBase quartet - vtable slots +0x00..+0x0C


// (eax holds the new count on exit, but only because the increment went through it; there is no
// separate materialisation, so the return type stays void - Core/IBase.h agrees.)
void EventManager::addRef()
{
    ++m_refCount;
}


int EventManager::getRef()
{
    return m_refCount;
}


// Releases one reference, deleting the manager at zero. Returns the remaining count.
int EventManager::release()
{
    int newCount = --m_refCount;
    if (newCount == 0 && this != 0)
        delete this;
    return newCount;
}


// Two ids, compared in this order against the dword globals: IID_IBase first, IID_IEventManager
// second. The `const_cast` follows Core/System.cpp's precedent for the same const-method shape.
void* EventManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<EventManager*>(this);
    if (iid == IID_IEventManager)
        return const_cast<EventManager*>(this);
    return 0;
}

// posting - vtable slots +0x10..+0x1C


// ownership: this call consumes the node. Three outcomes -
//     no listeners for the category -> release() and drop it (the node never reaches a handler);
//     delay <= 0                    -> dispatch now, then release();
//     delay >  0                    -> hand it to the queue, which releases it in popEvent().
// The float test is `fldz / fucomip / jb`, i.e. the queued path is taken when delay > 0 strictly.
unsigned int EventManager::postEvent(EventCategory category, EventNode* node, float delay)
{
    if (node == 0)
        return 0;

    if (m_handlers[category] == 0)      // no bounds check here - see registerEventHandler
    {
        node->release();
        return 0;
    }

#if !DEDICATED
    // BF2.exe 0x0068DCCB..0x0068DD82. Keep both virtual ID reads and the
    // null/listener guards before validation; notification nodes return -1.
    if (!isDedicated() && node->getType() != -1 && category != 4 &&
        m_eventNames[category].find(node->getType()) == m_eventNames[category].end())
        Debug(DebugTypeError, "Core/EventManager.cpp", 162)
            << "Attempted use of unregistred event";
#endif
    node->m_category = category;

    if (delay > 0.0f)
        return insertEvent(node, delay);

    dispatchEvent(node);
    node->release();
    return 0;
}

// Posts a notification and returns its event ID. Positive delays or reentrant dispatch allocate a node
// and use the virtual node overload; idle immediate notifications reuse the embedded node.
unsigned int EventManager::postEvent(EventCategory category, unsigned int id, void* data, float delay)
{
#if !DEDICATED
    // BF2.exe 0x0068DB0E..0x0068DBC3: raw notifications additionally exempt
    // category 2, and validate before choosing allocation or immediate dispatch.
    if (!isDedicated() && id != static_cast<unsigned int>(-1) &&
        category != 4 && category != 2 &&
        m_eventNames[category].find(id) == m_eventNames[category].end())
        Debug(DebugTypeError, "Core/EventManager.cpp", 112)
            << "Attempted use of unregistred event";
#endif
    if (delay > 0.0f)
    {
        NotificationEventNode* node = new NotificationEventNode(id, data);
        return this->postEvent(category, node, delay);          // ELF +0x10 / PE +0x14
    }

    if (m_dispatchDepth != 0)
    {
        NotificationEventNode* node = new NotificationEventNode(id, data);
        this->postEvent(category, node, 0.0f);                  // ELF +0x10 / PE +0x14, discarded
        return 0;
    }

    m_notificationNode.m_id       = id;              // +0x68
    m_notificationNode.m_data     = data;            // +0x6C
    m_notificationNode.m_category = category;        // +0x50
    dispatchEvent(&m_notificationNode);
    return 0;
}


// Identical to postEvent's queued path with delay 0. It lands at the front of the due window, and
// processEvents' update-stamp guard is what holds it back to the next update - see processEvents.
unsigned int EventManager::postEventNextUpdate(EventCategory category, EventNode* node)
{
    if (node == 0)
        return 0;

    if (m_handlers[category] == 0)
    {
        node->release();
        return 0;
    }

#if !DEDICATED
    // BF2.exe 0x0068DE0B..0x0068DEC2: the node overload exempts only category 4.
    if (!isDedicated() && node->getType() != -1 && category != 4 &&
        m_eventNames[category].find(node->getType()) == m_eventNames[category].end())
        Debug(DebugTypeError, "Core/EventManager.cpp", 202)
            << "Attempted use of unregistred event";
#endif
    node->m_category = category;
    return insertEvent(node, 0.0f);
}

// postEventNextUpdate(EventCategory, unsigned int, void*) -
// Straight wrap-and-delegate through ELF +0x18 / PE +0x1C; no fast path or null test.
unsigned int EventManager::postEventNextUpdate(EventCategory category, unsigned int id, void* data)
{
    NotificationEventNode* node = new NotificationEventNode(id, data);
    return this->postEventNextUpdate(category, node);           // ELF +0x18 / PE +0x1C
}

// the queue


// Stamps the node, gives it the next handle, and links it into the singly-linked queue in ascending
// due-time order. Ties go after the existing entries (`>=` in the walk), so equal-time events keep
// their posting order.
unsigned int EventManager::insertEvent(EventNode* node, float delay)
{
    double due = m_time + delay;

    node->m_updateStamp = m_updateCount;
    node->m_time        = due;
    node->m_eventId     = m_nextEventId++;

    EventNode* cur = m_pendingEvents;
    if (cur == 0)
    {
        node->m_next    = 0;
        m_pendingEvents = node;
        return node->m_eventId;
    }

    if (due < cur->m_time)
    {
        m_pendingEvents = node;         // new head
    }
    else
    {
        EventNode* prev;
        do
        {
            prev = cur;
            cur  = cur->m_next;
        }
        while (cur != 0 && due >= cur->m_time);
        prev->m_next = node;
    }
    node->m_next = cur;                 // cur is the node we stopped in front of (or 0)
    return node->m_eventId;
}


// Unlinks the head and releases it (vtable +0x08 on the node, which is `if (this) delete this`).
// Returns nothing: the exit value in eax is release()'s, not materialised by this function.
void EventManager::popEvent()
{
    EventNode* node = m_pendingEvents;
    if (node == 0)
        return;

    m_pendingEvents = node->m_next;
    node->release();
}

// Advances the queue clock by deltaTime and dispatches due events.
// Increment the update stamp first: posts made inside handlers wait until a later update.
bool EventManager::processEvents(float deltaTime)
{
    EventNode* node = m_pendingEvents;

    m_time += deltaTime;
    ++m_updateCount;

    if (node != 0 && m_time >= node->m_time)
    {
        do
        {
            if (node->m_updateStamp == m_updateCount)
                break;                  // posted during this update - wait for the next one

            dispatchEvent(node);
            popEvent();

            node = m_pendingEvents;
            if (node == 0)
                break;
        }
        while (m_time >= node->m_time);
    }

    return true;
}


// Drains the queue without dispatching - each popEvent releases its node.
void EventManager::removePendingEvents()
{
    while (m_pendingEvents != 0)
        popEvent();
}


// Cancels a queued event by its nonzero handle. Returns false when no matching event exists.
// Existing behavior when canceling the head drops the entire queue but releases only that node.
bool EventManager::removeEvent(unsigned int eventId)
{
    if (eventId == 0)
        return false;

    EventNode* cur = m_pendingEvents;
    if (cur == 0)
        return false;

    EventNode* prev = 0;
    if (eventId != cur->m_eventId)
    {
        do
        {
            prev = cur;
            cur  = cur->m_next;
            if (cur == 0)
                return false;
        }
        while (eventId != cur->m_eventId);
    }

    if (prev != 0)
        prev->m_next = cur->m_next;
    else
        m_pendingEvents = 0;            // the bug. Not `= cur->m_next`.

    cur->release();
    return true;
}

// Delivers a node to category listeners in priority order while holding the dispatch-depth guard.
// Notifications carry their own ID/data; typed nodes pass getType and themselves as payload.
// Preserve the repeated getType call for typed nodes.
void EventManager::dispatchEvent(EventNode* node)
{
    ++m_dispatchDepth;

    unsigned int id;
    void*        userData;
    if (node->getType() == -1)
    {
        // The sentinel path. The cast is what the binary does with no check of its own: it reads
        // node+0x1C and node+0x20 on the strength of getType() alone.
        NotificationEventNode* n = static_cast<NotificationEventNode*>(node);
        id       = n->m_id;
        userData = n->m_data;
    }
    else
    {
        id       = static_cast<unsigned int>(node->getType());
        userData = node;
    }

    // The node's due time is overwritten with the manager's current clock just before delivery, so
    // a handler reads "when this was dispatched", not "when it was scheduled".
    node->m_time = m_time;

    // Select the handler list once, but re-read the event category for each callback.
    Handler* h = m_handlers[node->m_category];
    while (h != 0)
    {
        h->m_listener->handleEvent(node->m_category, id, node, userData);
        h = h->m_next;
    }

    --m_dispatchDepth;
}

// Registers by ascending priority, with newer equal-priority handlers first.
// Requires category 0..11; the existing guard accepts out-of-range values that can corrupt state.
bool EventManager::registerEventHandler(EventCategory category, IEventListener* listener, int priority)
{
    if (listener == 0 || category > 12)     // `> 12` on a 12-entry array. The bug. Kept.
        return false;

    Handler* node = new Handler;            // `operator new(0xCu)`
    node->m_next     = 0;
    node->m_listener = listener;
    node->m_priority = priority;

    Handler* cur = m_handlers[category];
    if (cur == 0)
    {
        m_handlers[category] = node;
        return true;
    }

    Handler* prev = 0;
    while (cur->m_priority < priority)
    {
        prev = cur;
        cur  = cur->m_next;
        if (cur == 0)
        {
            prev->m_next = node;            // appended at the tail
            return true;
        }
    }

    node->m_next = cur;
    if (prev != 0)
        prev->m_next = node;
    else
        m_handlers[category] = node;
    return true;
}


// Same `> 12` guard as above. Removes the first node whose listener matches; a listener registered
// twice on one category needs two calls. Returns false when the category is empty or no node
// matches.
bool EventManager::unregisterEventHandler(EventCategory category, IEventListener* listener)
{
    if (listener == 0 || category > 12)
        return false;

    Handler* head = m_handlers[category];
    if (head == 0)
        return false;

    Handler* cur  = head;
    Handler* prev = head;
    if (cur->m_listener != listener)
    {
        do
        {
            prev = cur;
            cur  = cur->m_next;
            if (cur == 0)
                return false;
        }
        while (cur->m_listener != listener);
    }

    if (cur == head)
        m_handlers[category] = cur->m_next;
    else
        prev->m_next = cur->m_next;

    delete cur;                             // `operator delete` - the Handler is POD, no dtor call
    return true;
}


// no caller in the binary - not even ~EventManager, which counts and complains instead. Recovered
// because it is part of the class's surface (Core/Module.cpp's own inventory of EventManager lists
// it), not because anything exercises it.
void EventManager::unregisterAllHandlers()
{
    for (int i = 0; i <= 11; ++i)
    {
        while (m_handlers[i] != 0)
        {
            Handler* node = m_handlers[i];
            m_handlers[i] = node->m_next;
            delete node;
        }
    }
}

// Event-name tables are indexed by category. These are separate from the network GameEventRegistry.


// no category bounds check at all here or in unregisterEvent - unlike the handler pair, which at
// least checks `> 12`. The index goes straight into the twelve-map array.
bool EventManager::registerEvent(EventCategory category, unsigned int id, const std::string& name)
{
    std::map<unsigned int, std::string>& names = m_eventNames[category];

    std::map<unsigned int, std::string>::iterator it = names.find(id);
    if (it == names.end())
    {
        names.insert(std::pair<unsigned int, std::string>(id, name));
        return true;
    }

    // dice's spelling ("registed"), frozen. The 4th Debug argument is the empty string at
    // 0x88E24BB, i.e. the source used the three-argument form - see Core/Debug.h's default.
    Debug(DEBUGTYPE_6, "Core/EventManager.cpp", 73)
        << "Event " << id << " in cat " << category << " already registed as " << it->second;
    return false;
}


// find() then erase(key) - two lookups, as the binary does it. dice's spelling ("unregistred"),
// frozen. Type 6 aborts, so this diagnostic is fatal in a ship build.
bool EventManager::unregisterEvent(EventCategory category, unsigned int id)
{
    std::map<unsigned int, std::string>& names = m_eventNames[category];

    if (names.find(id) == names.end())
    {
        Debug(DEBUGTYPE_6, "Core/EventManager.cpp", 90)
            << "Trying to unregister unregistred event";
        return false;
    }

    names.erase(id);
    return true;
}

// Factory placement differs from the original: this body was emitted with Core/Module.cpp.


// Creates the requested interface with one owned reference. Returns null for aggregation or an unsupported iid.
IBase* EventManager::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    EventManager* self = new EventManager();        // `operator new(0x104u)` - 260 bytes
    void* itf = self->queryInterface(iid);
    if (itf == 0)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace hfe
} // namespace dice
