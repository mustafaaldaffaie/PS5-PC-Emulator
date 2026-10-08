#include <ps5emu/hle/NidNameDatabase.hpp>

#include <ps5emu/hle/Nid.hpp>

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ps5emu::hle {
namespace {

constexpr std::array<std::string_view, 17> kBuiltInNames{
    "memcpy",
    "memmove",
    "sceKernelCreateSema",
    "sceKernelOpen",
    "sceKernelSignalSema",
    "sceKernelWrite",
    "sceKernelGetProcParam",
    "scePthreadMutexLock",
    "sceKernelRead",
    "sceKernelCreateEqueue",
    "sceKernelMmap",
    "sceNetSocket",
    "sceVideoOutOpen",
    "scePthreadCondWait",
    "sceKernelWaitSema",
    "sceAudioOutOpen",
    "scePadOpen",
};

bool ContainsNull(std::string_view value) {
    return value.find('\0') != std::string_view::npos;
}

} // namespace

NidNameDatabase NidNameDatabase::CreateBuiltIn() {
    NidNameDatabase database;

    for (const auto name : kBuiltInNames) {
        const auto inserted = database.Add(std::string(name));
        if (!inserted) {
            throw std::runtime_error(
                "Duplicate symbol in the built-in NID database");
        }
    }

    return database;
}

bool NidNameDatabase::Add(std::string symbolName) {
    if (symbolName.empty()) {
        throw std::invalid_argument(
            "NID database symbol name cannot be empty");
    }

    if (ContainsNull(symbolName)) {
        throw std::invalid_argument(
            "NID database symbol name cannot contain null bytes");
    }

    if (nameToNid_.contains(symbolName)) {
        return false;
    }

    const auto nid = Nid::Compute(symbolName);

    const auto existingByNid = nidToName_.find(nid);
    if (existingByNid != nidToName_.end() &&
        existingByNid->second != symbolName) {
        throw std::runtime_error(
            "NID collision detected between symbol names");
    }

    const auto [nameIterator, nameInserted] =
        nameToNid_.emplace(symbolName, nid);
    if (!nameInserted) {
        return false;
    }

    try {
        const auto [nidIterator, nidInserted] =
            nidToName_.emplace(nid, std::move(symbolName));
        static_cast<void>(nidIterator);

        if (!nidInserted) {
            nameToNid_.erase(nameIterator);
            throw std::runtime_error(
                "NID database reverse mapping already exists");
        }
    } catch (...) {
        nameToNid_.erase(nameIterator);
        throw;
    }

    return true;
}

const std::string* NidNameDatabase::FindName(
    std::string_view nid) const {
    const auto found = nidToName_.find(std::string(nid));
    if (found == nidToName_.end()) {
        return nullptr;
    }

    return &found->second;
}

const std::string* NidNameDatabase::FindNid(
    std::string_view symbolName) const {
    const auto found = nameToNid_.find(std::string(symbolName));
    if (found == nameToNid_.end()) {
        return nullptr;
    }

    return &found->second;
}

std::size_t NidNameDatabase::Size() const noexcept {
    return nameToNid_.size();
}

void NidNameDatabase::Clear() noexcept {
    nameToNid_.clear();
    nidToName_.clear();
}

} // namespace ps5emu::hle
