// [INFERRED] private implementation split for the identical sprite/NSA texture-load sequence.
// BF2 1.50 RendDX9 PE 10176C20 / 101784E0; no corresponding dedicated ELF renderer body.
#ifndef BF2_GEOM_PARTICLESYSTEM_PARTICLETEMPLATERESOURCES_H
#define BF2_GEOM_PARTICLESYSTEM_PARTICLETEMPLATERESOURCES_H

#include "../../RendDX9/Texture.h"
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../GeomMisc/Debug.h"
#include <cstdio>
#include <cstdlib>

namespace dice { namespace hfe { namespace geom {

// Manager an atlas-owned region, with no added reference for the caller.
dx9rend::TextureAtlasEntry* particleSystemFindTextureRegion(const std::string& name);

// [CHOSEN helper factoring, recovered body] Both retail loaders reject an empty C string before
// releasing the old direct texture or replacing the region. The atlas success arm leaves the
// direct texture null: the region's page owns the texture used by rendering.
// PE 0x10176C20
// PE 0x101784E0
inline bool particleTemplateLoadTexture(dx9rend::EngineTexture*& texture,
                                       dx9rend::TextureAtlasEntry*& region,
                                       const std::string& name,
                                       const char* diagnosticFile, unsigned int diagnosticLine)
{
    // The retail comparison uses _stricmp against "", so a leading NUL also refuses.
    if (name.c_str()[0] == '\0')
        return false;

    dx9rend::textureReleaseRef(texture);
    region = particleSystemFindTextureRegion(name);
    if (region)
        return true;

    std::string path(name);
    path += ".dds";
    if (::renddx9abi::engineFileExists(path))
    {
        dx9rend::EngineTexture* const loaded =
            static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(name));
        // PE 1014DFD0 adopts the loader's reference after releasing the slot. Keep the second
        // release even though the earlier release normally made it null: the load may call out.
        dx9rend::textureReleaseRef(texture);
        texture = loaded;
    }
    else
    {
        // PE 0x10176D68 / 0x10178628.
        ::renddx9abi::Debug(DebugTypeError, diagnosticFile, diagnosticLine, "")
            << "File " << name << " not found!";
    }
    return texture != 0;
}

} } }
#endif
