#pragma once

#include <cstdint>

#include <ps5emu/runtime/GuestCallDispatcher.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <ucontext.h>
#else
#error "NativeContextAdapter currently supports Windows and Linux only"
#endif

namespace ps5emu::runtime {

class NativeContextAdapter final {
public:
#if defined(_WIN32)
    [[nodiscard]] static SysvGuestContext
    FromNative(const CONTEXT& nativeContext,
               std::uint64_t fsBase) noexcept;

    static void ToNative(const SysvGuestContext& guestContext,
                         CONTEXT& nativeContext) noexcept;
#elif defined(__linux__)
    [[nodiscard]] static SysvGuestContext
    FromNative(const ucontext_t& nativeContext,
               std::uint64_t fsBase) noexcept;

    static void ToNative(const SysvGuestContext& guestContext,
                         ucontext_t& nativeContext) noexcept;
#endif
};

} // namespace ps5emu::runtime
