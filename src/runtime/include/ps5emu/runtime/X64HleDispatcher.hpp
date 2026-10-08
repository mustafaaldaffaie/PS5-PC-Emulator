#pragma once

#include <cstdint>

#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>

namespace ps5emu::runtime {

struct X64GuestContext {
    std::uint64_t rax = 0;
    std::uint64_t rdi = 0;
    std::uint64_t rsi = 0;
    std::uint64_t rdx = 0;
    std::uint64_t rcx = 0;
    std::uint64_t r8 = 0;
    std::uint64_t r9 = 0;
    std::uint64_t rsp = 0;
    std::uint64_t rip = 0;
};

struct HleDispatchResult {
    bool handled = false;
    std::int64_t errorCode = 0;
};

class X64HleDispatcher final {
public:
    // Handles integer/pointer arguments using the SysV AMD64 calling
    // convention. Arguments 1-6 come from registers, arguments 7-8 come from
    // the guest stack after the return address.
    [[nodiscard]] static HleDispatchResult
    DispatchIfThunk(X64GuestContext& context,
                    const memory::GuestMemory& memory,
                    const HleThunkTable& thunks,
                    const hle::HleRegistry& registry);
};

} // namespace ps5emu::runtime
