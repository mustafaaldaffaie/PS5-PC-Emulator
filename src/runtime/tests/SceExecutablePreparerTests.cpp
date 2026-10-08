#include <ps5emu/runtime/SceExecutablePreparer.hpp>

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

void WriteProgramHeader(std::vector<std::byte>& bytes,
                        std::size_t offset,
                        std::uint32_t type,
                        std::uint32_t flags,
                        std::uint64_t fileOffset,
                        std::uint64_t virtualAddress,
                        std::uint64_t fileSize,
                        std::uint64_t memorySize,
                        std::uint64_t alignment) {
    Write<std::uint32_t>(bytes, offset + 0, type);
    Write<std::uint32_t>(bytes, offset + 4, flags);
    Write<std::uint64_t>(bytes, offset + 8, fileOffset);
    Write<std::uint64_t>(bytes, offset + 16, virtualAddress);
    Write<std::uint64_t>(bytes, offset + 24, 0);
    Write<std::uint64_t>(bytes, offset + 32, fileSize);
    Write<std::uint64_t>(bytes, offset + 40, memorySize);
    Write<std::uint64_t>(bytes, offset + 48, alignment);
}

void WriteDynamicEntry(std::vector<std::byte>& bytes,
                       std::size_t offset,
                       std::int64_t tag,
                       std::uint64_t value) {
    Write<std::int64_t>(bytes, offset, tag);
    Write<std::uint64_t>(bytes, offset + 8, value);
}

std::uint64_t PackModuleRecord(
    std::uint16_t id,
    std::uint8_t major,
    std::uint8_t minor,
    std::uint32_t nameOffset) {
    return (static_cast<std::uint64_t>(id) << 48) |
           (static_cast<std::uint64_t>(major) << 40) |
           (static_cast<std::uint64_t>(minor) << 32) |
           nameOffset;
}

std::uint64_t PackLibraryRecord(
    std::uint16_t id,
    std::uint16_t version,
    std::uint32_t nameOffset) {
    return (static_cast<std::uint64_t>(id) << 48) |
           (static_cast<std::uint64_t>(version) << 32) |
           nameOffset;
}

std::vector<std::byte> MakeImportingElf() {
    std::vector<std::byte> bytes(0x800);

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

    WriteProgramHeader(
        bytes,
        64,
        1,
        7,
        0x100,
        0x400000,
        0x600,
        0x600,
        0x1000);

    WriteProgramHeader(
        bytes,
        64 + 56,
        2,
        4,
        0x180,
        0x400080,
        0xa0,
        0xa0,
        8);

    WriteDynamicEntry(bytes, 0x180, 5, 0x400300);
    WriteDynamicEntry(bytes, 0x190, 10, 40);
    WriteDynamicEntry(bytes, 0x1a0, 6, 0x400380);
    WriteDynamicEntry(bytes, 0x1b0, 11, 24);
    WriteDynamicEntry(bytes, 0x1c0, 7, 0x400400);
    WriteDynamicEntry(bytes, 0x1d0, 8, 24);
    WriteDynamicEntry(bytes, 0x1e0, 9, 24);
    WriteDynamicEntry(
        bytes,
        0x1f0,
        0x61000045,
        PackModuleRecord(2, 1, 0, 1));
    WriteDynamicEntry(
        bytes,
        0x200,
        0x61000049,
        PackLibraryRecord(1, 1, 11));
    WriteDynamicEntry(bytes, 0x210, 0, 0);

    const char strings[] =
        "\0libkernel\0libSceKernel\0ABCDEFGHIJK#B#C\0";
    static_assert(sizeof(strings) - 1 == 40);
    std::memcpy(
        bytes.data() + 0x400,
        strings,
        sizeof(strings) - 1);

    const std::size_t symbol = 0x480 + 24;
    Write<std::uint32_t>(bytes, symbol + 0, 24);
    bytes[symbol + 4] = std::byte{0x12};
    bytes[symbol + 5] = std::byte{0};
    Write<std::uint16_t>(bytes, symbol + 6, 0);
    Write<std::uint64_t>(bytes, symbol + 8, 0);
    Write<std::uint64_t>(bytes, symbol + 16, 0);

    const std::size_t relocation = 0x500;
    Write<std::uint64_t>(
        bytes,
        relocation + 0,
        0x400550);
    Write<std::uint64_t>(
        bytes,
        relocation + 8,
        (static_cast<std::uint64_t>(1) << 32) | 7);
    Write<std::int64_t>(
        bytes,
        relocation + 16,
        0);

    return bytes;
}

