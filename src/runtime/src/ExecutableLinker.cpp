#include <ps5emu/runtime/ExecutableLinker.hpp>

#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Relocation.hpp>
#include <ps5emu/loader/RelocationApplier.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ps5emu::runtime {
namespace {

constexpr std::uint16_t kSectionAbsolute = 0xfff1;
constexpr std::uint16_t kSectionReserved = 0xff00;
constexpr std::uint8_t kBindingWeak = 2;

std::uint64_t BiasedAddress(std::uint64_t address, std::uint64_t bias) {
    if (address > std::numeric_limits<std::uint64_t>::max() - bias) {
        throw std::runtime_error("ELF symbol address overflows");
    }
    return address + bias;
}

} // namespace

LinkedImage ExecutableLinker::Load(
    std::span<const std::byte> bytes,
    memory::GuestMemory& memory,
    const LinkOptions& options) {
    const auto image = elf::Elf64::Parse(bytes);
    const auto metadata = elf::DynamicMetadataParser::Parse(bytes, image);
    const auto relocations = elf::RelocationTable::Parse(bytes, image, metadata);

    for (const auto& relocation : relocations) {
        if (relocation.type == 0) {
            continue;
        }
        const bool insideImage = std::any_of(
            image.loadSegments.begin(), image.loadSegments.end(),
            [&](const elf::Segment& segment) {
                if (relocation.offset < segment.virtualAddress) {
                    return false;
                }
                const auto offset = relocation.offset - segment.virtualAddress;
                return offset <= segment.memorySize &&
                    sizeof(std::uint64_t) <= segment.memorySize - offset;
            });
        if (!insideImage) {
            throw std::runtime_error("ELF relocation target is outside this image");
        }
    }

    auto staged = memory;
    LinkedImage result;
    result.loaded = loader::ExecutableImageLoader::LoadElf(
        bytes, staged, options.loadBias);

    std::unordered_map<std::uint32_t, std::uint64_t> addresses;
    loader::RelocationContext context;
    context.loadBias = options.loadBias;
    context.resolveSymbol = [&](std::uint32_t index)
        -> std::optional<std::uint64_t> {
        const auto found = addresses.find(index);
        if (found != addresses.end()) {
            return found->second;
        }

        const auto symbol = elf::DynamicSymbolTable::Read(
            bytes, image, metadata, index);
        // TLS and indirect functions need dedicated runtime machinery.
        if (symbol.Type() > 3) {
            throw std::runtime_error("Unsupported dynamic symbol type");
        }

        std::uint64_t address = 0;
        if (symbol.IsUndefined()) {
            const auto external = options.resolveExternal
                ? options.resolveExternal(symbol) : std::nullopt;
            if (external.has_value()) {
                address = *external;
                ++result.resolvedExternalSymbolCount;
            } else if (symbol.Binding() == kBindingWeak) {
                ++result.unresolvedWeakSymbolCount;
            } else {
                throw std::runtime_error(
                    "Unresolved required ELF symbol: " + symbol.name);
            }
        } else if (symbol.sectionIndex == kSectionAbsolute) {
            address = symbol.value;
        } else if (symbol.sectionIndex >= kSectionReserved) {
            throw std::runtime_error("Unsupported ELF symbol section index");
        } else {
            address = BiasedAddress(symbol.value, options.loadBias);
        }
        addresses.emplace(index, address);
        return address;
    };

    loader::RelocationApplier::Apply(relocations, staged, context);
    result.appliedRelocationCount = static_cast<std::size_t>(std::count_if(
        relocations.begin(), relocations.end(),
        [](const elf::Relocation& relocation) { return relocation.type != 0; }));
    memory = std::move(staged);
    return result;
}

} // namespace ps5emu::runtime
