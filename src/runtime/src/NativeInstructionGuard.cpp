#include <ps5emu/runtime/NativeInstructionGuard.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace ps5emu::runtime {
namespace {

struct ForbiddenInstruction {
    std::array<std::byte, 2> bytes;
    const char* name;
};

constexpr std::array<ForbiddenInstruction, 3>
    kForbiddenInstructions{{
        {
            {std::byte{0x0f}, std::byte{0x05}},
            "SYSCALL",
        },
        {
            {std::byte{0x0f}, std::byte{0x34}},
            "SYSENTER",
        },
        {
            {std::byte{0xcd}, std::byte{0x80}},
            "INT 0x80",
        },
    }};

[[noreturn]] void Reject(
    const char* name,
    std::uint64_t guestAddress) {
    std::ostringstream message;
    message
        << "Native execution refuses un-intercepted guest "
        << name
        << " at guest address 0x"
        << std::hex
        << guestAddress;

    throw std::runtime_error(
        message.str());
}

} // namespace

void NativeInstructionGuard::Validate(
    const memory::GuestMemory& memory) {
    for (const auto& mapping : memory.Mappings()) {
        if (!memory::HasProtection(
                mapping.protection,
                memory::Protection::Execute)) {
            continue;
        }

        if (mapping.data.size() < 2) {
            continue;
        }

        for (std::size_t offset = 0;
             offset + 1 < mapping.data.size();
             ++offset) {
            for (const auto& instruction :
                 kForbiddenInstructions) {
                if (mapping.data[offset] !=
                        instruction.bytes[0] ||
                    mapping.data[offset + 1] !=
                        instruction.bytes[1]) {
                    continue;
                }

                Reject(
                    instruction.name,
                    mapping.guestAddress +
                        static_cast<std::uint64_t>(
                            offset));
            }
        }
    }
}

} // namespace ps5emu::runtime
