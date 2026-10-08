#include <ps5emu/loader/RelocationApplier.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ps5emu::loader {
namespace {

constexpr std::uint32_t kRelocationNone = 0;
constexpr std::uint32_t kRelocation64 = 1;
constexpr std::uint32_t kRelocationGlobDat = 6;
constexpr std::uint32_t kRelocationJumpSlot = 7;
constexpr std::uint32_t kRelocationRelative = 8;

std::uint64_t AddUnsigned(std::uint64_t lhs,
                          std::uint64_t rhs,
                          const char* message) {
    if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs) {
        throw std::runtime_error(message);
    }

    return lhs + rhs;
}

std::uint64_t AddSigned(std::uint64_t base,
                        std::int64_t addend,
                        const char* message) {
    if (addend >= 0) {
        return AddUnsigned(
            base,
            static_cast<std::uint64_t>(addend),
            message);
    }

    const auto magnitude =
        static_cast<std::uint64_t>(-(addend + 1)) + 1;

    if (base < magnitude) {
        throw std::runtime_error(message);
    }

    return base - magnitude;
}

std::uint64_t ResolveSymbol(
    std::uint32_t symbolIndex,
    const RelocationContext& context) {
    if (symbolIndex == 0) {
        return 0;
    }
    if (!context.resolveSymbol) {
        throw std::runtime_error(
            "Relocation requires a symbol resolver");
    }

    const auto address = context.resolveSymbol(symbolIndex);
    if (!address.has_value()) {
        throw std::runtime_error(
            "Relocation references an unresolved symbol");
    }

    return *address;
}

void WriteU64(memory::GuestMemory& memory,
              std::uint64_t address,
              std::uint64_t value) {
    std::array<std::byte, sizeof(value)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    memory.Initialize(address, bytes);
}

} // namespace

void RelocationApplier::Apply(
    std::span<const elf::Relocation> relocations,
    memory::GuestMemory& memory,
    const RelocationContext& context) {
    struct PendingWrite {
        std::uint64_t address;
        std::uint64_t value;
    };
    std::vector<PendingWrite> writes;
    writes.reserve(relocations.size());

    for (const auto& relocation : relocations) {
        if (relocation.type == kRelocationNone) {
            continue;
        }
        const auto targetAddress =
            AddUnsigned(
                context.loadBias,
                relocation.offset,
                "Relocation target address overflows");

        std::uint64_t value = 0;

        switch (relocation.type) {
        case kRelocation64: {
            const auto symbolAddress =
                ResolveSymbol(relocation.symbolIndex, context);
            value = AddSigned(
                symbolAddress,
                relocation.addend,
                "R_X86_64_64 relocation value overflows");
            break;
        }

        case kRelocationGlobDat:
        case kRelocationJumpSlot:
            value = ResolveSymbol(
                relocation.symbolIndex,
                context);
            break;

        case kRelocationRelative:
            value = AddSigned(
                context.loadBias,
                relocation.addend,
                "R_X86_64_RELATIVE relocation value overflows");
            break;

        default:
            throw std::runtime_error(
                "Unsupported x86-64 relocation type");
        }

        if (!memory.IsMapped(targetAddress, sizeof(value))) {
            throw std::runtime_error("Relocation target is outside mapped memory");
        }
        writes.push_back({targetAddress, value});
    }

    // Resolving and validating the entire batch prevents partial patches.
    for (const auto& write : writes) {
        WriteU64(memory, write.address, write.value);
    }
}

} // namespace ps5emu::loader
