#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <ps5emu/hle/GuestMemoryAccess.hpp>

namespace ps5emu::hle {

struct HleCallFrame {
    std::array<std::uint64_t, 8> arguments{};
    std::uint64_t returnValue = 0;
    std::int64_t errorCode = 0;
    GuestMemoryAccess* memory = nullptr;
};

using HleHandler = std::function<void(HleCallFrame&)>;

struct HleService {
    std::string module;
    std::string nid;
    std::string debugName;
    HleHandler handler;
};

class HleRegistry final {
public:
    void Register(std::string module,
                  std::string nid,
                  std::string debugName,
                  HleHandler handler);

    void RegisterSymbol(std::string module,
                        std::string symbolName,
                        HleHandler handler);

    [[nodiscard]] const HleService*
    Find(std::string_view module,
         std::string_view nid) const;

    [[nodiscard]] bool
    Invoke(std::string_view module,
           std::string_view nid,
           HleCallFrame& frame) const;

    [[nodiscard]] std::size_t Size() const noexcept;

    void Clear() noexcept;

private:
    [[nodiscard]] static std::string
    MakeKey(std::string_view module,
            std::string_view nid);

    std::unordered_map<std::string, HleService> services_;
};

} // namespace ps5emu::hle
