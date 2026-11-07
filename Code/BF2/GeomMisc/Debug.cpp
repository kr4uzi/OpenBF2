// Renderer diagnostic filtering and callback dispatch. [INFERRED] Source placement.
#ifdef BF2_IMAGE_RENDDX9

#include "Debug.h"
#include "../Core/Hash.h"
#if defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#else
#include <csignal>
#include <cstdio>
#endif

namespace {

void debugBreak()
{
#if defined(_WIN32)
    __debugbreak();
#else
    // [CHOSEN, not recovered] POSIX equivalent of the PE's INT3.
    std::raise(SIGTRAP);
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86)
#define RD9_DEBUG_CALLBACK_CC __fastcall
#else
#define RD9_DEBUG_CALLBACK_CC
#endif

// PE 0x10074360 (type/module in ECX/EDX, six stack arguments, `retn 18h`). The fallback when the host
// registered no callback for the type: formats the line, and for Assert/Error shows the assertion
// dialog. It does not trap itself; ~Debug traps after it returns. The callback's unused basename and
// context remain in the host signature.
void RD9_DEBUG_CALLBACK_CC internalDebugCallback(dice::hfe::DebugType type, const std::string& module,
                           const std::string&, int line, const std::string& expression,
                           const std::string& text, const std::string& file, void*)
{
    using namespace dice::hfe;
    std::string name;
    switch (type)
    {
    case DebugTypeDebug: name = "Debug"; break;
    case DebugTypeInfo: name = "Info"; break;
    case DebugTypeWarning: name = "Warning"; break;
    case DebugTypeAssert: name = "Assert"; break;
    case DebugTypeDxAssert: name = "DxAssert"; break;
    case DebugTypeError: name = "Error"; break;
    case DebugTypeLog: name = "Log"; break;
    default: name = "(unknown)"; break;
    }
    std::ostringstream formatted;
    formatted << file << '(' << line << "): " << name << ": " << module << ": ";
    if (type == DebugTypeAssert)
        formatted << "(" << expression << ") ";   // the C-string inserter, not the char one
    formatted << text << std::endl;
    if (type == DebugTypeAssert || type == DebugTypeError)
    {
        std::ostringstream unused;
        std::ostringstream message;
        message << "Debug assertion failed!" << std::endl << std::endl
                << "Module: " << module << std::endl
                << "File: " << file << std::endl
                << "Line: " << line << std::endl << std::endl;
        if (!expression.empty()) message << "Expression: " << expression << std::endl;
        if (!text.empty()) message << "Text: " << text << std::endl;
        message << std::endl;
#if defined(_WIN32)
        MessageBoxA(0, message.str().c_str(), "BF2 Error", 0x16);
#else
        // [CHOSEN, not recovered] No USER32 modal dialog on POSIX; the message goes to stderr.
        // This path is used only when the host has no registered callback.
        std::fputs(message.str().c_str(), stderr);
        std::fflush(stderr);
#endif
    }
}

} // namespace

namespace renddx9abi {

// PE 0x10074300.
Debug::Debug(dice::hfe::DebugType type, const std::string& file, int line,
             const std::string& expression)
    : m_type(type), m_file(file), m_expression(expression), m_line(line)
{
}

// PE 0x10074690. Host arrays are bound by initDll, never owned by this image.
Debug::~Debug()
{
    using namespace dice::hfe;
    bool forceBreak = false;
    if (m_type == DebugTypeError2) { m_type = DebugTypeError; forceBreak = true; }
    else if (m_type == DebugTypeAssert2) { m_type = DebugTypeAssert; forceBreak = true; }

    const std::string::size_type slash = m_file.rfind('\\');
    const std::string basename = m_file.substr(slash + 1);
    const std::string::size_type parent = m_file.rfind('\\', slash - 1);
    const std::string module = m_file.substr(parent + 1, slash - parent - 1);
    if (debugOn[m_type] && debugModules[m_type].exists(module))
        return;
    if (debugOn[m_type] || debugModules[m_type].exists(module))
    {
        if (bfGlobal_callback[m_type])
        {
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(BF2_PLUGIN_ABI_NATIVE)
            // PE 0x100747D0: type/module in ECX/EDX, six remaining arguments on the stack.
            typedef void (__fastcall* Callback)(DebugType, const std::string&, const std::string&,
                int, const std::string&, const std::string&, const std::string&, void*);
            reinterpret_cast<Callback>(bfGlobal_callback[m_type])(
                m_type, module, basename, m_line, m_expression, m_stream.str(), m_file,
                bfGlobal_callbackData[m_type]);
#else
            // [CHOSEN, not recovered] Core/Debug.h's native host callback ABI (issue #7),
            // including native-host MSVC x86 builds. Retail ABI builds retain fastcall above.
            bfGlobal_callback[m_type](m_type, module, basename, m_line, m_expression,
                                      m_stream.str(), m_file, bfGlobal_callbackData[m_type]);
#endif
        }
        else
            internalDebugCallback(m_type, module, basename, m_line, m_expression, m_stream.str(),
                                  m_file, bfGlobal_callbackData[m_type]);
        if ((m_type != DebugTypeLog && m_type != DebugTypeInfo) || forceBreak)
        {
            debugBreak();
            *static_cast<volatile unsigned int*>(0) = 0x7306Bu;
        }
    }
}

} // namespace renddx9abi
#endif
