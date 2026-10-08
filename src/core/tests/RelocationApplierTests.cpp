#include <ps5emu/loader/RelocationApplier.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

std::uint64_t ReadU64(
    const ps5emu::memory::GuestMemory& memory,
    std::uint64_t address) {
    const auto bytes = memory.Read(address, sizeof(std::uint64_t));
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

} // namespace

int main() {
    using ps5emu::elf::Relocation;
    using ps5emu::loader::RelocationApplier;
    using ps5emu::loader::RelocationContext;
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;

    GuestMemory memory;
    memory.Map(
        0x500000,
        0x100,
        Protection::Read);

    const std::vector<Relocation> relocations{
        Relocation{
            .offset = 0x10,
            .symbolIndex = 0,
            .type = 8,
            .addend = 0x1234,
        },
        Relocation{
            .offset = 0x18,
            .symbolIndex = 2,
            .type = 7,
            .addend = 0,
            .procedureLinkage = true,
        },
        Relocation{
            .offset = 0x20,
            .symbolIndex = 3,
            .type = 1,
            .addend = -0x20,
        },
    };

    RelocationContext context;
    context.loadBias = 0x500000;
    context.resolveSymbol =
        [](std::uint32_t index)
            -> std::optional<std::uint64_t> {
        if (index == 2) {
            return 0x700000;
        }

        if (index == 3) {
            return 0x710000;
        }

        return std::nullopt;
    };

    RelocationApplier::Apply(
        relocations,
        memory,
        context);

    assert(ReadU64(memory, 0x500010) == 0x501234);
    assert(ReadU64(memory, 0x500018) == 0x700000);
    assert(ReadU64(memory, 0x500020) == 0x70ffe0);

    {
        const std::vector<Relocation> unresolved{
            Relocation{
                .offset = 0x28,
                .symbolIndex = 99,
                .type = 6,
            },
        };

        assert(ThrowsRuntimeError([&] {
            RelocationApplier::Apply(
                unresolved,
                memory,
                context);
        }));
    }

    {
        const std::vector<Relocation> unsupported{
            Relocation{
                .offset = 0x30,
                .symbolIndex = 0,
                .type = 12345,
            },
        };

        assert(ThrowsRuntimeError([&] {
            RelocationApplier::Apply(
                unsupported,
                memory,
                context);
        }));
    }

    return 0;
}
