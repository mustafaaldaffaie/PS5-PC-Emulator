#pragma once

#include <cstdint>

#include <ps5emu/hle/GuestMemoryAccess.hpp>
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
    std::uint64_t fsBase = 0;

    std::uint64_t rbx = 0;
    std::uint64_t rbp = 0;
    std::uint64_t r10 = 0;
    std::uint64_t r11 = 0;
    std::uint64_t r12 = 0;
    std::uint64_t r13 = 0;
    std::uint64_t r14 = 0;
    std::uint64_t r15 = 0;
    std::uint64_t rflags = 0x202;
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
             hle::GuestMemoryAccess& memory,
             const hle::HleRegistry& registry,
             const HleThunkTable& thunks,
             hle::GuestThreadAccess* threads = nullptr,
             hle::GuestFileSystemAccess* files = nullptr);

    [[nodiscard]] static GuestCallDispatchResult
    Dispatch(std::uint64_t targetAddress,
             SysvGuestContext& context,
             memory::GuestMemory& memory,
             const hle::HleRegistry& registry,
             const HleThunkTable& thunks,
             hle::GuestThreadAccess* threads = nullptr,
             hle::GuestFileSystemAccess* files = nullptr);
};

} // namespace ps5emu::runtime
