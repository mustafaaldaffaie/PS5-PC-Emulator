#include <charconv>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

#include <ps5emu/Core.hpp>
#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/ImportTable.hpp>
#include <ps5emu/elf/SceModuleMetadata.hpp>
#include <ps5emu/loader/ExecutableImageLoader.hpp>
#include <ps5emu/hle/BuiltinServices.hpp>
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/NativeExecutionBuilder.hpp>
#include <ps5emu/runtime/NativeGuestThreadRuntime.hpp>
#include <ps5emu/runtime/NativeHleExecutor.hpp>
#include <ps5emu/runtime/SceExecutablePreparer.hpp>

namespace {

std::vector<std::byte> ReadFile(const char* path);

void PrintUsage(std::ostream& stream) {
    stream << "Usage: ps5emu inspect <elf-file>\n"
           << "       ps5emu prepare <elf-file> [load-bias]\n"
           << "       ps5emu run-native <elf-file> [load-bias]\n";
}

std::uint64_t ParseLoadBias(std::string_view text) {
    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
        base = 16;
    }
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                        value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("Invalid load bias; use unsigned decimal or 0x hexadecimal");
    }
    return value;
}

int PrepareExecutable(const char* path, std::uint64_t loadBias) {
    const auto bytes = ReadFile(path);

    auto registry =
        ps5emu::hle::BuiltinServices::CreateRegistry();

    ps5emu::memory::GuestMemory memory;
    ps5emu::runtime::HleThunkTable thunks;

    const auto prepared =
        ps5emu::runtime::SceExecutablePreparer::Prepare(
            bytes,
            memory,
            registry,
            thunks,
            ps5emu::runtime::ScePrepareOptions{
                .loadBias = loadBias,
            });

    std::cout << "Prepared entry point: 0x" << std::hex
              << prepared.linked.loaded.entryPoint
              << std::dec << '\n';
    std::cout << "Mapped segments: "
              << prepared.linked.loaded.mappedSegmentCount
              << '\n';
    std::cout << "Applied relocations: "
              << prepared.linked.appliedRelocationCount
              << '\n';
    std::cout << "Resolved HLE imports: "
              << prepared.resolvedHleImportCount
              << '\n';
    std::cout << "Unresolved imports: "
              << prepared.unresolvedImportCount
              << '\n';
    std::cout << "HLE thunks: "
              << thunks.Size()
              << '\n';
    std::cout << "Unresolved weak symbols: "
              << prepared.linked.unresolvedWeakSymbolCount
              << '\n';
    std::cout
        << "Preparation only; use run-native for experimental execution.\n";
    return 0;
}

ps5emu::runtime::GuestExecutionOptions
NativeExecutionOptions(std::uint64_t loadBias) {
    ps5emu::runtime::GuestExecutionOptions options;
    options.loadBias = loadBias;

    options.threadMemory.stackAddress =
        0x0000200100000000ull;
    options.threadMemory.stackSize =
        8u * 1024u * 1024u;
    options.threadMemory.tlsAddress =
        0x0000200200000000ull;

    options.thunks.baseAddress =
        0x0000200300000000ull;

    return options;
}

int RunNativeExecutable(
    const char* path,
    std::uint64_t loadBias) {
    const auto bytes = ReadFile(path);

    auto prepared =
        ps5emu::runtime::NativeExecutionBuilder::Prepare(
            bytes,
            NativeExecutionOptions(loadBias));

    if (prepared.guest.image.unresolvedImportCount != 0) {
        throw std::runtime_error(
            "Native execution requires all non-weak imports to resolve");
    }

    ps5emu::runtime::NativeGuestThreadRuntime threadRuntime(
        bytes,
        ps5emu::elf::Elf64::Parse(bytes),
        prepared.nativeImage,
        prepared.guest.registry,
        prepared.guest.thunks,
        prepared.syscallTraps);

    ps5emu::runtime::NativeHleExecutor executor;
    const auto result =
        executor.Run(
            prepared.guest.context,
            prepared.nativeImage,
            prepared.guest.registry,
            prepared.guest.thunks,
            prepared.syscallTraps,
            &threadRuntime);

    if (result.interceptedSyscall) {
        std::cout << "Intercepted unsupported guest syscall "
                  << result.syscallNumber
                  << " at 0x"
                  << std::hex
                  << result.syscallAddress
                  << std::dec
                  << "\n";
        return 3;
    }

    std::cout << "Native guest returned.\n";
    std::cout << "Handled HLE traps: "
              << result.handledTrapCount
              << '\n';
    std::cout << "Guest RAX: 0x"
              << std::hex
              << prepared.guest.context.rax
              << std::dec
              << '\n';
    std::cout << "Guest RSP: 0x"
              << std::hex
              << prepared.guest.context.rsp
              << std::dec
              << '\n';

    return 0;
}

