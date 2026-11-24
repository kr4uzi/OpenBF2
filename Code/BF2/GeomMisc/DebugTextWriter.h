// Typed renderer-local access to the debug-text services.
#ifndef GEOMMISC_DEBUGTEXTWRITER_H
#define GEOMMISC_DEBUGTEXTWRITER_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"
#include <string>
#include <vector>

namespace dice { namespace hfe { namespace geom {

// Queued debug text; preserve the renderer's virtual order and nonvirtual destruction.
// RendDX9 PE table 0x1020CD10; ObjectManager PE 0x00726140 uses queueText at +0x40.
class IDebugTextWriter : public IBase
{
public:
    virtual bool setFont(const std::string& path) = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual unsigned getColour() const = 0;
    virtual void setColour(unsigned colour) = 0;
    virtual void queueWorldGroupColoured(const Vec3&, unsigned, const std::vector<std::string>&,
                                          int spacing, int alignment, int bottomAligned) = 0;
    virtual void queueWorldTextColoured(const Vec3&, unsigned, const std::string&, int alignment) = 0;
    virtual void queueGroupColoured(int x, int y, unsigned, const std::vector<std::string>&,
                                     int spacing, int alignment, int bottomAligned) = 0;
    virtual void queueTextColoured(int x, int y, unsigned, const std::string&, int alignment) = 0;
    virtual void queueWorldGroup(const Vec3&, const std::vector<std::string>&,
                                  int spacing, int alignment, int bottomAligned) = 0;
    virtual void queueWorldText(const Vec3&, const std::string&, int alignment) = 0;
    virtual void queueGroup(int x, int y, const std::vector<std::string>&,
                             int spacing, int alignment, int bottomAligned) = 0;
    virtual void queueText(int x, int y, const std::string&, int alignment) = 0;
    virtual void draw() = 0;
    virtual void clear() = 0;
};

extern IBase* g_DebugTextWriter; // PE 0x10247C50
void debugTextWriterManagerDraw(IBase* manager);
unsigned debugTextWriterGetColour(IBase* writer);
void debugTextWriterSetColour(IBase* writer, unsigned colour);
void debugTextWriterQueueText(IBase* writer, int x, int y, const std::string& text, int alignment);
void debugTextWriterQueueWorldText(IBase* writer, const Vec3& position, unsigned colour,
                                    const std::string& text, int alignment);

} } }
#endif
