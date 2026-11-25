// [CHOSEN, not recovered] Keep finite shader builds running while the window is hidden.
#include "ShaderCompileActivity.h"
#import <Foundation/Foundation.h>
#include <stdexcept>

namespace bf2macos {

ShaderCompileActivity::ShaderCompileActivity()
{
    @autoreleasepool {
        token_ = (__bridge_retained void*)[NSProcessInfo.processInfo
            beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep
            reason:@"OpenBF2 shader compilation"];
        if (!token_) throw std::runtime_error("OpenBF2 shader compilation activity could not begin");
    }
}

ShaderCompileActivity::~ShaderCompileActivity()
{
    @autoreleasepool {
        id token = (__bridge_transfer id)token_;
        [NSProcessInfo.processInfo endActivity:token];
    }
}

} // namespace bf2macos
