#include <ps5emu/runtime/SceImportResolver.hpp>

#include <cassert>
#include <vector>

int main() {
    ps5emu::elf::SceModuleMetadata metadata;
    metadata.neededModules.push_back(
        ps5emu::elf::SceModuleRecord{
            .id = 2,
            .versionMajor = 1,
            .versionMinor = 0,
            .name = "libkernel",
        });
    metadata.importLibraries.push_back(
        ps5emu::elf::SceLibraryRecord{
            .id = 1,
            .version = 1,
            .name = "libSceKernel",
        });

    ps5emu::hle::HleRegistry registry;
    registry.Register(
        "libkernel",
        "ABCDEFGHIJK",
        "sceKernelSynthetic",
        [](ps5emu::hle::HleCallFrame& frame) {
            frame.returnValue = 0x55;
        });

    const std::vector<ps5emu::elf::ImportSymbol> imports{
        ps5emu::elf::ImportSymbol{
            .symbolIndex = 1,
            .name = "ABCDEFGHIJK#B#C",
        },
    };

    const auto identity =
        ps5emu::runtime::SceImportResolver::ResolveIdentity(
            imports.front(),
            metadata);

    assert(identity.has_value());
    assert(identity->module == "libkernel");
    assert(identity->nid == "ABCDEFGHIJK");

    const auto report =
        ps5emu::runtime::SceImportResolver::Resolve(
            imports,
            metadata,
            registry);

    assert(report.Complete());
    assert(report.bindings.size() == 1);
    assert(report.bindings[0].service != nullptr);

    ps5emu::hle::HleCallFrame frame;
    report.bindings[0].service->handler(frame);
    assert(frame.returnValue == 0x55);

    {
        auto unknownLibrary = imports.front();
        unknownLibrary.name = "ABCDEFGHIJK#C#C";
        assert(!ps5emu::runtime::SceImportResolver::ResolveIdentity(
            unknownLibrary,
            metadata).has_value());
    }

    {
        auto unknownModule = imports.front();
        unknownModule.name = "ABCDEFGHIJK#B#D";
        assert(!ps5emu::runtime::SceImportResolver::ResolveIdentity(
            unknownModule,
            metadata).has_value());
    }

    {
        auto plain = imports.front();
        plain.name = "sceKernelSynthetic";
        assert(!ps5emu::runtime::SceImportResolver::ResolveIdentity(
            plain,
            metadata).has_value());
    }

    return 0;
}
