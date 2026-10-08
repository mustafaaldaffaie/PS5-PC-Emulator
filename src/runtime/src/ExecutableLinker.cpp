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
constexpr std::uint8_t kSymbolTypeTls = 6;
constexpr std::uint64_t kMainTlsModuleId = 1;
constexpr std::uint64_t kMinimumTlsAlignment = 16;

std::uint64_t BiasedAddress(std::uint64_t address, std::uint64_t bias) {
    if (address > std::numeric_limits<std::uint64_t>::max() - bias) {
        throw std::runtime_error("ELF symbol address overflows");
    }
    return address + bias;
}

std::uint64_t AlignUp(std::uint64_t value,
                      std::uint64_t alignment) {
    if (alignment <= 1) {
        return value;
    }

    const auto mask = alignment - 1;
    if (value > std::numeric_limits<std::uint64_t>::max() - mask) {
        throw std::runtime_error("ELF TLS block size overflows");
    }

    return (value + mask) & ~mask;
}

struct TlsLayout {
    std::uint64_t blockSize = 0;
};

std::optional<TlsLayout> BuildTlsLayout(const elf::Image& image) {
    if (!image.tlsSegment.has_value() ||
        image.tlsSegment->memorySize == 0) {
        return std::nullopt;
    }

    const auto alignment =
        std::max<std::uint64_t>(
            image.tlsSegment->alignment,
            kMinimumTlsAlignment);
    const auto blockSize =
        AlignUp(
            image.tlsSegment->memorySize,
            alignment);

    if (blockSize >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::runtime_error(
            "ELF TLS block is too large for x86-64 thread-pointer offsets");
    }

    return TlsLayout{
        .blockSize = blockSize,
    };
}

} // namespace

LinkedImage ExecutableLinker::Load(
    std::span<const std::byte> bytes,
    memory::GuestMemory& memory,
    const LinkOptions& options) {
    const auto image = elf::Elf64::Parse(bytes);
    const auto metadata = elf::DynamicMetadataParser::Parse(bytes, image);
    const auto relocations = elf::RelocationTable::Parse(bytes, image, metadata);
    const auto tlsLayout = BuildTlsLayout(image);

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
    std::unordered_map<std::uint32_t, loader::TlsSymbolResolution>
        tlsAddresses;

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

        if (symbol.Type() == kSymbolTypeTls) {
            throw std::runtime_error(
                "TLS symbol requires an x86-64 TLS relocation");
        }

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

    context.resolveTlsSymbol = [&](std::uint32_t index)
        -> std::optional<loader::TlsSymbolResolution> {
        if (!tlsLayout.has_value() ||
            !image.tlsSegment.has_value()) {
            throw std::runtime_error(
                "TLS relocation requires a non-empty PT_TLS segment");
        }

        const auto found = tlsAddresses.find(index);
        if (found != tlsAddresses.end()) {
            return found->second;
        }

        std::uint64_t moduleOffset = 0;

        if (index != 0) {
            const auto symbol =
                elf::DynamicSymbolTable::Read(
                    bytes,
                    image,
                    metadata,
                    index);

            if (symbol.Type() != kSymbolTypeTls) {
                throw std::runtime_error(
                    "TLS relocation references a non-TLS symbol");
            }

            if (symbol.IsUndefined()) {
                throw std::runtime_error(
                    "External TLS symbols require module-loader TLS support");
            }

            if (symbol.sectionIndex >= kSectionReserved) {
                throw std::runtime_error(
                    "Unsupported TLS symbol section index");
            }

            if (symbol.value > image.tlsSegment->memorySize ||
                symbol.size >
                    image.tlsSegment->memorySize - symbol.value) {
                throw std::runtime_error(
                    "TLS symbol exceeds the PT_TLS block");
            }

            moduleOffset = symbol.value;
        }

        if (moduleOffset > tlsLayout->blockSize) {
            throw std::runtime_error(
                "TLS symbol offset exceeds the runtime TLS block");
        }

        const auto signedBlockSize =
            static_cast<std::int64_t>(tlsLayout->blockSize);
        const auto signedModuleOffset =
            static_cast<std::int64_t>(moduleOffset);

        const loader::TlsSymbolResolution resolution{
            .moduleId = kMainTlsModuleId,
            .moduleOffset = moduleOffset,
            .threadPointerOffset =
                signedModuleOffset - signedBlockSize,
        };

        tlsAddresses.emplace(index, resolution);
        return resolution;
    };

    loader::RelocationApplier::Apply(relocations, staged, context);
    result.appliedRelocationCount = static_cast<std::size_t>(std::count_if(
        relocations.begin(), relocations.end(),
        [](const elf::Relocation& relocation) { return relocation.type != 0; }));
    memory = std::move(staged);
    return result;
}

} // namespace ps5emu::runtime
