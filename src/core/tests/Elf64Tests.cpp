#include <ps5emu/elf/Elf64.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

std::vector<std::byte> MakeMinimalElf() {
    std::vector<std::byte> bytes(0x200);

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
    Write<std::uint64_t>(bytes, 24, 0x401000);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 1);

    const std::size_t ph = 64;
    Write<std::uint32_t>(bytes, ph + 0, 1);
    Write<std::uint32_t>(bytes, ph + 4, 5);
    Write<std::uint64_t>(bytes, ph + 8, 0x100);
    Write<std::uint64_t>(bytes, ph + 16, 0x400000);
    Write<std::uint64_t>(bytes, ph + 32, 4);
    Write<std::uint64_t>(bytes, ph + 40, 8);
    Write<std::uint64_t>(bytes, ph + 48, 0x1000);

    bytes[0x100] = std::byte{0xaa};
    bytes[0x101] = std::byte{0xbb};
    bytes[0x102] = std::byte{0xcc};
    bytes[0x103] = std::byte{0xdd};

    return bytes;
}

} // namespace

int main() {
    auto bytes = MakeMinimalElf();

    const auto image = ps5emu::elf::Elf64::Parse(bytes);
    assert(image.entryPoint == 0x401000);
    assert(image.loadSegments.size() == 1);

    const auto& segment = image.loadSegments.front();
    assert(segment.virtualAddress == 0x400000);
    assert(segment.fileOffset == 0x100);
    assert(segment.fileSize == 4);
    assert(segment.memorySize == 8);
    assert(segment.flags == 5);
    assert(segment.alignment == 0x1000);

    auto invalidMagic = bytes;
    invalidMagic[0] = std::byte{0};
    assert(ThrowsRuntimeError([&] {
        static_cast<void>(ps5emu::elf::Elf64::Parse(invalidMagic));
    }));

    auto wrongMachine = bytes;
    Write<std::uint16_t>(wrongMachine, 18, 183);
    assert(ThrowsRuntimeError([&] {
        static_cast<void>(ps5emu::elf::Elf64::Parse(wrongMachine));
    }));

    auto oversizedSegment = bytes;
    Write<std::uint64_t>(oversizedSegment, 64 + 32, 16);
    Write<std::uint64_t>(oversizedSegment, 64 + 40, 8);
    assert(ThrowsRuntimeError([&] {
        static_cast<void>(ps5emu::elf::Elf64::Parse(oversizedSegment));
    }));

    return 0;
}
