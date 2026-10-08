#include <ps5emu/runtime/HleThunkTable.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace ps5emu::runtime {
namespace {

bool ContainsNull(std::string_view value) {
    return value.find('\0') != std::string_view::npos;
}

} // namespace

HleThunkTable::HleThunkTable(
    const HleThunkTableOptions& options)
    : options_(options) {
    if (options_.slotSize == 0) {
        throw std::invalid_argument(
            "HLE thunk slot size cannot be zero");
    }

    if (options_.capacity == 0) {
        throw std::invalid_argument(
            "HLE thunk capacity cannot be zero");
    }

    if (options_.capacity >
        std::numeric_limits<std::size_t>::max() /
            options_.slotSize) {
        throw std::invalid_argument(
            "HLE thunk arena size overflows the host");
    }

    arenaSize_ = options_.capacity * options_.slotSize;

    if (static_cast<std::uint64_t>(arenaSize_) >
        std::numeric_limits<std::uint64_t>::max() -
            options_.baseAddress) {
        throw std::invalid_argument(
            "HLE thunk arena address range overflows");
    }
}

std::uint64_t HleThunkTable::Bind(
    const hle::HleService& service) {
    if (service.module.empty() ||
        service.nid.empty() ||
        ContainsNull(service.module) ||
        ContainsNull(service.nid)) {
        throw std::invalid_argument(
            "HLE thunk service identity is invalid");
    }

    const auto key = MakeKey(service.module, service.nid);
    const auto existing = byIdentity_.find(key);
    if (existing != byIdentity_.end()) {
        return thunks_[existing->second].address;
    }

    if (thunks_.size() == options_.capacity) {
        throw std::runtime_error(
            "HLE thunk table capacity is exhausted");
    }

    const auto index = thunks_.size();
    const auto address = AddressForIndex(index);

    HleThunk thunk;
    thunk.address = address;
    thunk.module = service.module;
    thunk.nid = service.nid;
    thunk.debugName = service.debugName;

    thunks_.push_back(std::move(thunk));
    byIdentity_.emplace(key, index);

    return address;
}

const HleThunk* HleThunkTable::FindByAddress(
    std::uint64_t address) const noexcept {
    if (address < options_.baseAddress) {
        return nullptr;
    }

    const auto delta = address - options_.baseAddress;
    if (delta % options_.slotSize != 0) {
        return nullptr;
    }

    const auto index =
        static_cast<std::size_t>(delta / options_.slotSize);
    if (index >= thunks_.size()) {
        return nullptr;
    }

    return &thunks_[index];
}

const HleThunk* HleThunkTable::FindByIdentity(
    std::string_view module,
    std::string_view nid) const {
    const auto found = byIdentity_.find(MakeKey(module, nid));
    if (found == byIdentity_.end()) {
        return nullptr;
    }

    return &thunks_[found->second];
}

bool HleThunkTable::Dispatch(
    std::uint64_t address,
    const hle::HleRegistry& registry,
    hle::HleCallFrame& frame) const {
    const auto* thunk = FindByAddress(address);
    if (thunk == nullptr) {
        return false;
    }

    return registry.Invoke(thunk->module, thunk->nid, frame);
}

void HleThunkTable::InstallOrUpdate(
    memory::GuestMemory& memory) {
    if (thunks_.empty()) {
        return;
    }

    if (!arenaInstalled_) {
        memory.Map(
            options_.baseAddress,
            arenaSize_,
            memory::Protection::Read |
                memory::Protection::Execute);
        arenaInstalled_ = true;
    }

    std::vector<std::byte> trapSlot(
        options_.slotSize,
        std::byte{0xcc});

    for (std::size_t index = initializedCount_;
         index < thunks_.size();
         ++index) {
        memory.Initialize(
            AddressForIndex(index),
            trapSlot);
    }

    initializedCount_ = thunks_.size();
}

std::size_t HleThunkTable::Size() const noexcept {
    return thunks_.size();
}

std::size_t HleThunkTable::Capacity() const noexcept {
    return options_.capacity;
}

std::uint64_t HleThunkTable::BaseAddress() const noexcept {
    return options_.baseAddress;
}

std::size_t HleThunkTable::SlotSize() const noexcept {
    return options_.slotSize;
}

std::string HleThunkTable::MakeKey(
    std::string_view module,
    std::string_view nid) {
    std::string key;
    key.reserve(module.size() + nid.size() + 1);
    key.append(module);
    key.push_back('\0');
    key.append(nid);
    return key;
}

std::uint64_t HleThunkTable::AddressForIndex(
    std::size_t index) const {
    return options_.baseAddress +
        static_cast<std::uint64_t>(
            index * options_.slotSize);
}

} // namespace ps5emu::runtime
