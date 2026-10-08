#include <ps5emu/runtime/NativeSyscallInterceptor.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace ps5emu::runtime {

std::vector<NativeSyscallTrap>
NativeSyscallInterceptor::Rewrite(
    memory::GuestMemory& memory) {
    std::vector<NativeSyscallTrap> traps;

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
            if (mapping.data[offset] != std::byte{0x0f} ||
                mapping.data[offset + 1] != std::byte{0x05}) {
                continue;
            }

            if (static_cast<std::uint64_t>(offset) >
                std::numeric_limits<std::uint64_t>::max() -
                    mapping.guestAddress) {
                throw std::overflow_error(
                    "Guest syscall trap address overflows");
            }

            const auto address =
                mapping.guestAddress +
                static_cast<std::uint64_t>(offset);

            const std::array<std::byte, 2> replacement{
                std::byte{0xcc},
                std::byte{0x90},
            };

            memory.Initialize(
                address,
                replacement);

            traps.push_back(
                NativeSyscallTrap{
                    .guestAddress = address,
                });

            ++offset;
        }
    }

    return traps;
}

} // namespace ps5emu::runtime
