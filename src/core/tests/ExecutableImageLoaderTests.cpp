#include <ps5emu/loader/ExecutableImageLoader.hpp>

#include <ps5emu/memory/GuestMemory.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
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

std::vector<std::byte> MakeExecutableElf(std::uint64_t entryPoint = 0x400002) {
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
    Write<std::uint64_t>(bytes, 24, entryPoint);
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

    bytes[0x100] = std::byte{0xde};
    bytes[0x101] = std::byte{0xad};
    bytes[0x102] = std::byte{0xbe};
    bytes[0x103] = std::byte{0xef};

    return bytes;
}

} // namespace

int main() {
    {
        ps5emu::memory::GuestMemory memory;
        const auto bytes = MakeExecutableElf();

        const auto loaded =
            ps5emu::loader::ExecutableImageLoader::LoadElf(bytes, memory);

        assert(loaded.entryPoint == 0x400002);
        assert(loaded.mappedSegmentCount == 1);
        assert(memory.Mappings().size() == 1);

        const auto& mapping = memory.Mappings().front();
        assert(mapping.guestAddress == 0x400000);
        assert(mapping.size == 8);
        assert(ps5emu::memory::HasProtection(
            mapping.protection, ps5emu::memory::Protection::Read));
        assert(ps5emu::memory::HasProtection(
            mapping.protection, ps5emu::memory::Protection::Execute));
        assert(!ps5emu::memory::HasProtection(
            mapping.protection, ps5emu::memory::Protection::Write));

        const auto contents = memory.Read(0x400000, 8);
        assert(contents[0] == std::byte{0xde});
        assert(contents[1] == std::byte{0xad});
        assert(contents[2] == std::byte{0xbe});
        assert(contents[3] == std::byte{0xef});

        for (std::size_t index = 4; index < contents.size(); ++index) {
            assert(contents[index] == std::byte{0});
        }

        const std::byte replacement{0x90};
        assert(ThrowsRuntimeError([&] {
            memory.Write(
                0x400000,
                std::span<const std::byte>(&replacement, 1));
        }));
    }

    {
        ps5emu::memory::GuestMemory memory;
        const auto bytes = MakeExecutableElf(0x500000);

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::loader::ExecutableImageLoader::LoadElf(bytes, memory));
        }));
        assert(memory.Mappings().empty());
    }

    {
        ps5emu::memory::GuestMemory memory;
        const auto loaded = ps5emu::loader::ExecutableImageLoader::LoadElf(
            MakeExecutableElf(), memory, 0x100000);
        assert(loaded.entryPoint == 0x500002);
        assert(memory.Read(0x500000, 1)[0] == std::byte{0xde});
    }

    {
        // A conflict in the second segment must roll back the first one.
        auto bytes = MakeExecutableElf();
        Write<std::uint16_t>(bytes, 56, 2);
        std::memcpy(bytes.data() + 120, bytes.data() + 64, 56);
        Write<std::uint64_t>(bytes, 136, 0x600000);
        ps5emu::memory::GuestMemory memory;
        memory.Map(0x600000, 8, ps5emu::memory::Protection::Read);
        const std::byte marker{0x55};
        memory.Initialize(0x600000, std::span(&marker, 1));
        assert(ThrowsRuntimeError([&] {
            static_cast<void>(ps5emu::loader::ExecutableImageLoader::LoadElf(
                bytes, memory));
        }));
        assert(memory.Mappings().size() == 1);
        assert(memory.Read(0x600000, 1)[0] == marker);
        assert(!memory.IsMapped(0x400000, 8));
    }

    {
        // A late address overflow must also leave existing mappings intact.
        auto bytes = MakeExecutableElf();
        Write<std::uint16_t>(bytes, 56, 2);
        std::memcpy(bytes.data() + 120, bytes.data() + 64, 56);
        Write<std::uint64_t>(bytes, 136,
                            std::numeric_limits<std::uint64_t>::max() - 3);
        ps5emu::memory::GuestMemory memory;
        assert(ThrowsRuntimeError([&] {
            static_cast<void>(ps5emu::loader::ExecutableImageLoader::LoadElf(
                bytes, memory));
        }));
        assert(memory.Mappings().empty());
        assert(ThrowsRuntimeError([&] {
            static_cast<void>(ps5emu::loader::ExecutableImageLoader::LoadElf(
                MakeExecutableElf(), memory,
                std::numeric_limits<std::uint64_t>::max()));
        }));
        assert(memory.Mappings().empty());
    }

    return 0;
}
