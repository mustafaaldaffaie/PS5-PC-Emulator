#include <ps5emu/runtime/GuestModuleLoader.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes,
           std::size_t offset,
           T value) {
    assert(offset + sizeof(T) <=
           bytes.size());
    std::memcpy(
        bytes.data() + offset,
        &value,
        sizeof(T));
}

void ProgramHeader(
    std::vector<std::byte>& bytes,
    std::size_t index,
    std::uint32_t type,
    std::uint32_t flags,
    std::uint64_t fileOffset,
    std::uint64_t virtualAddress,
    std::uint64_t fileSize,
    std::uint64_t memorySize,
    std::uint64_t alignment) {
    const auto offset =
        64 + index * 56;

    Write<std::uint32_t>(
        bytes, offset + 0, type);
    Write<std::uint32_t>(
        bytes, offset + 4, flags);
    Write<std::uint64_t>(
        bytes, offset + 8, fileOffset);
    Write<std::uint64_t>(
        bytes, offset + 16, virtualAddress);
    Write<std::uint64_t>(
        bytes, offset + 24, 0);
    Write<std::uint64_t>(
        bytes, offset + 32, fileSize);
    Write<std::uint64_t>(
        bytes, offset + 40, memorySize);
    Write<std::uint64_t>(
        bytes, offset + 48, alignment);
}

void Dynamic(
    std::vector<std::byte>& bytes,
    std::size_t index,
    std::int64_t tag,
    std::uint64_t value) {
    const auto offset =
        0x180 + index * 16;
    Write<std::int64_t>(
        bytes,
        offset,
        tag);
    Write<std::uint64_t>(
        bytes,
        offset + 8,
        value);
}

std::uint64_t PackModule(
    std::uint16_t id,
    std::uint32_t nameOffset) {
    return
        (static_cast<std::uint64_t>(id) << 48) |
        (std::uint64_t{1} << 40) |
        nameOffset;
}

struct StringTable {
    std::vector<char> bytes{0};

    std::uint32_t Add(
        const std::string& value) {
        const auto offset =
            static_cast<std::uint32_t>(
                bytes.size());
        bytes.insert(
            bytes.end(),
            value.begin(),
            value.end());
        bytes.push_back('\0');
        return offset;
    }
};

std::vector<std::byte> MakeModule(
    const std::string& moduleName,
    const std::string& symbolName,
    bool definedSymbol,
    bool addNeededModule) {
    std::vector<std::byte> bytes(0x900);

    bytes[0] = std::byte{0x7f};
    bytes[1] = std::byte{'E'};
    bytes[2] = std::byte{'L'};
    bytes[3] = std::byte{'F'};
    bytes[4] = std::byte{2};
    bytes[5] = std::byte{1};
    bytes[6] = std::byte{1};

    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 24, 0x400010);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 2);

    ProgramHeader(
        bytes,
        0,
        1,
        5,
        0x100,
        0x400000,
        0x800,
        0x800,
        0x1000);

    StringTable strings;
    const auto moduleOffset =
        strings.Add(moduleName);
    const auto symbolOffset =
        strings.Add(symbolName);

    std::uint32_t neededOffset = 0;
    if (addNeededModule) {
        neededOffset =
            strings.Add("libAlpha");
    }

    constexpr std::uint64_t kStringAddress =
        0x400400;
    constexpr std::uint64_t kSymbolAddress =
        0x400500;
    constexpr std::uint64_t kRelaAddress =
        0x400600;

    std::size_t dynamicCount = 0;
    Dynamic(
        bytes,
        dynamicCount++,
        5,
        kStringAddress);
    Dynamic(
        bytes,
        dynamicCount++,
        10,
        strings.bytes.size());
    Dynamic(
        bytes,
        dynamicCount++,
        6,
        kSymbolAddress);
    Dynamic(
        bytes,
        dynamicCount++,
        11,
        24);
    Dynamic(
        bytes,
        dynamicCount++,
        0x6100003f,
        48);
    Dynamic(
        bytes,
        dynamicCount++,
        0x61000043,
        PackModule(
            addNeededModule ? 9 : 4,
            moduleOffset));

    if (addNeededModule) {
        Dynamic(
            bytes,
            dynamicCount++,
            0x61000045,
            PackModule(
                2,
                neededOffset));

        Dynamic(
            bytes,
            dynamicCount++,
            7,
            kRelaAddress);
        Dynamic(
            bytes,
            dynamicCount++,
            8,
            24);
        Dynamic(
            bytes,
            dynamicCount++,
            9,
            24);
    }

    Dynamic(
        bytes,
        dynamicCount++,
        0,
        0);

    const auto dynamicSize =
        dynamicCount * 16;

    ProgramHeader(
        bytes,
        1,
        2,
        4,
        0x180,
        0x400080,
        dynamicSize,
        dynamicSize,
        8);

    std::memcpy(
        bytes.data() + 0x500,
        strings.bytes.data(),
        strings.bytes.size());

    const auto symbolFileOffset =
        std::size_t{0x600} + 24;

    Write<std::uint32_t>(
        bytes,
        symbolFileOffset,
        symbolOffset);
    bytes[symbolFileOffset + 4] =
        std::byte{0x12};
    Write<std::uint16_t>(
        bytes,
        symbolFileOffset + 6,
        definedSymbol ? 1 : 0);
    Write<std::uint64_t>(
        bytes,
        symbolFileOffset + 8,
        definedSymbol
            ? 0x400350
            : 0);
    Write<std::uint64_t>(
        bytes,
        symbolFileOffset + 16,
        8);

    if (!definedSymbol) {
        Write<std::uint64_t>(
            bytes,
            0x700,
            0x400300);
        Write<std::uint64_t>(
            bytes,
            0x708,
            (std::uint64_t{1} << 32) |
                7u);
        Write<std::int64_t>(
            bytes,
            0x710,
            0);
    }

    return bytes;
}

