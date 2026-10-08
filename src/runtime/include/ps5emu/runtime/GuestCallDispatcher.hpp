#pragma once

#include <cstdint>

#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>

namespace ps5emu::runtime {

struct SysvGuestContext {
    std::uint64_t rdi = 0;
    std::uint64_t rsi = 0;
    std::uint64_t rdx = 0;
    std::uint64_t rcx = 0;
    std::uint64_t r8 = 0;
    std::uint64_t r9 = 0;
    std::uint64_t rsp = 0;
    std::uint64_t rax = 0;
    std::uint64_t rip = 0;
};

struct GuestCallDispatchResult {
    bool handled = false;
    std::int64_t errorCode = 0;
};

class GuestCallDispatcher final {
public:
    [[nodiscard]] static GuestCallDispatchResult
    Dispatch(std::uint64_t targetAddress,
             SysvGuestContext& context,
             memory::GuestMemory& memory,
             const hle::HleRegistry& registry,
             const HleThunkTable& thunks);
};

} // namespace ps5emu::runtime