std::uint64_t ReadU64(
    const ps5emu::memory::GuestMemory& memory,
    std::uint64_t address) {
    const auto bytes =
        memory.Read(address, sizeof(std::uint64_t));

    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
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

ps5emu::hle::HleRegistry MakeRegistry() {
    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "ABCDEFGHIJK",
        "sceKernelSynthetic",
        [](ps5emu::hle::HleCallFrame& frame) {
            frame.returnValue =
                frame.arguments[0] + frame.arguments[1];
        });
    return registry;
}

} // namespace

int main() {
    const auto bytes = MakeImportingElf();

    {
        auto registry = MakeRegistry();
        ps5emu::memory::GuestMemory memory;
        ps5emu::runtime::HleThunkTable thunks(
            ps5emu::runtime::HleThunkTableOptions{
                .baseAddress = 0x90000000,
                .slotSize = 16,
                .capacity = 4,
            });

        const auto prepared =
            ps5emu::runtime::SceExecutablePreparer::Prepare(
                bytes,
                memory,
                registry,
                thunks);

        assert(prepared.linked.loaded.entryPoint == 0x400000);
        assert(prepared.linked.appliedRelocationCount == 1);
        assert(prepared.resolvedHleImportCount == 1);
        assert(prepared.unresolvedImportCount == 0);
        assert(prepared.newThunkCount == 1);
        assert(thunks.Size() == 1);

        assert(ReadU64(memory, 0x400550) == 0x90000000);

        const auto* thunk =
            thunks.FindByAddress(0x90000000);
        assert(thunk != nullptr);
        assert(thunk->module == "libkernel");
        assert(thunk->nid == "ABCDEFGHIJK");
        assert(thunk->debugName == "sceKernelSynthetic");

        const auto trap = memory.Read(0x90000000, 16);
        for (const auto byte : trap) {
            assert(byte == std::byte{0xcc});
        }

        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 20;
        frame.arguments[1] = 22;

        assert(thunks.Dispatch(
            0x90000000,
            registry,
            frame));
        assert(frame.returnValue == 42);

        const auto* service =
            registry.Find("libkernel", "ABCDEFGHIJK");
        assert(service != nullptr);
        assert(thunks.Bind(*service) == 0x90000000);
        assert(thunks.Size() == 1);
    }

    {
        ps5emu::hle::HleRegistry registry;
        ps5emu::memory::GuestMemory memory;
        memory.Map(
            0x1000,
            16,
            ps5emu::memory::Protection::Read |
                ps5emu::memory::Protection::Write);

        ps5emu::runtime::HleThunkTable thunks(
            ps5emu::runtime::HleThunkTableOptions{
                .baseAddress = 0x90000000,
                .slotSize = 16,
                .capacity = 4,
            });

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::runtime::SceExecutablePreparer::Prepare(
                    bytes,
                    memory,
                    registry,
                    thunks));
        }));

        assert(memory.Mappings().size() == 1);
        assert(memory.Mappings()[0].guestAddress == 0x1000);
        assert(thunks.Size() == 0);
    }

    {
        auto registry = MakeRegistry();
        ps5emu::memory::GuestMemory memory;

        ps5emu::runtime::HleThunkTable thunks(
            ps5emu::runtime::HleThunkTableOptions{
                .baseAddress = 0x400500,
                .slotSize = 16,
                .capacity = 4,
            });

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                ps5emu::runtime::SceExecutablePreparer::Prepare(
                    bytes,
                    memory,
                    registry,
                    thunks));
        }));

        assert(memory.Mappings().empty());
        assert(thunks.Size() == 0);
    }

    return 0;
}
