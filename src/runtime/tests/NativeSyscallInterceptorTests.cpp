#include <ps5emu/runtime/NativeSyscallInterceptor.hpp>

#include <array>
#include <cassert>
#include <cstddef>

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::NativeSyscallInterceptor;

    GuestMemory memory;
    memory.Map(
        0x400000,
        0x100,
        Protection::Read |
            Protection::Execute);
    memory.Map(
        0x500000,
        0x100,
        Protection::Read |
            Protection::Write);

    const std::array<std::byte, 9> code{
        std::byte{0x90},
        std::byte{0x0f},
        std::byte{0x05},
        std::byte{0x90},
        std::byte{0x0f},
        std::byte{0x05},
        std::byte{0xc3},
        std::byte{0x90},
        std::byte{0x90},
    };

    memory.Initialize(
        0x400010,
        code);

    const std::array<std::byte, 2> data{
        std::byte{0x0f},
        std::byte{0x05},
    };
    memory.Initialize(
        0x500020,
        data);

    const auto traps =
        NativeSyscallInterceptor::Rewrite(
            memory);

    assert(traps.size() == 2);
    assert(traps[0].guestAddress == 0x400011);
    assert(traps[1].guestAddress == 0x400014);

    const auto patched =
        memory.Read(
            0x400010,
            code.size());

    assert(patched[0] == std::byte{0x90});
    assert(patched[1] == std::byte{0xcc});
    assert(patched[2] == std::byte{0x90});
    assert(patched[3] == std::byte{0x90});
    assert(patched[4] == std::byte{0xcc});
    assert(patched[5] == std::byte{0x90});
    assert(patched[6] == std::byte{0xc3});

    const auto unchangedData =
        memory.Read(
            0x500020,
            2);
    assert(unchangedData[0] == std::byte{0x0f});
    assert(unchangedData[1] == std::byte{0x05});

    const auto secondPass =
        NativeSyscallInterceptor::Rewrite(
            memory);
    assert(secondPass.empty());

    return 0;
}
