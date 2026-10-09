#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ps5emu::hle {

struct GuestFileResult {
    std::uint64_t errorCode = 0;
    std::int64_t value = 0;
};

class GuestFileSystemAccess {
public:
    virtual ~GuestFileSystemAccess() = default;

    [[nodiscard]] virtual GuestFileResult
    Open(std::string_view path,
         std::int32_t flags,
         std::uint16_t mode) = 0;

    [[nodiscard]] virtual GuestFileResult
    Close(std::int32_t descriptor) = 0;

    [[nodiscard]] virtual GuestFileResult
    Read(std::int32_t descriptor,
         std::span<std::byte> output) = 0;

    [[nodiscard]] virtual GuestFileResult
    Write(std::int32_t descriptor,
          std::span<const std::byte> input) = 0;

    [[nodiscard]] virtual GuestFileResult
    Seek(std::int32_t descriptor,
         std::int64_t offset,
         std::int32_t whence) = 0;
};

} // namespace ps5emu::hle
