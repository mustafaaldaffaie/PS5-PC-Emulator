#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

#include <ps5emu/Core.hpp>
#include <ps5emu/elf/DynamicMetadata.hpp>
#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/elf/ImportTable.hpp>
#include <ps5emu/loader/ExecutableImageLoader.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace {

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
            std::cout << "Usage: ps5emu inspect <elf-file>\n";
            return 0;
        }

        if (argc == 3 && std::string_view(argv[1]) == "inspect") {
            return InspectExecutable(argv[2]);
        }

        std::cerr << "Invalid command line.\n";
        std::cerr << "Usage: ps5emu inspect <elf-file>\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
