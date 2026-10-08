#include <ps5emu/runtime/GuestExecutionBuilder.hpp>

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
    std::memcpy(
        bytes.data() + offset,
        &value,
        sizeof(T));
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

    bytes[0x100] = std::byte{0x90};
    bytes[0x101] = std::byte{0xc3};

    bytes[0x180] = std::byte{0x11};
    bytes[0x181] = std::byte{0x22};
    bytes[0x182] = std::byte{0x33};
    bytes[0x183] = std::byte{0x44};

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
    using ps5emu::runtime::GuestExecutionBuilder;
    using ps5emu::runtime::GuestExecutionOptions;

    const auto bytes = MakeElf();

    GuestExecutionOptions options;
    options.threadMemory.stackAddress = 0x70000000;
    options.threadMemory.stackSize = 0x2000;
    options.threadMemory.tlsAddress = 0x71000000;
    options.threadMemory.stackGuard =
        0x1122334455667788ull;

    const auto execution =
        GuestExecutionBuilder::Prepare(
            bytes,
            options);

    assert(execution.registry.Size() == 21);
    assert(execution.thunks.Size() == 0);

    assert(
        execution.image.linked.loaded.entryPoint ==
        0x400000);

    assert(
        execution.context.rip ==
        0x400000);

    assert(
        execution.context.rsp ==
        0x70002000);

    assert(
        execution.threadMemory.threadPointer ==
        0x71000010);

    assert(
        execution.context.fsBase ==
        0x71000010);

    assert(
        execution.memory.Read(
            0x71000000,
            4)[0] ==
        std::byte{0x11});

    assert(
        ReadU64(
            execution.memory,
            0x71000010) ==
        0x71000010);

    assert(
        ReadU64(
            execution.memory,
            0x71000010 + 0x28) ==
        0x1122334455667788ull);

    assert(
        execution.memory.IsMapped(
            0x70000000,
            0x2000));

    assert(
        execution.memory.IsMapped(
            0x71000000,
            0x40));

    {
        auto biased = options;
        biased.loadBias = 0x100000;

        const auto executionWithBias =
            GuestExecutionBuilder::Prepare(
                bytes,
                biased);

        assert(
            executionWithBias.context.rip ==
            0x500000);

        assert(
            executionWithBias.context.rsp ==
            0x70002000);

        assert(
            executionWithBias.context.fsBase ==
            0x71000010);
    }

    {
        auto invalid = options;
        invalid.threadMemory.stackSize = 0;

        assert(Throws([&] {
            static_cast<void>(
                GuestExecutionBuilder::Prepare(
                    bytes,
                    invalid));
        }));
    }

    return 0;
}
