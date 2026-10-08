#pragma once

#include <cstdint>

namespace ps5emu::hle {

struct GuestThreadCreateRequest {
    std::uint64_t attributeAddress = 0;
    std::uint64_t entryPoint = 0;
    std::uint64_t argument = 0;
    std::uint64_t nameAddress = 0;
};

struct GuestThreadCreateResult {
    std::uint64_t errorCode = 0;
    std::uint64_t handle = 0;
};

struct GuestThreadJoinResult {
    std::uint64_t errorCode = 0;
    std::uint64_t returnValue = 0;
};

class GuestThreadAccess {
public:
    virtual ~GuestThreadAccess() = default;

    [[nodiscard]] virtual GuestThreadCreateResult
    Create(const GuestThreadCreateRequest& request) = 0;

    [[nodiscard]] virtual GuestThreadJoinResult
    Join(std::uint64_t handle) = 0;

    [[nodiscard]] virtual std::uint64_t
    CurrentThreadHandle() const noexcept = 0;
};

} // namespace ps5emu::hle
