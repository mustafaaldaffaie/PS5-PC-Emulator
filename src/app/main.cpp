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
#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/ExecutableLinker.hpp>

namespace {

std::vector<std::byte> ReadFile(const char* path);

void PrintUsage(std::ostream& stream) {
    stream << "Usage: ps5emu inspect <elf-file>\n"
           << "       ps5emu prepare <elf-file> [load-bias]\n";
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
    ps5emu::memory::GuestMemory memory;
    ps5emu::runtime::LinkOptions options;
    options.loadBias = loadBias;
    const auto linked = ps5emu::runtime::ExecutableLinker::Load(
        bytes, memory, options);
    std::cout << "Prepared entry point: 0x" << std::hex
              << linked.loaded.entryPoint << std::dec << '\n';
    std::cout << "Mapped segments: " << linked.loaded.mappedSegmentCount << '\n';
    std::cout << "Applied relocations: " << linked.appliedRelocationCount << '\n';
    std::cout << "Unresolved weak symbols: "
              << linked.unresolvedWeakSymbolCount << '\n';
    std::cout << "Guest execution is not implemented.\n";
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
            return PrepareExecutable(argv[2], argc == 4 ? ParseLoadBias(argv[3]) : 0);
        }

        std::cerr << "Invalid command line.\n";
        PrintUsage(std::cerr);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
