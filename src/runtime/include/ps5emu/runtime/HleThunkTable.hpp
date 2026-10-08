#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace ps5emu::runtime {

struct HleThunkTableOptions {
    std::uint64_t baseAddress = 0x00007fff00000000ull;
    std::size_t slotSize = 16;
    std::size_t capacity = 4096;
};

struct HleThunk {
    std::uint64_t address = 0;
    std::string module;
    std::string nid;
    std::string debugName;
};

class HleThunkTable final {
public:
    explicit HleThunkTable(
        const HleThunkTableOptions& options = {});

    [[nodiscard]] std::uint64_t
    Bind(const hle::HleService& service);

    [[nodiscard]] const HleThunk*
    FindByAddress(std::uint64_t address) const noexcept;

    [[nodiscard]] const HleThunk*
    FindByIdentity(std::string_view module,
                   std::string_view nid) const;

    [[nodiscard]] bool
    Dispatch(std::uint64_t address,
             const hle::HleRegistry& registry,
             hle::HleCallFrame& frame) const;

    void InstallOrUpdate(memory::GuestMemory& memory);

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept;
    [[nodiscard]] std::uint64_t BaseAddress() const noexcept;
    [[nodiscard]] std::size_t SlotSize() const noexcept;

private:
    [[nodiscard]] static std::string
    MakeKey(std::string_view module, std::string_view nid);

    [[nodiscard]] std::uint64_t
    AddressForIndex(std::size_t index) const;

    HleThunkTableOptions options_;
    std::size_t arenaSize_ = 0;
    bool arenaInstalled_ = false;
    std::size_t initializedCount_ = 0;
    std::vector<HleThunk> thunks_;
    std::unordered_map<std::string, std::size_t> byIdentity_;
};

} // namespace ps5emu::runtime
