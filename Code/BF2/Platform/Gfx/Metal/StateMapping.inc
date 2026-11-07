// [CHOSEN, not recovered] D3D9 values mapped to Metal enums, without GPU submission.
static float stateFloat(std::uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
static unsigned blendFactor(unsigned v) {
    switch (v) {
    case 1: return MTLBlendFactorZero;
    case 2: return MTLBlendFactorOne;
    case 3: return MTLBlendFactorSourceColor;
    case 4: return MTLBlendFactorOneMinusSourceColor;
    case 5: return MTLBlendFactorSourceAlpha;
    case 6: return MTLBlendFactorOneMinusSourceAlpha;
    case 7: return MTLBlendFactorDestinationAlpha;
    case 8: return MTLBlendFactorOneMinusDestinationAlpha;
    case 9: return MTLBlendFactorDestinationColor;
    case 10: return MTLBlendFactorOneMinusDestinationColor;
    case 11: return MTLBlendFactorSourceAlphaSaturated;
    case 14: return MTLBlendFactorBlendColor;
    case 15: return MTLBlendFactorOneMinusBlendColor;
    default: return ~0u;
    }
}
static unsigned stencilOp(unsigned v) {
    static const unsigned table[] = {~0u, MTLStencilOperationKeep, MTLStencilOperationZero,
        MTLStencilOperationReplace, MTLStencilOperationIncrementClamp, MTLStencilOperationDecrementClamp,
        MTLStencilOperationInvert, MTLStencilOperationIncrementWrap, MTLStencilOperationDecrementWrap};
    return v < 9 ? table[v] : ~0u;
}
static bool compare(unsigned v) { return v >= 1 && v <= 8; }
const char* mapState(const RawState& s, StateMapping& m)
{
    m = {};
    const auto* r = s.render;
    auto fail = [](const char* name) { return name; };
    if (r[7] > 1) return fail("IDirect3DDevice9::Draw state D3DRS_ZENABLE (W buffer)");
    if (!compare(r[23])) return fail("IDirect3DDevice9::Draw state D3DRS_ZFUNC");
    m.depthCompare = r[7] ? r[23]-1 : MTLCompareFunctionAlways;
    m.depthWrite = r[7] && r[14];
    m.blend = r[27] != 0;
    m.srcRGB = blendFactor(r[19]); m.dstRGB = blendFactor(r[20]);
    m.srcAlpha = blendFactor(r[206] ? r[207] : r[19]);
    m.dstAlpha = blendFactor(r[206] ? r[208] : r[20]);
    // Constant-color factors use the constant alpha in the alpha channel.
    if ((r[206] ? r[207] : r[19]) == 14) m.srcAlpha = MTLBlendFactorBlendAlpha;
    if ((r[206] ? r[207] : r[19]) == 15) m.srcAlpha = MTLBlendFactorOneMinusBlendAlpha;
    if ((r[206] ? r[208] : r[20]) == 14) m.dstAlpha = MTLBlendFactorBlendAlpha;
    if ((r[206] ? r[208] : r[20]) == 15) m.dstAlpha = MTLBlendFactorOneMinusBlendAlpha;
    if (m.srcRGB == ~0u || m.dstRGB == ~0u || m.srcAlpha == ~0u || m.dstAlpha == ~0u)
        return fail("IDirect3DDevice9::Draw state blend factor");
    if (r[171] < 1 || r[171] > 5 || (r[206] && (r[209] < 1 || r[209] > 5)))
        return fail("IDirect3DDevice9::Draw state blend operation");
    m.blendRGB = r[171]-1; m.blendAlpha = (r[206] ? r[209] : r[171])-1;
    if (r[22] < 1 || r[22] > 3) return fail("IDirect3DDevice9::Draw state D3DRS_CULLMODE");
    // G4 must use clockwise front winding after its D3D clip/viewport transform.
    m.cull = r[22] == 1 ? MTLCullModeNone : r[22] == 2 ? MTLCullModeFront : MTLCullModeBack;
    if (r[8] != 2 && r[8] != 3) return fail("IDirect3DDevice9::Draw state D3DRS_FILLMODE");
    m.fill = r[8] == 2 ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;
    if (r[168] & ~15u) return fail("IDirect3DDevice9::Draw state D3DRS_COLORWRITEENABLE");
    m.colorMask = ((r[168]&1) ? MTLColorWriteMaskRed : 0) | ((r[168]&2) ? MTLColorWriteMaskGreen : 0) |
        ((r[168]&4) ? MTLColorWriteMaskBlue : 0) | ((r[168]&8) ? MTLColorWriteMaskAlpha : 0);
    m.stencil = r[52] != 0;
    if (!compare(r[56]) || !compare(r[189])) return fail("IDirect3DDevice9::Draw state stencil compare");
    m.front = {stencilOp(r[53]),stencilOp(r[54]),stencilOp(r[55]),r[56]-1};
    m.back = r[185] ? StencilMapping{stencilOp(r[186]),stencilOp(r[187]),stencilOp(r[188]),r[189]-1} : m.front;
    for (unsigned v : {m.front.fail,m.front.depthFail,m.front.pass,m.back.fail,m.back.depthFail,m.back.pass})
        if (v == ~0u) return fail("IDirect3DDevice9::Draw state stencil operation");
    m.stencilReadMask = r[58]&255; m.stencilWriteMask = r[59]&255; m.stencilRef = r[57]&255;
    m.alphaTest = r[15] != 0;
    if (!compare(r[25])) return fail("IDirect3DDevice9::Draw state D3DRS_ALPHAFUNC");
    m.alphaCompare = r[25]-1; m.alphaRef = (r[24]&255)/255.0f;
    m.clipMask = r[152];
    if (m.clipMask & ~63u) return fail("IDirect3DDevice9::Draw state D3DRS_CLIPPLANEENABLE");
    m.scissor = r[174] != 0; m.srgbWrite = r[194] != 0;
    m.depthBias = stateFloat(r[195]); m.slopeBias = stateFloat(r[175]);
    if (!std::isfinite(m.depthBias) || !std::isfinite(m.slopeBias)) return fail("IDirect3DDevice9::Draw state depth bias");
    m.blendColor[0] = ((r[193]>>16)&255)/255.0f; m.blendColor[1] = ((r[193]>>8)&255)/255.0f;
    m.blendColor[2] = (r[193]&255)/255.0f; m.blendColor[3] = (r[193]>>24)/255.0f;
    static const unsigned addresses[] = {~0u,MTLSamplerAddressModeRepeat,MTLSamplerAddressModeMirrorRepeat,
        MTLSamplerAddressModeClampToEdge,MTLSamplerAddressModeClampToBorderColor,MTLSamplerAddressModeMirrorClampToEdge};
    for (unsigned i = 0; i < 21; ++i) {
        const auto* a = s.sampler[i]; auto& b = m.sampler[i];
        for (unsigned j=0; j<3; ++j) {
            if (a[j+1] < 1 || a[j+1] > 5) return fail("IDirect3DDevice9::Draw state sampler address");
            b.address[j] = addresses[a[j+1]];
        }
        if (a[5]<1 || a[5]>3 || a[6]<1 || a[6]>3 || a[7]>2 || a[10]<1 || a[10]>16)
            return fail("IDirect3DDevice9::Draw state sampler filter/anisotropy");
        b.magFilter = a[5] == 1 ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
        b.minFilter = a[6] == 1 ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
        b.mipFilter = a[7]; // D3D NONE/POINT/LINEAR equals Metal NotMipmapped/Nearest/Linear.
        b.anisotropy = a[6] == 3 ? a[10] : 1;
        b.minLod = float(a[9]); b.lodBias = stateFloat(a[8]); b.srgb = a[11] != 0;
        if (!std::isfinite(b.lodBias)) return fail("IDirect3DDevice9::Draw state sampler LOD bias");
        if (a[12] || a[13]) return fail("IDirect3DDevice9::Draw state sampler element/displacement");
        if (a[4] == 0) b.border = MTLSamplerBorderColorTransparentBlack;
        else if (a[4] == 0xff000000u) b.border = MTLSamplerBorderColorOpaqueBlack;
        else if (a[4] == 0xffffffffu) b.border = MTLSamplerBorderColorOpaqueWhite;
        else if (a[1] == 4 || a[2] == 4 || a[3] == 4)
            return fail("IDirect3DDevice9::Draw state sampler custom border color");
    }
    return nullptr;
}

struct VertexLayout { MTLVertexDescriptor* descriptor; std::vector<VertexElement> elements; };
static MTLVertexFormat vertexFormat(unsigned t) {
    switch (t) {
    case 0: return MTLVertexFormatFloat;
    case 1: return MTLVertexFormatFloat2;
    case 2: return MTLVertexFormatFloat3;
    case 3: return MTLVertexFormatFloat4;
    case 4: return MTLVertexFormatUChar4Normalized_BGRA;
    case 5: return MTLVertexFormatUChar4;
    case 6: return MTLVertexFormatShort2;
    case 7: return MTLVertexFormatShort4;
    case 8: return MTLVertexFormatUChar4Normalized;
    case 9: return MTLVertexFormatShort2Normalized;
    case 10: return MTLVertexFormatShort4Normalized;
    case 11: return MTLVertexFormatUShort2Normalized;
    case 12: return MTLVertexFormatUShort4Normalized;
    case 15: return MTLVertexFormatHalf2;
    case 16: return MTLVertexFormatHalf4;
    default: return MTLVertexFormatInvalid;
    }
}
VertexLayout* createLayout(const VertexElement* e, unsigned count) {
    mainThread("IDirect3DDevice9::CreateVertexDeclaration");
    @autoreleasepool {
    auto* layout = new VertexLayout;
    layout->descriptor = [MTLVertexDescriptor vertexDescriptor];
    layout->elements.assign(e,e+count);
    for (unsigned i=0; i<count; ++i) {
        const auto f = vertexFormat(e[i].type);
        if (!f || i >= 31 || e[i].stream >= 16) { delete layout; return nullptr; }
        auto* a = layout->descriptor.attributes[i];
        a.format = f; a.offset = e[i].offset; a.bufferIndex = e[i].stream+1;
        // Stream strides are supplied by SetStreamSource at draw time.
    }
    return layout;
    }
}
void destroyLayout(VertexLayout* p) { mainThread("IDirect3DVertexDeclaration9::Release"); delete p; }
unsigned layoutFormat(VertexLayout* p, unsigned i) {
    mainThread("IDirect3DVertexDeclaration9::GetDeclaration");
    return i < p->elements.size() ? unsigned(p->descriptor.attributes[i].format) : 0;
}
