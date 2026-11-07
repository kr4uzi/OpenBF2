// Debug filtering and callback dispatch; source path [INFERRED].
// Debug destruction can terminate the process according to DebugType; preserve the type mapping.

#include "Debug.h"
#include "Hash.h"        // dice::hfe::Hash<K,V,HashFunc>, StringHashFunc. API proven by this TU:

#include <cstdlib>       // abort

namespace dice {
namespace hfe {

// Debug globals point to storage arrays so loadable modules can bind to the host's arrays.
// Keep this indirection. Definition placement here is [CHOSEN, placement only].
bool _debugOn[DebugTypeCount] =                       // 0x8990144, .data: 01 x9 (+3 pad bytes)
{
    true, true, true, true, true, true, true, true, true    // every type 0..8 enabled by default
};
DebugCallback _bfGlobal_callback[DebugTypeCount];     // 0x8AC8600, .bss - all null until BF2Log
void*         _bfGlobal_callbackData[DebugTypeCount]; // 0x8AC8640, .bss
Hash<std::string, std::string, StringHashFunc> _debugModules[DebugTypeCount];

bool*          debugOn               = _debugOn;
DebugCallback* bfGlobal_callback     = _bfGlobal_callback;
void**         bfGlobal_callbackData = _bfGlobal_callbackData;
Hash<std::string, std::string, StringHashFunc>* debugModules = _debugModules;

// 0x83BFD60 (C2, 0 refs) / 0x83BFED0 (C1, 212 B - the ctor all 593 call sites use). C2 is the
// base-object clone with the stream construction inlined; one source constructor.
Debug::Debug(DebugType type, const std::string& file, int line, const std::string& func)
    : m_type(type)     // [this+0]
    , m_file(file)     // [this+4]
    , m_func(func)     // [this+8]  - constructed before m_line is stored; member order is attested
    , m_line(line)     // [this+12]
{
    // m_stream default-constructs at [this+16] (ios_base::out; the C1 calls streambuf's ctor and
    // stringbuf::_M_stringbuf_init out of line, C2 inlines them - same three stores either way).
}

// Dispatches the accumulated message through the registered callback after filtering.
// Emitted types other than Log and Info terminate the process after the callback returns.
Debug::~Debug()
{
    // Fold alias types before filtering so they share their base type's settings and callback.
    bool forceAbort = false;                                        // [INFERRED name]
    if (m_type == DebugTypeError2)          // 7 is tested first
    {
        forceAbort = true;
        m_type = DebugTypeError;
    }
    else if (m_type == DebugTypeAssert2)    // then 4
    {
        forceAbort = true;
        m_type = DebugTypeAssert;
    }

    // Derive module from the parent directory and fileName from the basename.
    // Without a slash, both retain the whole input string.
    std::string::size_type namePos = m_file.rfind('/');
    std::string fileName   = m_file.substr(namePos + 1);
    std::string::size_type dirPos  = m_file.rfind('/', namePos - 1);
    std::string moduleName = m_file.substr(dirPos + 1, namePos - dirPos - 1);

    // The module list excludes entries when the type is enabled and includes entries when disabled.
    if (debugOn[m_type] && debugModules[m_type].exists(moduleName))
    {
        // Suppressed messages also suppress process termination, including forced-abort types.
    }
    else if (debugOn[m_type] || debugModules[m_type].exists(moduleName))
    {
        // 4. Dispatch (0x83C00CB-0x83C038B). Per-type callback slot, else the built-in
        // emitter. Both receive bfGlobal_callbackData[m_type] (0x83C014B / 0x83C034E).
        if (bfGlobal_callback[m_type])
            bfGlobal_callback[m_type](m_type, moduleName, fileName, m_line, m_func,
                                      m_stream.str(), m_file, bfGlobal_callbackData[m_type]);
        else
            internal_debugCallback(m_type, moduleName, fileName, m_line, m_func,
                                   m_stream.str(), m_file, bfGlobal_callbackData[m_type]);

        // Emitted types other than Log and Info abort after the callback returns.
        if (m_type != DebugTypeLog && m_type != DebugTypeInfo)
            forceAbort = true;
        if (forceAbort)
            abort();
    }

    // Types 6/7 and 3/4 have identical termination behavior after folding.
    // Code following a Debug temporary does not imply it survives the destructor.
}

// No bounds check, no locking.
// RendDX9.dll copy PE 0x10074340 (type/callback in ECX/EDX, data on the stack; unreachable in 1.50:
// no code or data references).
void setDebugCallback(DebugType type, DebugCallback callback, void* data)
{
    bfGlobal_callback[type]     = callback;
    bfGlobal_callbackData[type] = data;
}

// (the clear() is inlined here and called out of line in turnOffAllDebug -
// compiler variance, same source). plain `int` parameter, attested by the mangling.
// RendDX9.dll copy PE 0x10074850 (clear() as a tail jump to the Hash clear).
void turnOnAllDebug(int type)
{
    debugOn[type] = true;
    debugModules[type].clear();
}


// RendDX9.dll copy PE 0x10074870.
void turnOffAllDebug(int type)
{
    debugOn[type] = false;
    debugModules[type].clear();
}

// Enables debugging for module. The exception list is an exclusion list when the type is on
// and an inclusion list when it is off; insertion avoids duplicate entries.
// RendDX9.dll copy PE 0x10074890 (fastcall type/module). Its exists-then-insert is one out-of-line
// Hash member there, PE 0x10074940: bucket, exists(), then a node with the key and an empty value
// pushed on the chain, answering whether it inserted; the same operations as below.
void turnOnDebug(int type, const std::string& module)
{
    if (debugOn[type])
        debugModules[type].remove(module);
    else if (!debugModules[type].exists(module))
        debugModules[type].insert(module, std::string());
}

// Exact mirror, same caller-side guard. RendDX9.dll copy PE 0x100748C0.
void turnOffDebug(int type, const std::string& module)
{
    if (!debugOn[type])
        debugModules[type].remove(module);
    else if (!debugModules[type].exists(module))
        debugModules[type].insert(module, std::string());
}

// Fallback callback for types without a registered handler. It formats but does not emit the message.
// file and data are unused; formatting uses the module path. Fatal types can therefore abort silently.
void internal_debugCallback(DebugType type, const std::string& module, const std::string& file,
                            int line, const std::string& func, const std::string& text,
                            const std::string& fullFileName, void* data)
{
    std::string typeName;
    switch (type)                       // 0x83C0F65.. - literals verbatim
    {
    case DebugTypeDebug:    typeName = "Debug";     break;
    case DebugTypeInfo:     typeName = "Info";      break;
    case DebugTypeWarning:  typeName = "Warning";   break;
    case DebugTypeAssert:   typeName = "Assert";    break;
    case DebugTypeDxAssert: typeName = "DxAssert";  break;   // yes, on the Linux server binary
    case DebugTypeError:    typeName = "Error";     break;
    case DebugTypeLog:      typeName = "Log";       break;
    default:                typeName = "(unknown)"; break;   // 4 and 7 would land here, but
                                                             // ~Debug folds them away first
    }

    std::ostringstream out;
    out << fullFileName << '(' << line << "): " << typeName << ": " << module << ": ";
    if (type == DebugTypeAssert)
        out << "(" << func << ") ";     // the '(' char vs "(" literal split is the binary's
    out << text << std::endl;
    // The formatted `out` stream is discarded.

    if (type == DebugTypeAssert || type == DebugTypeError)
    {
        std::ostringstream caption;     // [UNRESOLVED] constructed before msg and never written -
                                        // almost certainly the Win32 MessageBox caption
        std::ostringstream msg;
        msg << "Debug assertion failed!" << std::endl << std::endl;
        msg << "Module: " << module << std::endl;
        msg << "File: " << fullFileName << std::endl;
        msg << "Line: " << line << std::endl << std::endl;
        if (!func.empty())
            msg << "Expression: " << func << std::endl;   // the ctor's 4th argument doubles as the
                                                          // assert expression text
        if (!text.empty())
            msg << "Text: " << text << std::endl;
        msg << std::endl;
        abort();                        // `caption` and `msg` are discarded too; the abort is all
                                        // that survives on this build
    }
}

} // namespace hfe
} // namespace dice
