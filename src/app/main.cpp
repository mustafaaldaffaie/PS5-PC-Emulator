#include <iostream>
#include <string_view>

#include <ps5emu/Core.hpp>
#include <ps5emu/loader/ExecutableImageLoader.hpp>
#include <ps5emu/memory/GuestMemory.hpp>

namespace {

int InspectExecutable(const char* path) {
    ps5emu::memory::GuestMemory memory;
    const auto image =
        ps5emu::loader::ExecutableImageLoader::LoadElfFile(path, memory);

    std::cout << "Entry point: 0x"
              << std::hex << image.entryPoint << std::dec << '\n';
    std::cout << "Mapped segments: "
              << image.mappedSegmentCount << '\n';

    for (const auto& mapping : memory.Mappings()) {
        std::cout << "  guest=0x"
                  << std::hex << mapping.guestAddress
                  << " size=0x" << mapping.size
                  << " protection=0x"
                  << static_cast<unsigned>(mapping.protection)
                  << std::dec << '\n';
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
