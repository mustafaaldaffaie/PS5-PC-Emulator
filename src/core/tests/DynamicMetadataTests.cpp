#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    assert(offset + sizeof(T) <= bytes.size());
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void WriteDynamicEntry(std::vector<std::byte>& bytes,
                       std::size_t offset,
                       std::int64_t tag,
                       std::uint64_t value) {
    Write<std::int64_t>(bytes, offset, tag);
    Write<std::uint64_t>(bytes, offset + 8, value);
}

std::vector<std::byte> MakeDynamicElf() {
    std::vector<std::byte> bytes(0x300);

    bytes[0] = std::byte{0x7f};
    bytes[1] = std::byte{'E'};
    bytes[2] = std::byte{'L'};
    bytes[3] = std::byte{'F'};
    bytes[4] = std::byte{2};
    bytes[5] = std::byte{1};
    bytes[6] = std::byte{1};

    Write<std::uint16_t>(bytes, 16, 2);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 24, 0x400000);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 2);

    const std::size_t load = 64;
    Write<std::uint32_t>(bytes, load + 0, 1);
    Write<std::uint32_t>(bytes, load + 4, 5);
    Write<std::uint64_t>(bytes, load + 8, 0x100);
    Write<std::uint64_t>(bytes, load + 16, 0x400000);
    Write<std::uint64_t>(bytes, load + 32, 0x100);
    Write<std::uint64_t>(bytes, load + 40, 0x100);
    Write<std::uint64_t>(bytes, load + 48, 0x1000);

    const std::size_t dynamic = 64 + 56;
    Write<std::uint32_t>(bytes, dynamic + 0, 2);
    Write<std::uint32_t>(bytes, dynamic + 4, 4);
    Write<std::uint64_t>(bytes, dynamic + 8, 0x140);
    Write<std::uint64_t>(bytes, dynamic + 16, 0x400040);
    Write<std::uint64_t>(bytes, dynamic + 32, 0x60);
    Write<std::uint64_t>(bytes, dynamic + 40, 0x60);
    Write<std::uint64_t>(bytes, dynamic + 48, 8);

    WriteDynamicEntry(bytes, 0x140, 5, 0x4000c0);
    WriteDynamicEntry(bytes, 0x150, 10, 24);
    WriteDynamicEntry(bytes, 0x160, 1, 1);
    WriteDynamicEntry(bytes, 0x170, 1, 15);
    WriteDynamicEntry(bytes, 0x180, 6, 0x4000e0);
    WriteDynamicEntry(bytes, 0x190, 0, 0);

    const char strings[] = "\0libkernel.prx\0libc.prx\0";
    static_assert(sizeof(strings) - 1 == 24);
    std::memcpy(bytes.data() + 0x1c0, strings, sizeof(strings) - 1);

    return bytes;
}

} // namespace

int main() {
    {
        const auto bytes = MakeDynamicElf();
        const auto image = ps5emu::elf::Elf64::Parse(bytes);
        assert(image.dynamicSegment.has_value());

        const auto metadata =
            ps5emu::elf::DynamicMetadataParser::Parse(bytes, image);

        assert(metadata.stringTableAddress == 0x4000c0);
        assert(metadata.stringTableSize == 24);
        assert(metadata.symbolTableAddress == 0x4000e0);
        assert(metadata.neededLibraries.size() == 2);
        assert(metadata.neededLibraries[0] == "libkernel.prx");
        assert(metadata.neededLibraries[1] == "libc.prx");
    }

    {
        auto bytes = MakeDynamicElf();
        WriteDynamicEntry(bytes, 0x150, 10, 0);

        const auto image = ps5emu::elf::Elf64::Parse(bytes);
        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::elf::DynamicMetadataParser::Parse(bytes, image));
        }));
    }

    return 0;
}
