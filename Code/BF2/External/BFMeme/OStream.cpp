// Path [INFERRED]. Complete instruction reads837FD0..838397, table9352A0.
#include "OStream.h"
#if !DEDICATED
#include "System.h"
#include "NamedObjectRegistry.h"
#include "TypeDescriptor.h"
#include <cstring>
#include <cwchar>
#if !defined(_WIN32)
#include <vector>
#endif
namespace dice { namespace meme {
namespace {
// [INFERRED] Factored identical resource-name write sequences838190/838210/838290.
bool writeResourceName(OStream* output, const char* name)
{
    unsigned char length = 0;
    if (!name) return output->streamUByte(&length, "Ubyte") != false;
    length = static_cast<unsigned char>(std::strlen(name));
    if (!output->streamUByte(&length, "Ubyte")) return false;
    return output->write(name, length) == length; // also called for nonnull empty names
}
}
OStream::~OStream() {}
// 8383E0: guarded "dice::meme::OStream" descriptor, base slot 82B9E0 (RuntimeObject), factory
// 838420 (four bytes, table 9352A0), size slot 8149E0 (4).
TypeDescriptor* OStream::getClassType()
{
    static ClassTypeDescriptor<OStream, RuntimeObject> type("dice::meme::OStream");
    return &type;
}
TypeDescriptor* OStream::getType() { return getClassType(); }
unsigned int OStream::getObjectSize() { return sizeof(OStream); }
bool OStream::streamUByte(unsigned char* value, const char*) { return write(value, 1) == 1; }
bool OStream::streamByte(char* value, const char*) { return write(value, 1) == 1; }
bool OStream::streamUShort(unsigned short* value, const char*) { return write(value, 2) == 2; }
bool OStream::streamShort(short* value, const char*) { return write(value, 2) == 2; }
bool OStream::streamULong(unsigned long* value, const char*) { return write(value, 4) == 4; }
bool OStream::streamLong(long* value, const char*) { return write(value, 4) == 4; }
bool OStream::streamFloat(float* value, const char*) { return write(value, 4) == 4; }
bool OStream::streamBool(bool* value, const char* label)
{
    unsigned char byte = *reinterpret_cast<unsigned char*>(value) ? 1 : 0;
    return streamUByte(&byte, label) != false;
}
bool OStream::streamInt(int* value, const char* label)
{
    long temporary = *value;
    return streamLong(&temporary, label) != false;
}
bool OStream::streamString(char** value, const char*) // 838090, shipped comparison defect
{
    int length = *value ? static_cast<int>(std::strlen(*value)) : 0;
    if (!streamInt(&length, "Int")) return false;
    if (*value && static_cast<int>(write(*value, length) == 0) == length) return false;
    return true;
}
bool OStream::streamWideString(wchar_t** value, const char*)
{
    int length = *value ? static_cast<int>(std::wcslen(*value)) : 0;
    if (!streamInt(&length, "Int")) return false;
#if defined(_WIN32)
    if (*value && write(*value, 2 * length) != 2 * length) return false;
#else
    // [CHOSEN, not recovered] Keep the Windows wire format: `length` UTF-16LE units. Writing
    // 2 * length raw bytes of a 32-bit POSIX wchar_t buffer would emit half the text, mangled.
    // Characters above U+FFFF keep their low 16 bits, as the 16-bit getCharacter does.
    if (*value) {
        std::vector<unsigned char> units(2u * static_cast<unsigned int>(length));
        for (int i = 0; i < length; ++i) {
            const unsigned int unit = static_cast<unsigned int>((*value)[i]) & 0xFFFFu;
            units[2 * i] = static_cast<unsigned char>(unit);
            units[2 * i + 1] = static_cast<unsigned char>(unit >> 8);
        }
        if (write(units.data(), 2 * length) != 2 * length) return false;
    }
#endif
    return true;
}
bool OStream::streamCharacter(wchar_t* value, const char* label) // 838170 uses32-bit serialization
{
    int temporary = *value;
    return streamInt(&temporary, label);
}
bool OStream::streamTexture(System* system, TextureHandle* texture, const char*)
{ return writeResourceName(this, system->getTextureName(*texture)); }
bool OStream::streamFont(System* system, FontHandle* font, const char*)
{ return writeResourceName(this, system->getFontName(*font)); }
bool OStream::streamSound(System* system, int* sound, const char*)
{ return writeResourceName(this, system->getSoundName(*sound)); }
bool OStream::streamCollection(System* system, ObjectCollection* collection, const char*)
{ return collection->write(system, this); }
bool OStream::streamEnum(int* value, EnumNext, EnumName, const char* label)
{ return streamInt(value, label); }
bool OStream::streamObject(System* system, Object*& value, const char*, TypeDescriptor*)
{ return writeObject(system, value, this); }
int OStream::write(const void*, int) { return 0; } // 6212F0, real base default
void OStream::flush() {} // 83D270, real base default
int OStream::getPosition() { return 0; }
bool OStream::setPosition(int) { return false; }
bool OStream::writeShortString(System*, const char* value, const char*)
{
    unsigned char length = value ? static_cast<unsigned char>(std::strlen(value)) : 0;
    if (!streamUByte(&length, "Ubyte")) return false;
    if (!length) return true;
    return write(value, length) == length;
}
bool writeObject(System* system, Object*& value, OStream* output)
{
    const int start = output->getPosition();
    unsigned long size = 0;
    if (!output->streamULong(&size, "Ulong")) return false;
    if (!value) {
        if (!output->writeShortString(system, nullptr, "Object name")) return false;
        if (!output->writeShortString(system, nullptr, "Class name")) return false;
    } else {
        char name[256];
        system->m_registry->objectPath(value, name, 256); // return ignored, no registry guard
        if (!output->writeShortString(system, name, "Object name")) return false;
        if (name[0] && value->slot_0x20()) {
            if (!output->writeShortString(system, nullptr, "Class name")) return false;
        } else {
            const char* className = value->getType()->getName();
            if (!output->writeShortString(system, className, "Class name")) return false;
            if (!value->visit0x38(system, output)) {
                systemPrintf(system, "Meme: Error: Writing to ostream \"%s\"\r\n", className);
                return false;
            }
        }
    }
    const int end = output->getPosition();
    output->setPosition(start); // both seek results are ignored in the PE
    size = static_cast<unsigned long>(end - start);
    if (!output->streamULong(&size, "Ulong")) return false; // leaves the rewound position
    output->setPosition(end);
    return true;
}
// Descriptor is intentionally unpublished while deeper dependencies stay open.
#if defined(_M_IX86)
static_assert(sizeof(OStream) == 4, "BFMeme output stream layout");
#endif
} }
#endif
