// [CHOSEN, not recovered] Private clear/present quads. No D3D draw state is changed.
// Included after DrawBridge.inc for the per-device library and pipeline caches.
static id<MTLLibrary> menuLibrary(Frame* f) {
    static const char source[]=R"(
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; float2 uv; };
vertex V menuVertex(uint i [[vertex_id]], constant float& z [[buffer(0)]]) {
    const float2 p[4]={float2(-1,1),float2(1,1),float2(-1,-1),float2(1,-1)};
    V o; o.position=float4(p[i],z,1); o.uv=p[i]*float2(.5,-.5)+.5; return o;
}
fragment float4 menuClear(constant float4& c [[buffer(0)]]) { return c; }
fragment float4 menuPresent(V v [[stage_in]], texture2d<float> t [[texture(0)]]) {
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);
    return t.sample(s,v.uv);
}
fragment float4 menuCopy(V v [[stage_in]], texture2d<float> t [[texture(0)]], constant int4& o [[buffer(0)]]) {
    const float4 c = t.read(uint2(int2(v.position.xy) + o.xy));
    return o.z ? float4(c.rgb, 1) : c;
}
)";
    if(!f->draws) f->draws=new DrawCaches;
    auto& library=f->draws->libraries[source];
    if(!library) {
        NSError* error=nil;
        library=[f->device newLibraryWithSource:[NSString stringWithUTF8String:source] options:nil error:&error];
        if(!library) {
            std::fprintf(stderr,"[FRONTIER] IDirect3DDevice9::Clear/Present shader compile: %s\n",error.localizedDescription.UTF8String);
            std::fflush(stderr); std::abort();
        }
    }
    return library;
}
static id<MTLRenderPipelineState> menuPipeline(Frame* f,MTLPixelFormat color,MTLPixelFormat depth,
                                               unsigned flags,bool presenting,unsigned samples=1,bool copying=false) {
    id<MTLLibrary> library=menuLibrary(f);
    std::string key="menu"; keyAppend(key,color); keyAppend(key,depth); keyAppend(key,flags); keyAppend(key,presenting);
    keyAppend(key,samples); keyAppend(key,copying);
    auto& pipeline=f->draws->pipelines[key];
    if(!pipeline) {
        auto* p=[MTLRenderPipelineDescriptor new];
        p.rasterSampleCount=samples;
        p.vertexFunction=[library newFunctionWithName:@"menuVertex"];
        // Always bind a fragment function: on Intel Iris 655 a depth/stencil-only pipeline without
        // one ignored the scissor rectangle and cleared the whole attachment (menu.inc Z-only Clear).
        p.fragmentFunction=[library newFunctionWithName:copying?@"menuCopy":presenting?@"menuPresent":@"menuClear"];
        p.colorAttachments[0].pixelFormat=color;
        p.colorAttachments[0].writeMask=(flags&1)?MTLColorWriteMaskAll:MTLColorWriteMaskNone;
        p.depthAttachmentPixelFormat=depth;
        p.stencilAttachmentPixelFormat=depthHasStencil(depth)?depth:MTLPixelFormatInvalid;
        NSError* error=nil;
        pipeline=[f->device newRenderPipelineStateWithDescriptor:p error:&error];
        if(!pipeline) {
            std::fprintf(stderr,"[FRONTIER] IDirect3DDevice9::Clear/Present pipeline: %s\n",error.localizedDescription.UTF8String);
            std::fflush(stderr); std::abort();
        }
    }
    return pipeline;
}
// [CHOSEN, not recovered] Copies the single-sample back buffer into every sample of its
// multisampled twin, after a CPU or blit write into the back buffer (ResourceBridge.inc).
static bool seedBackBufferSamples(Frame* f) {
    if(!commands(f)) return false;
    auto pipeline=menuPipeline(f,f->colorMS.pixelFormat,MTLPixelFormatInvalid,1,true,f->samples,true);
    auto* pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=f->colorMS;
    pass.colorAttachments[0].loadAction=MTLLoadActionDontCare; pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    auto e=[f->commands renderCommandEncoderWithDescriptor:pass];
    if(!e) return false;
    const float z=0;
    [e setRenderPipelineState:pipeline]; [e setVertexBytes:&z length:sizeof(z) atIndex:0];
    [e setViewport:(MTLViewport){0,0,double(f->color.width),double(f->color.height),0,1}];
    const int offset[4]={0,0,0,0}; [e setFragmentBytes:offset length:sizeof(offset) atIndex:0];
    [e setFragmentTexture:f->color atIndex:0];
    [e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4]; [e endEncoding];
    f->unresolved=false;
    return true;
}
bool copyColor(Frame* f,Texture* s,unsigned sx,unsigned sy,Texture* d,unsigned dl,unsigned ds,
               unsigned dx,unsigned dy,unsigned w,unsigned h,bool opaque) {
    mainThread("IDirect3DDevice9::StretchRect");
    @autoreleasepool {
        if(d->object.sampleCount>1) frontier("IDirect3DDevice9::StretchRect into a multisampled surface");
        if(!(d->object.usage&MTLTextureUsageRenderTarget) || s->object.textureType==MTLTextureTypeCube ||
            s->object.textureType==MTLTextureType3D || d->object.textureType==MTLTextureType3D) return false;
        if(s->object==f->color && !resolveBackBuffer(f)) return false;
        id<MTLTexture> source=s->object.sampleCount>1?resolvedCopy(f,s->object):s->object;
        if(!source || !(source.usage&MTLTextureUsageShaderRead) || !commands(f)) return false;
        const unsigned samples=colorSamples(f,d->object);
        auto pipeline=menuPipeline(f,d->object.pixelFormat,MTLPixelFormatInvalid,1,true,samples,true);
        auto* pass=[MTLRenderPassDescriptor renderPassDescriptor];
        auto* a=pass.colorAttachments[0]; attachColor(f,a,d->object); a.level=dl; a.slice=ds;
        a.loadAction=MTLLoadActionLoad;
        auto e=[f->commands renderCommandEncoderWithDescriptor:pass];
        if(!e) return false;
        const float z=0;
        const int offset[4]={int(sx)-int(dx),int(sy)-int(dy),opaque?1:0,0};
        [e setRenderPipelineState:pipeline]; [e setVertexBytes:&z length:sizeof(z) atIndex:0];
        [e setViewport:(MTLViewport){double(dx),double(dy),double(w),double(h),0,1}];
        [e setScissorRect:(MTLScissorRect){dx,dy,w,h}];
        [e setFragmentBytes:offset length:sizeof(offset) atIndex:0];
        [e setFragmentTexture:source atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4]; [e endEncoding];
        return true;
    }
}
bool clearTargets(Frame* f,Texture* color,unsigned cl,unsigned cs,Texture* depth,unsigned dl,unsigned ds,
        unsigned flags,std::uint32_t value,float z,unsigned stencil,const ClearRect* rects,std::size_t count) {
    mainThread("IDirect3DDevice9::Clear");
    @autoreleasepool {
        if(!count) return true;
        if((flags&1) && (flags&6) && samplesMismatch(f,color->object,depth->object))
            return clearTargets(f,color,cl,cs,depth,dl,ds,flags&1,value,z,stencil,rects,count) &&
                clearTargets(f,color,cl,cs,depth,dl,ds,flags&6,value,z,stencil,rects,count);
        const unsigned w=std::max(1u,unsigned(color->object.width)>>cl),h=std::max(1u,unsigned(color->object.height)>>cl);
        // A Metal loadAction clear ignores scissor. Use it only for whole attachments.
        if(count==1 && !rects[0].x && !rects[0].y && rects[0].width==w && rects[0].height==h &&
            (!(flags&6) || (std::max(1u,unsigned(depth->object.width)>>dl)==w && std::max(1u,unsigned(depth->object.height)>>dl)==h)))
            return fullClearTargets(f,color,cl,cs,depth,dl,ds,flags,value,z,stencil);
        if(!commands(f)) return false;
        const MTLPixelFormat cf=(flags&1)?color->object.pixelFormat:MTLPixelFormatInvalid;
        const MTLPixelFormat df=(flags&6)?depth->object.pixelFormat:MTLPixelFormatInvalid;
        const unsigned samples=targetSamples(f,(flags&1)?color->object:nil,(flags&6)?depth->object:nil);
        auto pipeline=menuPipeline(f,cf,df,flags,false,samples);
        auto* pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.renderTargetWidth=w; pass.renderTargetHeight=h;
        if(flags&1) {
            auto* a=pass.colorAttachments[0]; attachColor(f,a,color->object); a.level=cl; a.slice=cs;
            a.loadAction=MTLLoadActionLoad;
        }
        if(flags&6) {
            auto* a=pass.depthAttachment; a.texture=depth->object; a.level=dl; a.slice=ds;
            a.loadAction=MTLLoadActionLoad; a.storeAction=MTLStoreActionStore;
            if(depthHasStencil(df)) {
                auto* s=pass.stencilAttachment; s.texture=depth->object; s.level=dl; s.slice=ds;
                s.loadAction=MTLLoadActionLoad; s.storeAction=MTLStoreActionStore;
            }
        }
        auto* desc=[MTLDepthStencilDescriptor new];
        desc.depthCompareFunction=MTLCompareFunctionAlways; desc.depthWriteEnabled=(flags&2)!=0;
        if(flags&4) {
            auto* s=[MTLStencilDescriptor new]; s.stencilCompareFunction=MTLCompareFunctionAlways;
            s.depthStencilPassOperation=MTLStencilOperationReplace; s.writeMask=255;
            desc.frontFaceStencil=desc.backFaceStencil=s;
        }
        auto e=[f->commands renderCommandEncoderWithDescriptor:pass];
        if(!e) return false;
        [e setRenderPipelineState:pipeline]; [e setDepthStencilState:[f->device newDepthStencilStateWithDescriptor:desc]];
        [e setViewport:(MTLViewport){0,0,double(w),double(h),0,1}];
        [e setStencilReferenceValue:stencil&255];
        // Match the full-clear path's D3D-format quantization, including native expanded formats.
        double channels[4]={((value>>16)&255)/255.0,((value>>8)&255)/255.0,(value&255)/255.0,(value>>24)/255.0};
        if(flags&1) {
            unsigned char packed[16]={},native[16]={}; encodeNative(color->format,native,channels);
            convertPixel(color->format,packed,native,false); convertPixel(color->format,packed,native,true);
            decodeNative(color->format,native,channels);
        }
        const float rgba[4]={float(channels[0]),float(channels[1]),float(channels[2]),float(channels[3])};
        const float depthValue=(flags&2)?(depth->format==75?float(std::uint32_t(z*16777215.0+0.5)/16777215.0):z):0;
        [e setVertexBytes:&depthValue length:sizeof(depthValue) atIndex:0];
        [e setFragmentBytes:rgba length:sizeof(rgba) atIndex:0];
        for(std::size_t i=0;i<count;++i) {
            const auto& r=rects[i]; [e setScissorRect:(MTLScissorRect){r.x,r.y,r.width,r.height}];
            [e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        }
        [e endEncoding]; return true;
    }
}
