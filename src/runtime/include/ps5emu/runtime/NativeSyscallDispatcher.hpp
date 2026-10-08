#pragma once

#include <cstdint>

#include <ps5emu/hle/GuestMemoryAccess.hpp>
#include <ps5emu/runtime/GuestCallDispatcher.hpp>

namespace ps5emu::runtime {

struct GuestProcessIdentity {
    std::uint64_t processId = 1000;
    std::uint64_t parentProcessId = 1;
    std::uint64_t userId = 1000;
    std::uint64_t effectiveUserId = 1000;
    std::uint64_t groupId = 1000;
    std::uint64_t effectiveGroupId = 1000;
};

struct NativeSyscallDispatchResult {
    bool handled = false;
    std::uint64_t syscallNumber = 0;
};

class NativeSyscallDispatcher final {
public:
    explicit NativeSyscallDispatcher(
        GuestProcessIdentity identity = {}) noexcept;

    [[nodiscard]] NativeSyscallDispatchResult
    Dispatch(SysvGuestContext& context,
             std::uint64_t syscallAddress,
             hle::GuestMemoryAccess* memory = nullptr) const;

private:
    GuestProcessIdentity identity_{};
};

} // namespace ps5emu::runtime
