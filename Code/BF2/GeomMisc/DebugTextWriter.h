// Typed renderer-local access to the debug-text services.
#ifndef GEOMMISC_DEBUGTEXTWRITER_H
#define GEOMMISC_DEBUGTEXTWRITER_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"
#include <string>

namespace dice { namespace hfe { namespace geom {

extern IBase* g_DebugTextWriter; // PE 0x10247C50
void debugTextWriterManagerDraw(IBase* manager);
unsigned debugTextWriterGetColour(IBase* writer);
void debugTextWriterSetColour(IBase* writer, unsigned colour);
void debugTextWriterQueueText(IBase* writer, int x, int y, const std::string& text, int alignment);
void debugTextWriterQueueWorldText(IBase* writer, const Vec3& position, unsigned colour,
                                    const std::string& text, int alignment);

} } }
#endif
