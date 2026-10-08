#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <ps5emu/elf/ImportTable.hpp>
#include <ps5emu/hle/HleRegistry.hpp>

namespace ps5emu::runtime {

struct ImportIdentity {
    std::string module;
    std::string nid;
};

using ImportIdentityResolver =
    std::function<std::optional<ImportIdentity>(
        const elf::ImportSymbol&)>;

enum class ImportResolutionFailure {
    IdentityUnavailable,
    ServiceUnavailable
};

struct ImportBinding {
    std::uint32_t symbolIndex = 0;
    std::string symbolName;
    ImportIdentity identity;
    const hle::HleService* service = nullptr;
};

struct UnresolvedImport {
    std::uint32_t symbolIndex = 0;
    std::string symbolName;
    std::optional<ImportIdentity> identity;
    ImportResolutionFailure failure =
        ImportResolutionFailure::IdentityUnavailable;
};

struct ImportResolutionReport {
    std::vector<ImportBinding> bindings;
    std::vector<UnresolvedImport> unresolved;

    [[nodiscard]] bool Complete() const noexcept;
};

class ImportResolver final {
public:
    [[nodiscard]] static ImportResolutionReport
    Resolve(std::span<const elf::ImportSymbol> imports,
            const hle::HleRegistry& registry,
            const ImportIdentityResolver& identityResolver);
};

} // namespace ps5emu::runtime
