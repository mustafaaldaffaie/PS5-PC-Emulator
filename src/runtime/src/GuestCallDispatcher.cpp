#include <ps5emu/runtime/GuestCallDispatcher.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <ps5emu/runtime/GuestMemoryAccessAdapter.hpp>

namespace ps5emu::runtime {
namespace {

std::uint64_t CheckedAdd(std::uint64_t value,
                         std::uint64_t amount,
                         const char* message) {
    if (value >
        std::numeric_limits<std::uint64_t>::max() -
            amount) {
        throw std::runtime_error(message);
    }

    return value + amount;
}

std::uint64_t ReadU64(
    const hle::GuestMemoryAccess& memory,
    std::uint64_t address) {
    std::array<std::byte, sizeof(std::uint64_t)> bytes{};
    memory.Read(
        address,
        bytes);

    std::uint64_t value = 0;
    std::memcpy(
        &value,
        bytes.data(),
        sizeof(value));
    return value;
}

hle::HleCallFrame BuildCallFrame(
    const SysvGuestContext& context,
    hle::GuestMemoryAccess& memory) {
    hle::HleCallFrame frame;

    frame.arguments[0] = context.rdi;
    frame.arguments[1] = context.rsi;
    frame.arguments[2] = context.rdx;
    frame.arguments[3] = context.rcx;
    frame.arguments[4] = context.r8;
    frame.arguments[5] = context.r9;

    const auto argument7Address =
        CheckedAdd(
            context.rsp,
            sizeof(std::uint64_t),
            "Guest stack argument address overflows");
    const auto argument8Address =
        CheckedAdd(
            argument7Address,
            sizeof(std::uint64_t),
            "Guest stack argument address overflows");

    frame.arguments[6] =
        ReadU64(
            memory,
            argument7Address);
    frame.arguments[7] =
        ReadU64(
            memory,
            argument8Address);
    frame.memory = &memory;

    return frame;
}

} // namespace

GuestCallDispatchResult GuestCallDispatcher::Dispatch(
    std::uint64_t targetAddress,
    SysvGuestContext& context,
    hle::GuestMemoryAccess& memory,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks) {
    if (thunks.FindByAddress(
            targetAddress) == nullptr) {
        return {};
    }

    auto frame =
        BuildCallFrame(
            context,
            memory);

    if (!thunks.Dispatch(
            targetAddress,
            registry,
            frame)) {
        return {};
    }

    context.rax =
        frame.returnValue;

    return GuestCallDispatchResult{
        .handled = true,
        .errorCode = frame.errorCode,
    };
}

GuestCallDispatchResult GuestCallDispatcher::Dispatch(
    std::uint64_t targetAddress,
    SysvGuestContext& context,
    memory::GuestMemory& memory,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks) {
    GuestMemoryAccessAdapter memoryAccess(
        memory);

    return Dispatch(
        targetAddress,
        context,
        memoryAccess,
        registry,
        thunks);
}

} // namespace ps5emu::runtime
