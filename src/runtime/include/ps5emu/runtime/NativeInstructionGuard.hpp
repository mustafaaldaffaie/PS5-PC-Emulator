#pragma once

#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

// Conservative pre-execution guard for guest instructions that would otherwise
// cross directly into the host kernel. This is intentionally a byte-pattern
// guard until a real syscall interception/decoder path is installed.
class NativeInstructionGuard final {
public:
    static void Validate(
        const memory::GuestMemory& memory);
};

} // namespace ps5emu::runtime
