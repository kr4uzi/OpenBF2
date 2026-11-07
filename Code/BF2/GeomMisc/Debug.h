// Renderer-local diagnostic stream and host-array binding.
// [INFERRED] Source placement; the DLL contains its own Core Debug implementation.
#ifndef GEOMMISC_DEBUG_H
#define GEOMMISC_DEBUG_H

#include "../Core/Debug.h"

namespace renddx9abi {

// [CHOSEN, not recovered] Image-local spelling prevents collisions with Core/Debug in
// the statically linked macOS client. State still comes from the four host singletons.
class Debug
{
public:
    Debug(dice::hfe::DebugType type, const std::string& file, int line,
          const std::string& expression);
    ~Debug();

    template <typename T> Debug& operator<<(const T& value)
    {
        m_stream << value;
        return *this;
    }

private:
    dice::hfe::DebugType m_type; // PE +0x00
    std::string m_file;         // PE +0x04
    std::string m_expression;   // PE +0x20
    int m_line;                // PE +0x3C
    std::ostringstream m_stream; // PE +0x40
};

} // namespace renddx9abi
#endif
