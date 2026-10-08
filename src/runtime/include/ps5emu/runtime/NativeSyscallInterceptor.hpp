#pragma once

#include <cstdint>
#include <vector>

#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

struct NativeSyscallTrap {
    std::uint64_t guestAddress = 0;
};

class NativeSyscallInterceptor final {
public:
    // Conservatively scans executable guest mappings for x86-64 SYSCALL
    // byte pairs (0F 05), replaces them with INT3/NOP, and returns the
    // breakpoint addresses. This prevents guest code from entering the host
    // kernel while preserving the original two-byte instruction footprint.
    [[nodiscard]] static std::vector<NativeSyscallTrap>
    Rewrite(memory::GuestMemory& memory);
};

} // namespace ps5emu::runtime
