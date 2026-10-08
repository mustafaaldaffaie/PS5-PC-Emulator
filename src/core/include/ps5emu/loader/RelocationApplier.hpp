#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include <ps5emu/elf/Relocation.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::loader {

using SymbolAddressResolver =
    std::function<std::optional<std::uint64_t>(std::uint32_t)>;

struct RelocationContext {
    std::uint64_t loadBias = 0;
    SymbolAddressResolver resolveSymbol;
};

class RelocationApplier final {
public:
    // All values and targets are validated before writing. Resolver callbacks
    // must not mutate memory or its mappings during this operation.
    static void Apply(
        std::span<const elf::Relocation> relocations,
        memory::GuestMemory& memory,
        const RelocationContext& context);
};

} // namespace ps5emu::loader
