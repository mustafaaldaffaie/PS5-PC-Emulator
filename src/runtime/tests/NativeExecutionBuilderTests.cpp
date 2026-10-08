#include <ps5emu/runtime/NativeExecutionBuilder.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

std::vector<std::byte> MakeElf(
    std::uint64_t codeAddress) {
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
    Write<std::uint64_t>(bytes, 24, codeAddress);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 2);

    const std::size_t load = 64;
    Write<std::uint32_t>(bytes, load + 0, 1);
    Write<std::uint32_t>(bytes, load + 4, 5);
    Write<std::uint64_t>(bytes, load + 8, 0x100);
    Write<std::uint64_t>(bytes, load + 16, codeAddress);
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

    const std::array<std::byte, 6> code{
        std::byte{0xb8},
        std::byte{0x2a},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0xc3},
    };

    std::memcpy(
        bytes.data() + 0x100,
        code.data(),
        code.size());

    bytes[0x180] = std::byte{0x11};
    bytes[0x181] = std::byte{0x22};
    bytes[0x182] = std::byte{0x33};
    bytes[0x183] = std::byte{0x44};

    return bytes;
}

} // namespace

int main() {
    using ps5emu::runtime::GuestExecutionOptions;
    using ps5emu::runtime::NativeExecutionBuilder;

    constexpr std::uint64_t reservationBase =
        0x0000200001000000ull;
    constexpr std::uint64_t codeAddress =
        reservationBase + 0x1000ull;
    constexpr std::uint64_t stackAddress =
        reservationBase + 0x200000ull;
    constexpr std::uint64_t tlsAddress =
        reservationBase + 0x300010ull;

    const auto bytes =
        MakeElf(codeAddress);

    GuestExecutionOptions options;
    options.threadMemory.stackAddress =
        stackAddress;
    options.threadMemory.stackSize =
        0x2000;
    options.threadMemory.tlsAddress =
        tlsAddress;
    options.threadMemory.stackGuard =
        0x8877665544332211ull;

    auto execution =
        NativeExecutionBuilder::Prepare(
            bytes,
            options);

    assert(
        execution.guest.context.rip ==
        codeAddress);

    assert(
        execution.guest.context.rsp ==
        stackAddress + 0x2000);

    assert(
        execution.guest.context.fsBase ==
        tlsAddress + 0x10);

    assert(
        execution.nativeImage.Contains(
            execution.guest.context.rip,
            6));

    assert(
        execution.nativeImage.Contains(
            execution.guest.context.rsp - 1,
            1));

    assert(
        execution.nativeImage.Contains(
            execution.guest.context.fsBase,
            sizeof(std::uint64_t)));

    const auto* tlsBytes =
        static_cast<const std::byte*>(
            execution.nativeImage.HostAddress(
                tlsAddress,
                4));

    assert(tlsBytes[0] == std::byte{0x11});
    assert(tlsBytes[3] == std::byte{0x44});

    auto* stackWord =
        static_cast<std::uint64_t*>(
            execution.nativeImage.HostAddress(
                execution.guest.context.rsp -
                    sizeof(std::uint64_t),
                sizeof(std::uint64_t)));

    *stackWord = 0x123456789abcdef0ull;
    assert(*stackWord == 0x123456789abcdef0ull);

#if defined(_M_X64) || defined(__x86_64__)
    using LeafFunction = std::uint64_t (*)();

    const auto function =
        reinterpret_cast<LeafFunction>(
            execution.nativeImage.HostAddress(
                codeAddress,
                6));

    assert(function() == 42);
#endif

    return 0;
}
