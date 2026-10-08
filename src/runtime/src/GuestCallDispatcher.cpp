#include <ps5emu/runtime/GuestCallDispatcher.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ps5emu::runtime {
namespace {

class GuestMemoryAdapter final : public hle::GuestMemoryAccess {
public:
    explicit GuestMemoryAdapter(memory::GuestMemory& memory)
        : memory_(memory) {
    }

    void Read(std::uint64_t guestAddress,
              std::span<std::byte> output) const override {
        if (output.empty()) {
            return;
        }

        const auto source =
            memory_.Read(guestAddress, output.size());

        std::copy(source.begin(), source.end(), output.begin());
    }

    void Write(std::uint64_t guestAddress,
               std::span<const std::byte> input) override {
        memory_.Write(guestAddress, input);
    }

private:
    memory::GuestMemory& memory_;
};

std::uint64_t CheckedAdd(std::uint64_t value,
                         std::uint64_t amount,
                         const char* message) {
    if (value > std::numeric_limits<std::uint64_t>::max() - amount) {
        throw std::runtime_error(message);
    }

    return value + amount;
}

std::uint64_t ReadU64(const memory::GuestMemory& memory,
                      std::uint64_t address) {
    const auto bytes = memory.Read(address, sizeof(std::uint64_t));

    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

hle::HleCallFrame BuildCallFrame(
    const SysvGuestContext& context,
    const memory::GuestMemory& memory,
    hle::GuestMemoryAccess& memoryAccess) {
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

    frame.arguments[6] = ReadU64(memory, argument7Address);
    frame.arguments[7] = ReadU64(memory, argument8Address);
    frame.memory = &memoryAccess;

    return frame;
}

} // namespace

GuestCallDispatchResult GuestCallDispatcher::Dispatch(
    std::uint64_t targetAddress,
    SysvGuestContext& context,
    memory::GuestMemory& memory,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks) {
    if (thunks.FindByAddress(targetAddress) == nullptr) {
        return {};
    }

    GuestMemoryAdapter memoryAccess(memory);
    auto frame =
        BuildCallFrame(
            context,
            memory,
            memoryAccess);

    if (!thunks.Dispatch(
            targetAddress,
            registry,
            frame)) {
        return {};
    }

    context.rax = frame.returnValue;

    return GuestCallDispatchResult{
        .handled = true,
        .errorCode = frame.errorCode,
    };
}

} // namespace ps5emu::runtime
