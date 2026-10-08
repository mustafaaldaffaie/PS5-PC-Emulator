#include <ps5emu/runtime/HleTrapHandler.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <ps5emu/runtime/GuestMemoryAccessAdapter.hpp>

namespace ps5emu::runtime {
namespace {

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

std::uint64_t CheckedStackAfterReturn(
    std::uint64_t rsp) {
    if (rsp >
        std::numeric_limits<std::uint64_t>::max() -
            sizeof(std::uint64_t)) {
        throw std::runtime_error(
            "Guest stack pointer overflows while returning from HLE");
    }

    return rsp +
        sizeof(std::uint64_t);
}

} // namespace

HleTrapResult HleTrapHandler::HandleInt3(
    SysvGuestContext& context,
    hle::GuestMemoryAccess& memory,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks) {
    if (context.rip == 0) {
        return {};
    }

    const auto thunkAddress =
        context.rip - 1;

    if (thunks.FindByAddress(
            thunkAddress) == nullptr) {
        return {};
    }

    const auto returnAddress =
        ReadU64(
            memory,
            context.rsp);
    const auto returnedStackPointer =
        CheckedStackAfterReturn(
            context.rsp);

    const auto callResult =
        GuestCallDispatcher::Dispatch(
            thunkAddress,
            context,
            memory,
            registry,
            thunks);

    if (!callResult.handled) {
        return {};
    }

    context.rsp =
        returnedStackPointer;
    context.rip =
        returnAddress;

    return HleTrapResult{
        .handled = true,
        .errorCode = callResult.errorCode,
    };
}

HleTrapResult HleTrapHandler::HandleInt3(
    SysvGuestContext& context,
    memory::GuestMemory& memory,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks) {
    GuestMemoryAccessAdapter memoryAccess(
        memory);

    return HandleInt3(
        context,
        memoryAccess,
        registry,
        thunks);
}

} // namespace ps5emu::runtime
