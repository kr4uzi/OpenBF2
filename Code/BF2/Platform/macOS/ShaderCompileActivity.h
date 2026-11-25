// [CHOSEN, not recovered] macOS shader compilation scheduling. See docs/macos-port.md.
#pragma once
#if defined(__APPLE__)
#include <pthread/qos.h>

namespace bf2macos {

// Compilation is finite work needed by the requested level. Keep higher caller priorities.
inline qos_class_t shaderCompileQos(qos_class_t caller)
{
    return caller == QOS_CLASS_USER_INTERACTIVE ? caller : QOS_CLASS_USER_INITIATED;
}

// Prevents App Nap during one effect, Metal library or pipeline build. Destroy on scope exit,
// including failure/exception paths. Independent overlapping builds own independent activities.
// This does not prevent idle system/display sleep or change application focus.
class ShaderCompileActivity {
public:
    ShaderCompileActivity();
    ~ShaderCompileActivity();
    ShaderCompileActivity(const ShaderCompileActivity&) = delete;
    ShaderCompileActivity& operator=(const ShaderCompileActivity&) = delete;
private:
    void* token_;
};

} // namespace bf2macos
#endif
