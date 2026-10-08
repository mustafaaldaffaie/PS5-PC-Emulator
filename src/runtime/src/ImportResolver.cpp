#include <ps5emu/runtime/ImportResolver.hpp>

#include <stdexcept>
#include <utility>

namespace ps5emu::runtime {

bool ImportResolutionReport::Complete() const noexcept {
    return unresolved.empty();
}

ImportResolutionReport ImportResolver::Resolve(
    std::span<const elf::ImportSymbol> imports,
    const hle::HleRegistry& registry,
    const ImportIdentityResolver& identityResolver) {
    if (!identityResolver) {
        throw std::invalid_argument(
            "Import identity resolver cannot be empty");
    }

    ImportResolutionReport report;
    report.bindings.reserve(imports.size());
    report.unresolved.reserve(imports.size());

    for (const auto& import : imports) {
        auto identity = identityResolver(import);

        if (!identity.has_value() ||
            identity->module.empty() ||
            identity->nid.empty()) {
            report.unresolved.push_back(UnresolvedImport{
                .symbolIndex = import.symbolIndex,
                .symbolName = import.name,
                .identity = std::move(identity),
                .failure =
                    ImportResolutionFailure::IdentityUnavailable,
            });
            continue;
        }

        const auto* service =
            registry.Find(identity->module, identity->nid);

        if (service == nullptr) {
            report.unresolved.push_back(UnresolvedImport{
                .symbolIndex = import.symbolIndex,
                .symbolName = import.name,
                .identity = std::move(identity),
                .failure =
                    ImportResolutionFailure::ServiceUnavailable,
            });
            continue;
        }

        report.bindings.push_back(ImportBinding{
            .symbolIndex = import.symbolIndex,
            .symbolName = import.name,
            .identity = std::move(*identity),
            .service = service,
        });
    }

    return report;
}

} // namespace ps5emu::runtime
