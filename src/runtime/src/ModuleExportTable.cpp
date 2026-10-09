#include <ps5emu/runtime/ModuleExportTable.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

#include <ps5emu/elf/DynamicSymbol.hpp>

namespace ps5emu::runtime {
namespace {

constexpr std::uint8_t kBindingGlobal = 1;
constexpr std::uint8_t kBindingWeak = 2;
constexpr std::uint8_t kSymbolTypeTls = 6;
constexpr std::uint16_t kSectionAbsolute = 0xfff1;
constexpr std::uint16_t kSectionReserved = 0xff00;

std::uint64_t BiasedAddress(
    std::uint64_t value,
    std::uint64_t loadBias) {
    if (value >
        std::numeric_limits<std::uint64_t>::max() -
            loadBias) {
        throw std::overflow_error(
            "Module export address overflows");
    }

    return value + loadBias;
}

bool IsExportBinding(std::uint8_t binding) noexcept {
    return binding == kBindingGlobal ||
           binding == kBindingWeak;
}

} // namespace

ModuleExportTable ModuleExportTable::Build(
    std::span<const std::byte> bytes,
    const elf::Image& image,
    const elf::DynamicMetadata& metadata,
    std::uint64_t loadBias) {
    if (!metadata.symbolTable.has_value() ||
        metadata.symbolTableSize == 0 ||
        metadata.symbolEntrySize == 0) {
        throw std::runtime_error(
            "Module export discovery requires a sized dynamic symbol table");
    }

    if (metadata.symbolTableSize %
            metadata.symbolEntrySize !=
        0) {
        throw std::runtime_error(
            "Dynamic symbol table size is not entry-aligned");
    }

    const auto count =
        metadata.symbolTableSize /
        metadata.symbolEntrySize;

    if (count >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()) +
            1ull) {
        throw std::runtime_error(
            "Dynamic symbol table contains too many entries");
    }

    ModuleExportTable table;

    for (std::uint64_t index = 0;
         index < count;
         ++index) {
        const auto symbol =
            elf::DynamicSymbolTable::Read(
                bytes,
                image,
                metadata,
                static_cast<std::uint32_t>(index));

        if (symbol.IsUndefined() ||
            symbol.name.empty() ||
            !IsExportBinding(symbol.Binding()) ||
            symbol.Type() == kSymbolTypeTls) {
            continue;
        }

        std::uint64_t address = 0;

        if (symbol.sectionIndex ==
            kSectionAbsolute) {
            address = symbol.value;
        } else {
            if (symbol.sectionIndex >=
                kSectionReserved) {
                throw std::runtime_error(
                    "Module export uses an unsupported reserved section index");
            }

            address =
                BiasedAddress(
                    symbol.value,
                    loadBias);
        }

        const auto found =
            table.byName_.find(symbol.name);

        if (found != table.byName_.end()) {
            auto& existing =
                table.exports_[found->second];

            if (existing.binding ==
                    kBindingWeak &&
                symbol.Binding() ==
                    kBindingGlobal) {
                existing = ModuleExport{
                    .symbolIndex = symbol.index,
                    .name = symbol.name,
                    .address = address,
                    .size = symbol.size,
                    .binding = symbol.Binding(),
                    .type = symbol.Type(),
                };
                continue;
            }

            if (existing.binding ==
                    kBindingGlobal &&
                symbol.Binding() ==
                    kBindingWeak) {
                continue;
            }

            if (existing.address != address) {
                throw std::runtime_error(
                    "Module contains conflicting dynamic exports for symbol: " +
                    symbol.name);
            }

            continue;
        }

        const auto outputIndex =
            table.exports_.size();

        table.exports_.push_back(
            ModuleExport{
                .symbolIndex = symbol.index,
                .name = symbol.name,
                .address = address,
                .size = symbol.size,
                .binding = symbol.Binding(),
                .type = symbol.Type(),
            });

        table.byName_.emplace(
            table.exports_.back().name,
            outputIndex);
    }

    return table;
}

const ModuleExport*
ModuleExportTable::Find(
    std::string_view name) const noexcept {
    const auto found =
        byName_.find(std::string(name));

    if (found == byName_.end()) {
        return nullptr;
    }

    return &exports_[found->second];
}

const std::vector<ModuleExport>&
ModuleExportTable::Exports() const noexcept {
    return exports_;
}

std::size_t
ModuleExportTable::Size() const noexcept {
    return exports_.size();
}

} // namespace ps5emu::runtime
