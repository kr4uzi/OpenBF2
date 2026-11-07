// tu_math.h	-- Willem Kokke

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// platform abstract math.h include file

#ifndef TU_MATH_H
#define TU_MATH_H

#include "base/tu_config.h"
#include <math.h>
#include <float.h>

// OSX doesn't have single precision math functions defined in math.h
// [BUILD-FIX SwiffPlayer: every supported macOS SDK declares the float functions, and these macros
// rename unrelated identifiers (renddx9abi::logf). The Windows build never took this branch, so
// it stays off on macOS too and the menu player uses the same float math everywhere.]
#if defined(__MACH__) && 0
    #define sinf sin
    #define cosf cos
    #define tanf tan
    #define asinf asin
    #define acosf acos
    #define atanf atan
    #define atan2f atan2
    #define sqrtf sqrt
    #define logf log
    #define expf exp
    #define fabsf fabs
    #define powf pow
#endif


// isfinite() comes with C99; fake version here in case compiler lacks it.
#ifndef isfinite
#define isfinite(x) (sizeof(x) == sizeof(float) ? isfinitef(x) : isfinited(x))
#define isfinitef(x) ((x) >= -FLT_MAX && (x) <= FLT_MAX)	// NAN should fail this, yes?
#define isfinited(x) ((x) >= -DBL_MAX && (x) <= DBL_MAX)
#endif // not isfinite


#endif // TU_MATH_H
