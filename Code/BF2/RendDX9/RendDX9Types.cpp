// D3D format sizing used by texture loading.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/Frontier.h"
#include "Texture.h"
#include <ostream>

// PE 0x1000CB80. File-local placement keeps the D3D enum formatter visible to
// the diagnostic template without exposing a new renderer interface.
static std::ostream& operator<<(std::ostream& out, D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_X1R5G5B5: out << "X1R5G5B5"; break;
    case D3DFMT_A8R8G8B8: out << "A8R8G8B8"; break;
    case D3DFMT_A1R5G5B5: out << "A1R5G5B5"; break;
    case D3DFMT_DXT1: case D3DFMT_DXT2: out << "DXT1/2"; break;
    case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5: out << "DXT3/4/5"; break;
    default: out << "Unknown type"; break;
    }
    return out;
}

#include "../GeomMisc/Debug.h"

namespace dice {
namespace hfe {
namespace dx9rend {

// 1000CC30, read in full. Input is the already-truncated D3D declaration type.
// The invalid-type diagnostic uses the existing renderer log adapter, as below.
int vertexDeclarationTypeSize(unsigned int type)
{
    switch (type)
    {
    case 0: case 4: case 5: case 6: case 8: case 9: case 11:
    case 13: case 14: case 15: return 4;
    case 1: case 7: case 10: case 12: case 16: return 8;
    case 2: return 12;
    case 3: return 16;
    case 17: return 0;
    default:
        ::renddx9abi::logf("RendDX9Types.cpp:93: Unknown type: %u", type);
        return 0;
    }
}


// Returns the format's legacy bits-per-pixel value, or -1 for unsupported formats.
// X1R5G5B5 and G32R32F are unsupported; D24X8 reports 24 despite 32-bit storage.
// DXT1 reports 4 and DXT3/DXT5 report 8 for mip-size calculations.

// PE 0x1000CD30
int bitsPerPixel(unsigned int d3dFormat)
{
    switch (d3dFormat)
    {
        case 36:  return 64;    // A16B16G16R16
        case 20:  return 24;    // R8G8B8
        case 21:                // A8R8G8B8
        case 22:  return 32;    // X8R8G8B8
        case 23:                // R5G6B5
        case 25:                // A1R5G5B5
        case 26:  return 16;    // A4R4G4B4     (24, X1R5G5B5, is deliberately absent)
        case 28:  return 8;     // A8

        case 41:  return 8;     // P8
        case 63:  return 32;    // Q8W8V8U8
        case 75:  return 32;    // D24S8
        case 77:  return 24;    // D24X8 - the shipped answer, shared with R8G8B8's arm
        case 80:  return 16;    // D16
        case 101: return 16;    // INDEX16
        case 102: return 32;    // INDEX32
        case 111: return 16;    // R16F

        case 112: return 32;    // G16R16F
        case 113: return 64;    // A16B16G16R16F
        case 114: return 32;    // R32F         (115, G32R32F, is deliberately absent)
        case 116: return 128;   // A32B32G32R32F

        case 827611204: return 4;   // 'DXT1'
        case 861165636:             // 'DXT3'
        case 894720068: return 8;   // 'DXT5'

        default:
        {
            ::renddx9abi::Debug diagnostic(dice::hfe::DebugTypeError,
                "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\RendDX9Types.cpp", 151, "");
            diagnostic << "Unknown format " << static_cast<D3DFORMAT>(d3dFormat);
            return -1;
        }
    }
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
