#include <ps5emu/runtime/ModuleExportRegistry.hpp>

#include <stdexcept>
#include <utility>

#include <ps5emu/elf/SceQualifiedSymbol.hpp>

namespace ps5emu::runtime {

void ModuleExportRegistry::Register(
    std::string moduleName,
    ModuleExportTable exports) {
    if (moduleName.empty()) {
        throw std::invalid_argument(
            "Module export registry name cannot be empty");
    }

    if (modules_.contains(moduleName)) {
        throw std::runtime_error(
            "Module export registry already contains module: " +
            moduleName);
    }

    ModuleEntry entry{
        .exports = std::move(exports),
    };

    for (const auto& item : entry.exports.Exports()) {
        const auto qualified =
            elf::SceQualifiedSymbol::Parse(item.name);

        if (!qualified.has_value()) {
            continue;
        }

        const auto found =
            entry.rawNameByNid.find(qualified->nid);

        if (found == entry.rawNameByNid.end()) {
            entry.rawNameByNid.emplace(
                qualified->nid,
                item.name);
            continue;
        }

        const auto* existing =
            entry.exports.Find(found->second);

        if (existing == nullptr ||
            existing->address != item.address) {
            throw std::runtime_error(
                "Module contains conflicting exports for NID: " +
                qualified->nid);
        }
    }

    exportCount_ += entry.exports.Size();
    modules_.emplace(
        std::move(moduleName),
        std::move(entry));
}

const ModuleExport*
ModuleExportRegistry::FindByName(
    std::string_view moduleName,
    std::string_view symbolName) const noexcept {
    const auto module =
        modules_.find(std::string(moduleName));

    if (module == modules_.end()) {
        return nullptr;
    }

    return module->second.exports.Find(symbolName);
}

const ModuleExport*
ModuleExportRegistry::FindByNid(
    std::string_view moduleName,
    std::string_view nid) const noexcept {
    const auto module =
        modules_.find(std::string(moduleName));

    if (module == modules_.end()) {
        return nullptr;
    }

    const auto rawName =
        module->second.rawNameByNid.find(
            std::string(nid));

    if (rawName ==
        module->second.rawNameByNid.end()) {
        return nullptr;
    }

    return module->second.exports.Find(
        rawName->second);
}

std::optional<std::uint64_t>
ModuleExportRegistry::ResolveByNid(
    std::string_view moduleName,
    std::string_view nid) const noexcept {
    const auto* item =
        FindByNid(moduleName, nid);

    if (item == nullptr) {
        return std::nullopt;
    }

    return item->address;
}

std::size_t
ModuleExportRegistry::ModuleCount() const noexcept {
    return modules_.size();
}

std::size_t
ModuleExportRegistry::ExportCount() const noexcept {
    return exportCount_;
}

} // namespace ps5emu::runtime
