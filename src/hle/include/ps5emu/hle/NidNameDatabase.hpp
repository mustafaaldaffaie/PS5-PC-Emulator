#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ps5emu::hle {

class NidNameDatabase final {
public:
    [[nodiscard]] static NidNameDatabase CreateBuiltIn();

    [[nodiscard]] bool Add(std::string symbolName);

    [[nodiscard]] const std::string*
    FindName(std::string_view nid) const noexcept;

    [[nodiscard]] const std::string*
    FindNid(std::string_view symbolName) const noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;

    void Clear() noexcept;

private:
    std::unordered_map<std::string, std::string> nameToNid_;
    std::unordered_map<std::string, std::string> nidToName_;
};

} // namespace ps5emu::hle
