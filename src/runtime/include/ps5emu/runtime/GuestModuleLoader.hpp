#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/ExecutableLinker.hpp>
#include <ps5emu/runtime/GuestModuleCatalog.hpp>

namespace ps5emu::runtime {

struct GuestModuleLoadOptions {
    std::uint64_t loadBias = 0;
    ExternalSymbolResolver fallbackResolver;
};

struct LoadedGuestModule {
    std::string name;
    LinkedImage linked;
    elf::SceModuleMetadata metadata;
};

class GuestModuleLoader final {
public:
    [[nodiscard]] LoadedGuestModule
    Load(std::span<const std::byte> bytes,
         const GuestModuleLoadOptions& options = {});

    [[nodiscard]] const memory::GuestMemory&
    Memory() const noexcept;

    [[nodiscard]] const GuestModuleCatalog&
    Catalog() const noexcept;

private:
    memory::GuestMemory memory_;
    GuestModuleCatalog catalog_;
};

} // namespace ps5emu::runtime
