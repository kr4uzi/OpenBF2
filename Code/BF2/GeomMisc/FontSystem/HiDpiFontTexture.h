#ifndef BF2_HIDPIFONTTEXTURE_H
#define BF2_HIDPIFONTTEXTURE_H

#include <string>

namespace dice { namespace hfe { namespace geom {

// 1 transfers an atlas reference. 0 requests a retry when more SWF outlines arrive.
// -1 keeps the shipped atlas.
// error (optional): in, the fit error of the atlas the caller already holds, which a new atlas must
// beat (0 is returned otherwise); out, the fit error of the atlas returned.
int hidpiBuildFontTexture(const std::string& base, const char* exportName, int scale, void** outTexture,
                          float* error = nullptr);
// A fit error above this is accepted, but the font is fitted again when more SWF outlines arrive:
// the typeface it was drawn from may not be loaded yet.
const float kHiDpiConfidentError = 0.05f;
void hidpiAddPending(void* owner, void (*retry)(void*));
void hidpiRemovePending(void* owner);

} } }

#endif
