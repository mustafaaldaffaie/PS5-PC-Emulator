#include <ps5emu/elf/SceQualifiedSymbol.hpp>

#include <limits>
#include <stdexcept>

namespace ps5emu::elf {
namespace {

constexpr std::string_view kSonyBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

std::uint16_t DecodeId(std::string_view encoded) {
    if (encoded.empty()) {
        throw std::runtime_error("SCE qualified import contains an empty ID");
    }

    std::uint32_t value = 0;
    for (const char character : encoded) {
        const auto digit = kSonyBase64Alphabet.find(character);
        if (digit == std::string_view::npos) {
            throw std::runtime_error(
                "SCE qualified import contains an invalid ID character");
        }

        constexpr auto kMaximum =
            std::numeric_limits<std::uint16_t>::max();
        if (value >
            (kMaximum - static_cast<std::uint32_t>(digit)) / 64u) {
            throw std::runtime_error(
                "SCE qualified import ID exceeds 16 bits");
        }

        value =
            value * 64u + static_cast<std::uint32_t>(digit);
    }

    return static_cast<std::uint16_t>(value);
}

void ValidateNid(std::string_view nid) {
    if (nid.size() != 11) {
        throw std::runtime_error(
            "SCE qualified import NID must contain 11 characters");
    }

    for (const char character : nid) {
        if (kSonyBase64Alphabet.find(character) ==
            std::string_view::npos) {
            throw std::runtime_error(
                "SCE qualified import NID contains an invalid character");
        }
    }
}

} // namespace

std::optional<SceQualifiedImport> SceQualifiedSymbol::Parse(
    std::string_view symbol) {
    const auto first = symbol.find('#');
    if (first == std::string_view::npos) {
        return std::nullopt;
    }

    const auto second = symbol.find('#', first + 1);
    if (second == std::string_view::npos ||
        symbol.find('#', second + 1) != std::string_view::npos) {
        throw std::runtime_error(
            "Malformed SCE qualified import symbol");
    }

    const auto nid = symbol.substr(0, first);
    const auto library = symbol.substr(
        first + 1,
        second - first - 1);
    const auto module = symbol.substr(second + 1);

    ValidateNid(nid);

    return SceQualifiedImport{
        .nid = std::string(nid),
        .libraryId = DecodeId(library),
        .moduleId = DecodeId(module),
    };
}

} // namespace ps5emu::elf
