#include <iostream>

#include <ps5emu/Core.hpp>

int main() {
    std::cout << "PS5-PC-Emulator " << ps5emu::Core::Version() << '\n';
    return 0;
}
