#include <ps5emu/elf/ImportTable.hpp>

#include <ps5emu/elf/DynamicSymbol.hpp>

#include <stdexcept>
#include <unordered_map>

namespace ps5emu::elf {

std::vector<ImportSymbol> ImportTable::Parse(
    std::span<const std::byte> bytes,
    const Image& image,
    const DynamicMetadata& metadata) {
    const auto relocations =
        RelocationTable::Parse(bytes, image, metadata);

    std::vector<ImportSymbol> imports;
    std::unordered_map<std::uint32_t, std::size_t> bySymbolIndex;

    for (const auto& relocation : relocations) {
        if (relocation.symbolIndex == 0) {
            continue;
        }

        const auto existing = bySymbolIndex.find(relocation.symbolIndex);
        if (existing != bySymbolIndex.end()) {
            imports[existing->second].relocations.push_back(relocation);
            continue;
        }

        const auto symbol =
            DynamicSymbolTable::Read(
                bytes,
                image,
                metadata,
                relocation.symbolIndex);

        if (!symbol.IsUndefined()) {
            continue;
        }

        if (symbol.name.empty()) {
            throw std::runtime_error(
                "Undefined dynamic import has an empty symbol name");
        }

        const auto newIndex = imports.size();
        bySymbolIndex.emplace(relocation.symbolIndex, newIndex);

        ImportSymbol import;
        import.symbolIndex = relocation.symbolIndex;
        import.name = symbol.name;
        import.binding = symbol.Binding();
        import.type = symbol.Type();
        import.relocations.push_back(relocation);

        imports.push_back(std::move(import));
    }

    return imports;
}

} // namespace ps5emu::elf
