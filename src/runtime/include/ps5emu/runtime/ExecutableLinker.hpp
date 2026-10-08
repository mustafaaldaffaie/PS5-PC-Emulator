#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include <ps5emu/elf/DynamicSymbol.hpp>
#include <ps5emu/loader/ExecutableImageLoader.hpp>

namespace ps5emu::runtime {

// Addresses must already refer to guest-visible code or data. An HLE handler
// is not itself a callable guest address; native trampolines are still needed.
using ExternalSymbolResolver =
    std::function<std::optional<std::uint64_t>(const elf::DynamicSymbol&)>;

struct LinkOptions {
    std::uint64_t loadBias = 0;
    ExternalSymbolResolver resolveExternal;
};

struct LinkedImage {
    loader::LoadedImage loaded;
    std::size_t appliedRelocationCount = 0;
    std::size_t resolvedExternalSymbolCount = 0;
    std::size_t unresolvedWeakSymbolCount = 0;
};

class ExecutableLinker final {
public:
    // On failure, the caller's memory is unchanged. Resolver callbacks must
    // not mutate that memory or recursively invoke this linker on it.
    [[nodiscard]] static LinkedImage
    Load(std::span<const std::byte> bytes,
         memory::GuestMemory& memory,
         const LinkOptions& options = {});
};

} // namespace ps5emu::runtime
