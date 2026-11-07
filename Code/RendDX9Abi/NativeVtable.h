// [CHOSEN, not recovered] Native adapters for explicit-self PE function tables.
// The shared consumer declaration supplies every native argument and return type.
// MSVC continues to use the original tables and thunks without these adapters.
#ifndef BF2_NATIVE_VTABLE_H
#define BF2_NATIVE_VTABLE_H
#if !defined(_MSC_VER)
#include "../BF2/Math/Vec3.h"
#include "../BF2/Core/Random.h"
#include <memory>
#include <type_traits>
#include <utility>

namespace renddx9abi {

// Only bridge the representation erasures used by the existing thunk bodies.
// Arithmetic category changes (especially integer/floating-point registers) are
// forbidden: those require a specific, reviewed adapter at the call boundary.
template<class To, class From>
typename std::enable_if<std::is_convertible<From&, To>::value, To>::type
nativeArgument(From& value)
{
    static_assert(!(std::is_floating_point<To>::value != std::is_floating_point<From>::value &&
                    std::is_arithmetic<To>::value && std::is_arithmetic<From>::value),
                  "native thunk changes arithmetic register category");
    return value;
}

template<class To, class From>
typename std::enable_if<!std::is_convertible<From&, To>::value &&
                        std::is_pointer<To>::value && std::is_pointer<From>::value, To>::type
nativeArgument(From& value)
{
    return reinterpret_cast<To>(value);
}

template<class To, class From>
typename std::enable_if<!std::is_convertible<From&, To>::value &&
                        std::is_pointer<To>::value && !std::is_pointer<From>::value, To>::type
nativeArgument(From& value)
{
    using Pointee = typename std::remove_cv<typename std::remove_pointer<To>::type>::type;
    using Value = typename std::remove_cv<From>::type;
    constexpr bool vector = std::is_same<Value, dice::hfe::Math::Vec2>::value ||
                            std::is_same<Value, dice::hfe::Math::Vec3>::value ||
                            std::is_same<Value, dice::hfe::Math::Vec4>::value;
    static_assert(std::is_same<Pointee, Value>::value ||
                  (std::is_class<Value>::value && std::is_void<Pointee>::value) ||
                  (vector && (std::is_same<Pointee, float>::value || std::is_same<Pointee, unsigned int>::value)) ||
                  (std::is_same<Value, dice::hfe::RandomDistribution>::value && std::is_same<Pointee, float>::value),
                  "native thunk cannot invent a pointer to an unrelated argument");
    return reinterpret_cast<To>(std::addressof(value));
}

template<class To, class From>
typename std::enable_if<std::is_convertible<From&&, To>::value, To>::type
nativeResult(From&& value)
{
    static_assert(!std::is_reference<To>::value || std::is_lvalue_reference<From>::value,
                  "native thunk cannot return a reference to a temporary");
    using Value = typename std::remove_reference<From>::type;
    static_assert(!(std::is_floating_point<To>::value != std::is_floating_point<Value>::value &&
                    std::is_arithmetic<To>::value && std::is_arithmetic<Value>::value),
                  "native thunk changes return register category");
    return std::forward<From>(value);
}

template<class To, class From>
typename std::enable_if<std::is_enum<To>::value && std::is_integral<From>::value, To>::type
nativeResult(From value) { return static_cast<To>(value); }

template<class To, class From>
typename std::enable_if<std::is_reference<To>::value && std::is_pointer<From>::value, To>::type
nativeResult(From value) { return *reinterpret_cast<typename std::remove_reference<To>::type*>(value); }

template<class To, class From>
typename std::enable_if<!std::is_convertible<From, To>::value &&
                        std::is_pointer<To>::value && std::is_pointer<From>::value, To>::type
nativeResult(From value) { return reinterpret_cast<To>(value); }

template<class R> struct NativeReturn
{
    template<class F, class... A> static R call(F function, A&&... arguments)
    {
        return nativeResult<R>(function(std::forward<A>(arguments)...));
    }
};
template<> struct NativeReturn<void>
{
    template<class F, class... A> static void call(F function, A&&... arguments)
    {
        function(std::forward<A>(arguments)...);
    }
};

template<class R, class Function, Function Target, int Extra> struct NativeCall;
template<class R, class S, class Q, class... B, Q(*Target)(S, B...)>
struct NativeCall<R, Q(*)(S, B...), Target, 0>
{
    template<class... A> static R invoke(void* self, A&&... arguments)
    {
        return NativeReturn<R>::call(Target, static_cast<S>(self), nativeArgument<B>(arguments)...);
    }
};
// These legacy value-return thunks explicitly construct/write the result through
// the first argument after self. The compiler, rather than a hand-written sret
// position, supplies the native return convention of invoke().
template<class R, class S, class Q, class O, class... B, Q(*Target)(S, O, B...)>
struct NativeCall<R, Q(*)(S, O, B...), Target, 1>
{
    template<class... A> static R invoke(void* self, A&&... arguments)
    {
        static_assert(std::is_class<R>::value && std::is_pointer<O>::value,
                      "only explicit class-value output buffers are adapted");
        typename std::aligned_storage<sizeof(R), alignof(R)>::type storage;
        R* result = reinterpret_cast<R*>(&storage);
        Target(static_cast<S>(self), reinterpret_cast<O>(result), nativeArgument<B>(arguments)...);
        R value(std::move(*result));
        result->~R();
        return value;
    }
};
template<class Method, class Function, Function Target> struct NativeSlot;
template<class R, class C, class... A, class S, class Q, class... B, Q(*Target)(S, B...)>
struct NativeSlot<R(C::*)(A...), Q(*)(S, B...), Target>
{
    static R invoke(void* self, A... arguments)
    {
        return NativeCall<R, Q(*)(S, B...), Target, int(sizeof...(B))-int(sizeof...(A))>::invoke(self, arguments...);
    }
};
template<class R, class C, class... A, class F, F Target>
struct NativeSlot<R(C::*)(A...) const, F, Target> : NativeSlot<R(C::*)(A...), F, Target> {};

// A PE deleting-destructor thunk takes a flag byte. Native complete and deleting
// destructors take self only. The original destructor still owns destruction/free.
template<class F, F Target> struct NativeDestructor
{
    static void complete(void* self) { Target(self, 0); }
    static void deleting(void* self) { Target(self, 1); }
};
}
#define BF2_NATIVE_SLOT(Interface, Method, Function) \
    reinterpret_cast<void*>(&::renddx9abi::NativeSlot<decltype(&Interface::Method), decltype(&Function), &Function>::invoke)
#define BF2_NATIVE_DTOR(Function, Which) \
    reinterpret_cast<void*>(&::renddx9abi::NativeDestructor<decltype(&Function), &Function>::Which)
#endif
#endif
