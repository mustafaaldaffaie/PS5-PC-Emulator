#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <ps5emu/runtime/ModuleExportTable.hpp>

namespace ps5emu::runtime {

class ModuleExportRegistry final {
public:
    void Register(
        std::string moduleName,
        ModuleExportTable exports);

    [[nodiscard]] const ModuleExport*
    FindByName(std::string_view moduleName,
               std::string_view symbolName) const noexcept;

    [[nodiscard]] const ModuleExport*
    FindByNid(std::string_view moduleName,
              std::string_view nid) const noexcept;

    [[nodiscard]] std::optional<std::uint64_t>
    ResolveByNid(std::string_view moduleName,
                 std::string_view nid) const noexcept;

    [[nodiscard]] std::size_t ModuleCount() const noexcept;
    [[nodiscard]] std::size_t ExportCount() const noexcept;

private:
    struct ModuleEntry {
        ModuleExportTable exports;
        std::unordered_map<std::string, std::string> rawNameByNid;
    };

    std::unordered_map<std::string, ModuleEntry> modules_;
    std::size_t exportCount_ = 0;
};

} // namespace ps5emu::runtime