std::uint64_t ReadU64(
    const ps5emu::memory::GuestMemory& memory,
    std::uint64_t address) {
    const auto bytes =
        memory.Read(
            address,
            sizeof(std::uint64_t));

    std::uint64_t value = 0;
    std::memcpy(
        &value,
        bytes.data(),
        sizeof(value));
    return value;
}

template <typename Function>
bool Throws(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

} // namespace

int main() {
    const auto provider =
        MakeModule(
            "libAlpha",
            "ABCDEFGHIJK#D#E",
            true,
            false);

    const auto importer =
        MakeModule(
            "eboot",
            "ABCDEFGHIJK#B#C",
            false,
            true);

    ps5emu::runtime::GuestModuleLoader loader;

    const auto first =
        loader.Load(
            provider,
            ps5emu::runtime::GuestModuleLoadOptions{
                .loadBias = 0x100000,
            });

    assert(first.name == "libAlpha");
    assert(loader.Catalog().ModuleCount() == 1);
    assert(loader.Memory().Mappings().size() == 1);

    const auto second =
        loader.Load(
            importer,
            ps5emu::runtime::GuestModuleLoadOptions{
                .loadBias = 0x200000,
            });

    assert(second.name == "eboot");
    assert(second.linked.resolvedExternalSymbolCount == 1);
    assert(loader.Catalog().ModuleCount() == 2);
    assert(loader.Memory().Mappings().size() == 2);

    assert(
        ReadU64(
            loader.Memory(),
            0x600300) ==
        0x500350);

    const auto mappingsBefore =
        loader.Memory().Mappings().size();
    const auto modulesBefore =
        loader.Catalog().ModuleCount();

    assert(Throws([&] {
        static_cast<void>(
            loader.Load(
                provider,
                ps5emu::runtime::GuestModuleLoadOptions{
                    .loadBias = 0x300000,
                }));
    }));

    assert(
        loader.Memory().Mappings().size() ==
        mappingsBefore);
    assert(
        loader.Catalog().ModuleCount() ==
        modulesBefore);

    {
        ps5emu::runtime::GuestModuleLoader empty;

        assert(Throws([&] {
            static_cast<void>(
                empty.Load(
                    importer,
                    ps5emu::runtime::GuestModuleLoadOptions{
                        .loadBias = 0x200000,
                    }));
        }));

        assert(empty.Memory().Mappings().empty());
        assert(empty.Catalog().ModuleCount() == 0);
    }

    return 0;
}
