// [CHOSEN, not recovered] D3D9 menu contracts, independent of GPU submission.
#pragma once
#include "../D3D9Subset.h"
#include "MetalBridge.h"
#include <algorithm>
#include <vector>

namespace bf2metal {
inline HRESULT presentParameters(D3DSWAPEFFECT swap, const RECT* source, const RECT* dest,
                                  const RGNDATA* dirty, DWORD flags) {
    // The native device currently creates DISCARD chains only. Rectangles/dirty
    // regions require COPY; LINEAR_CONTENT is accepted but our caps do not enable it.
    if ((flags & ~3u) || (swap != D3DSWAPEFFECT_COPY && (source || dest || dirty)))
        return D3DERR_INVALIDCALL;
    return D3D_OK;
}
inline std::vector<ClearRect> clearRectangles(const D3DVIEWPORT9& v, unsigned w, unsigned h,
        DWORD count, const D3DRECT* rects, const RECT* scissor) {
    using I = std::int64_t;
    const I left=v.X, top=v.Y;
    const I right=std::min<I>(w,I(v.X)+v.Width), bottom=std::min<I>(h,I(v.Y)+v.Height);
    std::vector<ClearRect> result;
    auto clip = [&](I l,I t,I r,I b) {
        l=std::max(l,left); t=std::max(t,top); r=std::min(r,right); b=std::min(b,bottom);
        if(scissor) { l=std::max<I>(l,scissor->left); t=std::max<I>(t,scissor->top);
            r=std::min<I>(r,scissor->right); b=std::min<I>(b,scissor->bottom); }
        if(l<r && t<b) result.push_back({unsigned(l),unsigned(t),unsigned(r-l),unsigned(b-t)});
    };
    if(!count) clip(left,top,right,bottom);
    else for(DWORD i=0;i<count;++i) clip(rects[i].x1,rects[i].y1,rects[i].x2,rects[i].y2);
    return result;
}
}
