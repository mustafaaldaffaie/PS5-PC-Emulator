#include <ps5emu/memory/GuestMemory.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>

using ps5emu::memory::GuestMemory;
using ps5emu::memory::Protection;

namespace {

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

} // namespace

int main() {
    GuestMemory memory;

    memory.Map(0x1000, 0x100, Protection::Read | Protection::Write);
    assert(memory.Mappings().size() == 1);

    const std::array<std::byte, 4> input{
        std::byte{0x11},
        std::byte{0x22},
        std::byte{0x33},
        std::byte{0x44},
    };

    memory.Write(0x1010, input);
    const auto output = memory.Read(0x1010, input.size());

    assert(output.size() == input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
    }

    assert(ThrowsRuntimeError([&] {
        memory.Map(0x1080, 0x100, Protection::Read);
    }));

    memory.Map(0x2000, 0x100, Protection::Read);
    assert(ThrowsRuntimeError([&] {
        memory.Write(0x2000, input);
    }));

    memory.Map(0x3000, 0x100, Protection::Write);
    assert(ThrowsRuntimeError([&] {
        static_cast<void>(memory.Read(0x3000, 1));
    }));

    assert(ThrowsRuntimeError([&] {
        static_cast<void>(memory.Read(0x5000, 1));
    }));

    return 0;
}
