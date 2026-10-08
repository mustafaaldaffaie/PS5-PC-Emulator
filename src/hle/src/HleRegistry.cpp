#include <ps5emu/hle/HleRegistry.hpp>

#include <ps5emu/hle/Nid.hpp>

#include <stdexcept>
#include <utility>

namespace ps5emu::hle {

void HleRegistry::Register(std::string module,
                           std::string nid,
                           std::string debugName,
                           HleHandler handler) {
    if (module.empty()) {
        throw std::invalid_argument("HLE module name cannot be empty");
    }

    if (nid.empty()) {
        throw std::invalid_argument("HLE NID cannot be empty");
    }

    if (module.find('\0') != std::string::npos ||
        nid.find('\0') != std::string::npos) {
        throw std::invalid_argument("HLE identifiers cannot contain null bytes");
    }

    if (!handler) {
        throw std::invalid_argument("HLE handler cannot be empty");
    }

    const auto key = MakeKey(module, nid);
    if (services_.contains(key)) {
        throw std::runtime_error(
            "HLE service is already registered");
    }

    HleService service;
    service.module = std::move(module);
    service.nid = std::move(nid);
    service.debugName = std::move(debugName);
    service.handler = std::move(handler);

    services_.emplace(key, std::move(service));
}

void HleRegistry::RegisterSymbol(
    std::string module,
    std::string symbolName,
    HleHandler handler) {
    if (symbolName.find('\0') != std::string::npos) {
        throw std::invalid_argument(
            "HLE symbol name cannot contain null bytes");
    }

    auto nid = Nid::Compute(symbolName);
    Register(
        std::move(module),
        std::move(nid),
        std::move(symbolName),
        std::move(handler));
}

const HleService* HleRegistry::Find(
    std::string_view module,
    std::string_view nid) const {
    if (module.find('\0') != std::string_view::npos ||
        nid.find('\0') != std::string_view::npos) {
        return nullptr;
    }
    const auto iterator = services_.find(MakeKey(module, nid));
    if (iterator == services_.end()) {
        return nullptr;
    }

    return &iterator->second;
}

bool HleRegistry::Invoke(
    std::string_view module,
    std::string_view nid,
    HleCallFrame& frame) const {
    const auto* service = Find(module, nid);
    if (service == nullptr) {
        return false;
    }

    service->handler(frame);
    return true;
}

std::size_t HleRegistry::Size() const noexcept {
    return services_.size();
}

void HleRegistry::Clear() noexcept {
    services_.clear();
}

std::string HleRegistry::MakeKey(
    std::string_view module,
    std::string_view nid) {
    std::string key;
    key.reserve(module.size() + nid.size() + 1);
    key.append(module);
    key.push_back('\0');
    key.append(nid);
    return key;
}

} // namespace ps5emu::hle