std::vector<std::byte> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open executable file");
    }

    const auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to determine executable file size");
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);

    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!file) {
            throw std::runtime_error("Failed to read executable file");
        }
    }

    return bytes;
}

int InspectExecutable(const char* path) {
    const auto bytes = ReadFile(path);
    const auto elfImage = ps5emu::elf::Elf64::Parse(bytes);
    const auto dynamic =
        ps5emu::elf::DynamicMetadataParser::Parse(bytes, elfImage);
    const auto sceMetadata =
        ps5emu::elf::SceModuleMetadataParser::Parse(
            bytes,
            elfImage,
            dynamic);
    const auto imports =
        ps5emu::elf::ImportTable::Parse(bytes, elfImage, dynamic);

    ps5emu::memory::GuestMemory memory;
    const auto loaded =
        ps5emu::loader::ExecutableImageLoader::LoadElf(bytes, memory);

    std::cout << "Entry point: 0x"
              << std::hex << loaded.entryPoint << std::dec << '\n';
    std::cout << "Mapped segments: "
              << loaded.mappedSegmentCount << '\n';

    for (const auto& mapping : memory.Mappings()) {
        std::cout << "  guest=0x"
                  << std::hex << mapping.guestAddress
                  << " size=0x" << mapping.size
                  << " protection=0x"
                  << static_cast<unsigned>(mapping.protection)
                  << std::dec << '\n';
    }

    std::cout << "Needed libraries: "
              << dynamic.neededLibraries.size() << '\n';
    for (const auto& library : dynamic.neededLibraries) {
        std::cout << "  " << library << '\n';
    }

    if (sceMetadata.module.has_value()) {
        std::cout << "SCE module: "
                  << sceMetadata.module->name
                  << " id=" << sceMetadata.module->id
                  << " version="
                  << static_cast<unsigned>(
                         sceMetadata.module->versionMajor)
                  << '.'
                  << static_cast<unsigned>(
                         sceMetadata.module->versionMinor)
                  << '\n';
    }

    std::cout << "SCE needed modules: "
              << sceMetadata.neededModules.size() << '\n';
    for (const auto& module : sceMetadata.neededModules) {
        std::cout << "  id=" << module.id
                  << " name=" << module.name
                  << " version="
                  << static_cast<unsigned>(module.versionMajor)
                  << '.'
                  << static_cast<unsigned>(module.versionMinor)
                  << '\n';
    }

    std::cout << "SCE import libraries: "
              << sceMetadata.importLibraries.size() << '\n';
    for (const auto& library : sceMetadata.importLibraries) {
        std::cout << "  id=" << library.id
                  << " name=" << library.name
                  << " version=0x"
                  << std::hex << library.version << std::dec
                  << '\n';
    }

    std::cout << "Imports: " << imports.size() << '\n';
    for (const auto& import : imports) {
        std::cout << "  [" << import.symbolIndex << "] "
                  << import.name
                  << " relocations="
                  << import.relocations.size() << '\n';
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            std::cout << "PS5-PC-Emulator "
                      << ps5emu::Core::Version() << '\n';
            PrintUsage(std::cout);
            return 0;
        }

        if (argc == 3 && std::string_view(argv[1]) == "inspect") {
            return InspectExecutable(argv[2]);
        }

        if ((argc == 3 || argc == 4) &&
            std::string_view(argv[1]) == "prepare") {
            return PrepareExecutable(
                argv[2],
                argc == 4 ? ParseLoadBias(argv[3]) : 0);
        }

        if ((argc == 3 || argc == 4) &&
            std::string_view(argv[1]) == "run-native") {
            return RunNativeExecutable(
                argv[2],
                argc == 4 ? ParseLoadBias(argv[3]) : 0);
        }

        std::cerr << "Invalid command line.\n";
        PrintUsage(std::cerr);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
