#include <ps5emu/elf/Relocation.hpp>

#include <ps5emu/elf/ElfFileView.hpp>

#include <cstring>
#include <limits>
#include <stdexcept>

namespace ps5emu::elf {
namespace {

constexpr std::uint64_t kDynamicTagRela = 7;
constexpr std::size_t kElf64RelaSize = 24;

#pragma pack(push, 1)
struct Elf64Rela {
    std::uint64_t offset;
    std::uint64_t info;
    std::int64_t addend;
};
#pragma pack(pop)

static_assert(sizeof(Elf64Rela) == kElf64RelaSize);

void ParseRange(std::vector<Relocation>& output,
                const ElfFileView& view,
                const DynamicTableReference& reference,
                std::uint64_t byteSize,
                bool procedureLinkage) {
    if (byteSize == 0) {
        return;
    }

    if (byteSize > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "ELF relocation table is too large for this host");
    }

    const auto hostSize = static_cast<std::size_t>(byteSize);
    if (hostSize % kElf64RelaSize != 0) {
        throw std::runtime_error(
            "ELF RELA table size is not entry-aligned");
    }

    const auto range = view.ResolveRange(reference, hostSize);

    for (std::size_t offset = 0;
         offset < range.size();
         offset += kElf64RelaSize) {
        Elf64Rela rela{};
        std::memcpy(&rela, range.data() + offset, sizeof(rela));

        output.push_back(Relocation{
            .offset = rela.offset,
            .symbolIndex =
                static_cast<std::uint32_t>(rela.info >> 32),
            .type =
                static_cast<std::uint32_t>(rela.info & 0xffffffffu),
            .addend = rela.addend,
            .procedureLinkage = procedureLinkage,
        });
    }
}

} // namespace

std::vector<Relocation> RelocationTable::Parse(
    std::span<const std::byte> bytes,
    const Image& image,
    const DynamicMetadata& metadata) {
    std::vector<Relocation> relocations;
    const ElfFileView view(bytes, image);

    if (metadata.relaSize != 0) {
        if (!metadata.relaTable.has_value()) {
            throw std::runtime_error(
                "ELF DT_RELASZ is present without a RELA table");
        }

        if (metadata.relaEntrySize != kElf64RelaSize) {
            throw std::runtime_error(
                "Unexpected ELF64 RELA entry size");
        }

        ParseRange(
            relocations,
            view,
            *metadata.relaTable,
            metadata.relaSize,
            false);
    }

    if (metadata.jumpRelocationSize != 0) {
        if (!metadata.jumpRelocationTable.has_value()) {
            throw std::runtime_error(
                "ELF DT_PLTRELSZ is present without a PLT relocation table");
        }

        if (!metadata.pltRelocationType.has_value() ||
            *metadata.pltRelocationType != kDynamicTagRela) {
            throw std::runtime_error(
                "Only ELF64 RELA PLT relocations are supported");
        }

        if (metadata.relaEntrySize != kElf64RelaSize) {
            throw std::runtime_error(
                "Unexpected ELF64 RELA entry size");
        }

        ParseRange(
            relocations,
            view,
            *metadata.jumpRelocationTable,
            metadata.jumpRelocationSize,
            true);
    }

    return relocations;
}

} // namespace ps5emu::elf
