#include <ps5emu/hle/Nid.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

namespace ps5emu::hle {
namespace {

constexpr std::array<std::uint8_t, 16> kNidSalt{
    0x51, 0x8d, 0x64, 0xa6,
    0x35, 0xde, 0xd8, 0xc1,
    0xe6, 0xb0, 0x39, 0xb1,
    0xc3, 0xe5, 0x52, 0x30,
};

constexpr std::string_view kSonyBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

class Sha1 final {
public:
    void Update(std::span<const std::byte> data) {
        if (data.size() >
            std::numeric_limits<std::uint64_t>::max() - totalBytes_) {
            throw std::length_error("SHA-1 input is too large");
        }

        totalBytes_ += static_cast<std::uint64_t>(data.size());

        std::size_t inputOffset = 0;
        while (inputOffset < data.size()) {
            const auto available = block_.size() - buffered_;
            const auto remaining = data.size() - inputOffset;
            const auto amount = std::min(available, remaining);

            std::memcpy(
                block_.data() + buffered_,
                data.data() + inputOffset,
                amount);

            buffered_ += amount;
            inputOffset += amount;

            if (buffered_ == block_.size()) {
                ProcessBlock(block_);
                buffered_ = 0;
            }
        }
    }

    [[nodiscard]] std::array<std::uint8_t, 20> Finalize() {
        if (totalBytes_ >
            std::numeric_limits<std::uint64_t>::max() / 8u) {
            throw std::length_error("SHA-1 bit length overflows");
        }

        const auto bitLength = totalBytes_ * 8u;

        block_[buffered_++] = std::byte{0x80};

        if (buffered_ > 56) {
            std::fill(
                block_.begin() + static_cast<std::ptrdiff_t>(buffered_),
                block_.end(),
                std::byte{0});
            ProcessBlock(block_);
            buffered_ = 0;
        }

        std::fill(
            block_.begin() + static_cast<std::ptrdiff_t>(buffered_),
            block_.begin() + 56,
            std::byte{0});

        for (std::size_t index = 0; index < 8; ++index) {
            const auto shift =
                static_cast<unsigned>((7 - index) * 8);
            block_[56 + index] = static_cast<std::byte>(
                (bitLength >> shift) & 0xffu);
        }

        ProcessBlock(block_);
        buffered_ = 0;

        std::array<std::uint8_t, 20> digest{};
        for (std::size_t word = 0; word < state_.size(); ++word) {
            digest[word * 4 + 0] =
                static_cast<std::uint8_t>(state_[word] >> 24);
            digest[word * 4 + 1] =
                static_cast<std::uint8_t>(state_[word] >> 16);
            digest[word * 4 + 2] =
                static_cast<std::uint8_t>(state_[word] >> 8);
            digest[word * 4 + 3] =
                static_cast<std::uint8_t>(state_[word]);
        }

        return digest;
    }

private:
    void ProcessBlock(const std::array<std::byte, 64>& block) {
        std::array<std::uint32_t, 80> words{};

        for (std::size_t index = 0; index < 16; ++index) {
            const auto offset = index * 4;
            words[index] =
                (Byte(block[offset + 0]) << 24) |
                (Byte(block[offset + 1]) << 16) |
                (Byte(block[offset + 2]) << 8) |
                Byte(block[offset + 3]);
        }

        for (std::size_t index = 16; index < words.size(); ++index) {
            words[index] = std::rotl(
                words[index - 3] ^
                words[index - 8] ^
                words[index - 14] ^
                words[index - 16],
                1);
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];

        for (std::size_t index = 0; index < words.size(); ++index) {
            std::uint32_t function = 0;
            std::uint32_t constant = 0;

            if (index < 20) {
                function = (b & c) | ((~b) & d);
                constant = 0x5a827999u;
            } else if (index < 40) {
                function = b ^ c ^ d;
                constant = 0x6ed9eba1u;
            } else if (index < 60) {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8f1bbcdcu;
            } else {
                function = b ^ c ^ d;
                constant = 0xca62c1d6u;
            }

            const auto next =
                std::rotl(a, 5) +
                function +
                e +
                constant +
                words[index];

            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = next;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
    }

    [[nodiscard]] static std::uint32_t
    Byte(std::byte value) noexcept {
        return std::to_integer<std::uint32_t>(value);
    }

    std::array<std::uint32_t, 5> state_{
        0x67452301u,
        0xefcdab89u,
        0x98badcfeu,
        0x10325476u,
        0xc3d2e1f0u,
    };
    std::array<std::byte, 64> block_{};
    std::size_t buffered_ = 0;
    std::uint64_t totalBytes_ = 0;
};

std::string EncodeNid(const std::array<std::uint8_t, 20>& digest) {
    std::array<std::uint8_t, 8> reversed{};
    for (std::size_t index = 0; index < reversed.size(); ++index) {
        reversed[index] = digest[reversed.size() - 1 - index];
    }

    std::string output;
    output.reserve(11);

    std::size_t index = 0;
    while (index + 3 <= reversed.size()) {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(reversed[index]) << 16) |
            (static_cast<std::uint32_t>(reversed[index + 1]) << 8) |
            static_cast<std::uint32_t>(reversed[index + 2]);

        output.push_back(kSonyBase64Alphabet[(value >> 18) & 0x3fu]);
        output.push_back(kSonyBase64Alphabet[(value >> 12) & 0x3fu]);
        output.push_back(kSonyBase64Alphabet[(value >> 6) & 0x3fu]);
        output.push_back(kSonyBase64Alphabet[value & 0x3fu]);
        index += 3;
    }

    const auto remaining = reversed.size() - index;
    if (remaining != 0) {
        std::uint32_t value =
            static_cast<std::uint32_t>(reversed[index]) << 16;
        if (remaining == 2) {
            value |=
                static_cast<std::uint32_t>(reversed[index + 1]) << 8;
        }

        output.push_back(kSonyBase64Alphabet[(value >> 18) & 0x3fu]);
        output.push_back(kSonyBase64Alphabet[(value >> 12) & 0x3fu]);

        if (remaining == 2) {
            output.push_back(
                kSonyBase64Alphabet[(value >> 6) & 0x3fu]);
        }
    }

    if (output.size() != 11) {
        throw std::runtime_error(
            "Internal NID encoding produced an invalid length");
    }

    return output;
}

} // namespace

std::string Nid::Compute(std::string_view symbolName) {
    if (symbolName.empty()) {
        throw std::invalid_argument("NID symbol name cannot be empty");
    }

    Sha1 sha1;

    const auto nameBytes = std::as_bytes(
        std::span(symbolName.data(), symbolName.size()));
    sha1.Update(nameBytes);

    const auto saltBytes = std::as_bytes(
        std::span(kNidSalt.data(), kNidSalt.size()));
    sha1.Update(saltBytes);

    return EncodeNid(sha1.Finalize());
}

bool Nid::IsValid(std::string_view nid) noexcept {
    if (nid.size() != 11) {
        return false;
    }

    return std::all_of(
        nid.begin(),
        nid.end(),
        [](char character) {
            return kSonyBase64Alphabet.find(character) !=
                   std::string_view::npos;
        });
}

} // namespace ps5emu::hle
