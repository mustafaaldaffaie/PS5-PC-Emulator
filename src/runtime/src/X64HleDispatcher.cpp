#include <ps5emu/runtime/X64HleDispatcher.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ps5emu::runtime {
namespace {

std::uint64_t ReadU64(const memory::GuestMemory& memory,
                      std::uint64_t address) {
    const auto bytes = memory.Read(address, sizeof(std::uint64_t));

    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

std::uint64_t AddStackOffset(std::uint64_t stackPointer,
                             std::uint64_t offset) {
    if (stackPointer >
        std::numeric_limits<std::uint64_t>::max() - offset) {
        throw std::runtime_error(
            "Guest stack address overflows during HLE dispatch");
    }

    return stackPointer + offset;
}

} // namespace

HleDispatchResult X64HleDispatcher::DispatchIfThunk(
    X64GuestContext& context,
    const memory::GuestMemory& memory,
    const HleThunkTable& thunks,
    const hle::HleRegistry& registry) {
    const auto* thunk = thunks.FindByAddress(context.rip);
    if (thunk == nullptr) {
        return {};
    }

    const auto* service =
        registry.Find(thunk->module, thunk->nid);
    if (service == nullptr) {
        throw std::runtime_error(
            "HLE thunk references a service that is no longer registered");
    }

    const auto returnAddress =
        ReadU64(memory, context.rsp);
    const auto argument7 =
        ReadU64(memory, AddStackOffset(context.rsp, 8));
    const auto argument8 =
        ReadU64(memory, AddStackOffset(context.rsp, 16));

    hle::HleCallFrame frame;
    frame.arguments = std::array<std::uint64_t, 8>{
        context.rdi,
        context.rsi,
        context.rdx,
        context.rcx,
        context.r8,
        context.r9,
        argument7,
        argument8,
    };

    service->handler(frame);

    auto updated = context;
    updated.rax = frame.returnValue;
    updated.rip = returnAddress;
    updated.rsp = AddStackOffset(context.rsp, 8);

    context = updated;

    return HleDispatchResult{
        .handled = true,
        .errorCode = frame.errorCode,
    };
}

} // namespace ps5emu::runtime
