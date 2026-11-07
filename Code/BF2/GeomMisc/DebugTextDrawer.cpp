// Queued screen-space text used by DebugRenderer's final flush.
#ifdef BF2_IMAGE_RENDDX9
#include "DebugRenderer.h"
#include "Debug.h"
#include "../RendDX9/RendDX9Base.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include <new>
#include <vector>

namespace dice { namespace hfe { namespace geom {
bool fontPortedLoad(IBase*, const std::string&);
void fontPortedDrawText(IBase*, float, float, const std::string&);
extern const unsigned int CID_DebugTextDrawer = 81037u;
namespace {
struct DebugTextDrawer;

// PE secondary table 0x1020CAD8 is a RenderPathListener, not a device-lost listener.
class DrawerPathListener : public dx9rend::RenderPathListener
{
public:
    void onRenderPathActivated(int);
    void onRenderPathDeactivated() {} // PE 0x10037D40
    void onRenderPathChanged(int) {} // PE 0x100E13B0
    void onRenderPathNotification(int) {} // PE 0x100E13B0
    int getCurrentRenderPath() const { return currentPath; } // PE 0x101A9A10
    std::string getRenderPathListenerName() const { return "DebugTextDrawer"; } // PE 0x10098150
    // Shipped constructor and callbacks leave this getter's field unwritten.
    int currentPath; // PE secondary +0x04
#if !defined(_M_IX86)
    // [CHOSEN, not recovered] Native owner link replaces the retail secondary-this -4 adjustment.
    DebugTextDrawer* owner;
#endif
};

struct DrawerLine
{
    int x, y;
    std::string text;
};

struct DebugTextDrawer
{
    DebugTextDrawer();
    ~DebugTextDrawer();
    void flush();
    void releaseFont();
    void* const* table;
    DrawerPathListener listener; // PE +0x04 (with its unwritten currentPath at +0x08)
    int refs;                   // PE +0x0C
    std::vector<DrawerLine> lines; // PE +0x10 (including the old allocator word)
    IBase* font;                // PE +0x20
};

extern void* const kDrawerTable[6];
// PE 0x10132630 / 0x10051710 / 0x10098170 / 0x10098120 / 0x10098190.
int RD9_THUNK drawer_addRef(DebugTextDrawer* self RD9_THUNK_EDX) { return ++self->refs; }
int RD9_THUNK drawer_getRef(DebugTextDrawer* self RD9_THUNK_EDX) { return self->refs; }
// PE 0x10098190: deleting variant of the owner destructor.
void* RD9_THUNK drawer_destroy(DebugTextDrawer* self RD9_THUNK_EDX, unsigned int flags)
{
    self->~DebugTextDrawer();
    if (flags & 1) ::renddx9abi::engineFree(self);
    return self;
}
int RD9_THUNK drawer_release(DebugTextDrawer* self RD9_THUNK_EDX)
{
    const int refs = --self->refs;
    if (!refs)
    {
        typedef void* (__thiscall* Destroy)(void*, unsigned int);
        reinterpret_cast<Destroy>(self->table[5])(self, 1);
    }
    return refs;
}
void* RD9_THUNK drawer_query(DebugTextDrawer* self RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 80038u ? self : 0; }
// PE 0x10097820. The temporary's string is default-constructed before assignment.
void RD9_THUNK drawer_queue(DebugTextDrawer* self RD9_THUNK_EDX, int x, int y, const std::string& text)
{
    DrawerLine line;
    line.x = x; line.y = y; line.text = text;
    self->lines.push_back(line);
}
void* const kDrawerTable[6] = {
    reinterpret_cast<void*>(&drawer_addRef), reinterpret_cast<void*>(&drawer_getRef),
    reinterpret_cast<void*>(&drawer_release), reinterpret_cast<void*>(&drawer_query),
    reinterpret_cast<void*>(&drawer_queue), reinterpret_cast<void*>(&drawer_destroy)
};

// PE 0x100976C0. Receiver is the secondary listener and the path argument is unused.
void DrawerPathListener::onRenderPathActivated(int)
{
#if defined(_M_IX86)
    DebugTextDrawer* self = reinterpret_cast<DebugTextDrawer*>(reinterpret_cast<char*>(this) - 4);
#else
    DebugTextDrawer* self = owner;
#endif
    if (!fontPortedLoad(self->font, "Fonts/BF1942.font"))
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugTextDrawer.cpp", 52, "")
            << "Failed to load font Fonts/BF1942.font, aborting.";
}

// PE 0x10097780. Font creation precedes listener registration and its activation callback.
DebugTextDrawer::DebugTextDrawer() : table(kDrawerTable), refs(1), font(0)
{
#if !defined(_M_IX86)
    listener.owner = this;
#endif
    font = classManager->createInstance(81007u, 80001u, 0);
    dx9rend::registerRenderPathListener(&listener);
}

// Releases the font and clears the pointer; a missing font is skipped.
// PE 0x100976A0. unreachable in 1.50: no code or data references; the destructor 0x100977E0
// inlines the same sequence. [INFERRED] Name and owner receiver (the font is owner +0x20).
void DebugTextDrawer::releaseFont()
{
    if (font) { font->release(); font = 0; }
}

// PE 0x100977E0. Release font, unregister listener, then destroy queued strings.
DebugTextDrawer::~DebugTextDrawer()
{
    table = kDrawerTable;
    releaseFont();
    dx9rend::unregisterRenderPathListener(&listener);
}

// PE 0x10097870. resize(0, value) constructs the otherwise unused sentinel record.
void DebugTextDrawer::flush()
{
    for (auto cursor = lines.begin(); cursor != lines.end(); ++cursor)
        fontPortedDrawText(font, static_cast<float>(cursor->x), static_cast<float>(cursor->y), cursor->text);
    DrawerLine value;
    value.x = -99999; value.y = -88888; value.text = "";
    lines.resize(0, value);
}
} // namespace

void debugTextDrawerFlush(IBase* drawer)
{ reinterpret_cast<DebugTextDrawer*>(drawer)->flush(); }

// PE 0x1007CD10.
IBase* __fastcall createDebugTextDrawer(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTextDrawer));
    DebugTextDrawer* self = storage ? new (storage) DebugTextDrawer() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(self->table[3])(self, iid);
    if (!result) drawer_release(self RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}
} } }
#endif
