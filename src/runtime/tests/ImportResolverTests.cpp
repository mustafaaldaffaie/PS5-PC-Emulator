#include <ps5emu/runtime/ImportResolver.hpp>

#include <cassert>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

template <typename Function>
bool ThrowsInvalidArgument(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

} // namespace

int main() {
    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "NID_A",
        "sceKernelA",
        [](ps5emu::hle::HleCallFrame& frame) {
            frame.returnValue = 7;
        });

    const std::vector<ps5emu::elf::ImportSymbol> imports{
        ps5emu::elf::ImportSymbol{
            .symbolIndex = 1,
            .name = "symbol_a",
        },
        ps5emu::elf::ImportSymbol{
            .symbolIndex = 2,
            .name = "symbol_b",
        },
        ps5emu::elf::ImportSymbol{
            .symbolIndex = 3,
            .name = "symbol_c",
        },
    };

    const auto report =
        ps5emu::runtime::ImportResolver::Resolve(
            imports,
            registry,
            [](const ps5emu::elf::ImportSymbol& import)
                -> std::optional<ps5emu::runtime::ImportIdentity> {
                if (import.name == "symbol_a") {
                    return ps5emu::runtime::ImportIdentity{
                        .module = "libkernel",
                        .nid = "NID_A",
                    };
                }

                if (import.name == "symbol_b") {
                    return ps5emu::runtime::ImportIdentity{
                        .module = "libkernel",
                        .nid = "NID_MISSING",
                    };
                }

                return std::nullopt;
            });

    assert(!report.Complete());
    assert(report.bindings.size() == 1);
    assert(report.unresolved.size() == 2);

    const auto& binding = report.bindings.front();
    assert(binding.symbolIndex == 1);
    assert(binding.symbolName == "symbol_a");
    assert(binding.identity.module == "libkernel");
    assert(binding.identity.nid == "NID_A");
    assert(binding.service != nullptr);

    ps5emu::hle::HleCallFrame frame;
    binding.service->handler(frame);
    assert(frame.returnValue == 7);

    assert(
        report.unresolved[0].failure ==
        ps5emu::runtime::ImportResolutionFailure::ServiceUnavailable);
    assert(report.unresolved[0].identity.has_value());

    assert(
        report.unresolved[1].failure ==
        ps5emu::runtime::ImportResolutionFailure::IdentityUnavailable);
    assert(!report.unresolved[1].identity.has_value());

    assert(ThrowsInvalidArgument([&] {
        static_cast<void>(
            ps5emu::runtime::ImportResolver::Resolve(
                imports,
                registry,
                {}));
    }));

    return 0;
}
