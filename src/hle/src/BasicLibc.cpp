#include <ps5emu/hle/BasicLibc.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ps5emu::hle {
namespace {

std::size_t CheckedSize(std::uint64_t value) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "Guest libc memory operation size is too large for this host");
    }

    return static_cast<std::size_t>(value);
}

GuestMemoryAccess& RequireMemory(HleCallFrame& frame) {
    if (frame.memory == nullptr) {
        throw std::runtime_error(
            "Guest libc memory operation requires guest memory access");
    }

    return *frame.memory;
}

void CopyMemory(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto source = frame.arguments[1];
    const auto size = CheckedSize(frame.arguments[2]);

    frame.returnValue = destination;

    if (size == 0) {
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size);
    memory.Read(source, buffer);
    memory.Write(destination, buffer);
}

void SetMemory(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto value =
        static_cast<std::byte>(frame.arguments[1] & 0xffu);
    const auto size = CheckedSize(frame.arguments[2]);

    frame.returnValue = destination;

    if (size == 0) {
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size, value);
    memory.Write(destination, buffer);
}

std::byte ReadByte(GuestMemoryAccess& memory,
                   std::uint64_t address) {
    std::array<std::byte, 1> value{};
    memory.Read(address, value);
    return value[0];
}

std::uint64_t CheckedAddress(std::uint64_t base,
                             std::uint64_t offset) {
    if (offset >
        std::numeric_limits<std::uint64_t>::max() - base) {
        throw std::runtime_error(
            "Guest libc string address overflows");
    }

    return base + offset;
}

void CompareMemory(HleCallFrame& frame) {
    const auto left = frame.arguments[0];
    const auto right = frame.arguments[1];
    const auto size = CheckedSize(frame.arguments[2]);

    if (size == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);

    std::vector<std::byte> leftBytes(size);
    std::vector<std::byte> rightBytes(size);
    memory.Read(left, leftBytes);
    memory.Read(right, rightBytes);

    for (std::size_t index = 0; index < size; ++index) {
        const auto lhs =
            std::to_integer<unsigned int>(leftBytes[index]);
        const auto rhs =
            std::to_integer<unsigned int>(rightBytes[index]);

        if (lhs == rhs) {
            continue;
        }

        const std::int64_t result =
            lhs < rhs ? -1 : 1;
        frame.returnValue =
            static_cast<std::uint64_t>(result);
        return;
    }

    frame.returnValue = 0;
}

void StringLength(HleCallFrame& frame) {
    const auto address = frame.arguments[0];
    auto& memory = RequireMemory(frame);

    std::uint64_t length = 0;
    while (true) {
        const auto currentAddress =
            CheckedAddress(address, length);

        if (ReadByte(memory, currentAddress) == std::byte{0}) {
            frame.returnValue = length;
            return;
        }

        if (length ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc string length overflows");
        }

        ++length;
    }
}

void StringLengthBounded(HleCallFrame& frame) {
    const auto address = frame.arguments[0];
    const auto maximum = frame.arguments[1];

    if (maximum == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);

    for (std::uint64_t length = 0;
         length < maximum;
         ++length) {
        const auto currentAddress =
            CheckedAddress(address, length);

        if (ReadByte(memory, currentAddress) == std::byte{0}) {
            frame.returnValue = length;
            return;
        }
    }

    frame.returnValue = maximum;
}

} // namespace

void BasicLibc::Register(HleRegistry& registry,
                         std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Basic libc module name cannot be empty");
    }

    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "memcpy",
        CopyMemory);

    registry.RegisterSymbol(
        moduleName,
        "memmove",
        CopyMemory);

    registry.RegisterSymbol(
        moduleName,
        "memset",
        SetMemory);

    registry.RegisterSymbol(
        moduleName,
        "memcmp",
        CompareMemory);

    registry.RegisterSymbol(
        moduleName,
        "strlen",
        StringLength);

    registry.RegisterSymbol(
        std::move(module),
        "strnlen",
        StringLengthBounded);
}

} // namespace ps5emu::hle
