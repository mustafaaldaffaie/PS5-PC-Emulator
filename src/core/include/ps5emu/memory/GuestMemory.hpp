#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ps5emu::memory {

enum class Protection : std::uint8_t {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4
};

constexpr Protection operator|(Protection lhs, Protection rhs) noexcept {
    return static_cast<Protection>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

struct Mapping {
    std::uint64_t guestAddress = 0;
    std::size_t size = 0;
    Protection protection = Protection::None;
    std::vector<std::byte> data;
};

class GuestMemory final {
public:
    void Map(std::uint64_t guestAddress,
             std::size_t size,
             Protection protection);

    void Write(std::uint64_t guestAddress,
               std::span<const std::byte> bytes);

    [[nodiscard]] std::span<const std::byte>
    Read(std::uint64_t guestAddress, std::size_t size) const;

    [[nodiscard]] const std::vector<Mapping>& Mappings() const noexcept;

private:
    [[nodiscard]] Mapping& FindMapping(std::uint64_t guestAddress,
                                       std::size_t size);

    [[nodiscard]] const Mapping& FindMapping(std::uint64_t guestAddress,
                                             std::size_t size) const;

    std::vector<Mapping> mappings_;
};

} // namespace ps5emu::memory
