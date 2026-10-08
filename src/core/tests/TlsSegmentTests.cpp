#include <ps5emu/elf/Elf64.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes,
           std::size_t offset,
           T value) {
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

std::vector<std::byte> MakeElf() {
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
    Write<std::uint64_t>(bytes, load + 32, 0x80);
    Write<std::uint64_t>(bytes, load + 40, 0x80);
    Write<std::uint64_t>(bytes, load + 48, 0x1000);

    const std::size_t tls = 64 + 56;
    Write<std::uint32_t>(bytes, tls + 0, 7);
    Write<std::uint32_t>(bytes, tls + 4, 4);
    Write<std::uint64_t>(bytes, tls + 8, 0x180);
    Write<std::uint64_t>(bytes, tls + 16, 0);
    Write<std::uint64_t>(bytes, tls + 32, 4);
    Write<std::uint64_t>(bytes, tls + 40, 16);
    Write<std::uint64_t>(bytes, tls + 48, 16);

    bytes[0x180] = std::byte{0x11};
    bytes[0x181] = std::byte{0x22};
    bytes[0x182] = std::byte{0x33};
    bytes[0x183] = std::byte{0x44};

    return bytes;
}

} // namespace

int main() {
    {
        const auto bytes = MakeElf();
        const auto image = ps5emu::elf::Elf64::Parse(bytes);

        assert(image.tlsSegment.has_value());
        assert(image.tlsSegment->fileOffset == 0x180);
        assert(image.tlsSegment->fileSize == 4);
        assert(image.tlsSegment->memorySize == 16);
        assert(image.tlsSegment->alignment == 16);
    }

    {
        auto bytes = MakeElf();
        const std::size_t tls = 64 + 56;
        Write<std::uint64_t>(bytes, tls + 32, 17);
        Write<std::uint64_t>(bytes, tls + 40, 16);

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::elf::Elf64::Parse(bytes));
        }));
    }

    {
        auto bytes = MakeElf();
        const std::size_t tls = 64 + 56;
        Write<std::uint64_t>(bytes, tls + 48, 24);

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::elf::Elf64::Parse(bytes));
        }));
    }

    return 0;
}
