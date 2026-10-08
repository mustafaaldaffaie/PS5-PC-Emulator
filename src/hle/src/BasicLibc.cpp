#include <ps5emu/hle/BasicLibc.hpp>

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

} // namespace

void BasicLibc::Register(HleRegistry& registry,
                         std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Basic libc module name cannot be empty");
    }

    registry.RegisterSymbol(
        module,
        "memcpy",
        CopyMemory);

    registry.RegisterSymbol(
        module,
        "memmove",
        CopyMemory);

    registry.RegisterSymbol(
        std::move(module),
        "memset",
        SetMemory);
}

} // namespace ps5emu::hle
