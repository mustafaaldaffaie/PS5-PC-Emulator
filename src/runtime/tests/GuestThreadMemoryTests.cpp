#include <ps5emu/runtime/GuestThreadMemory.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

template <typename Function>
bool Throws(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::runtime::GuestThreadMemory;
    using ps5emu::runtime::GuestThreadMemoryOptions;

    std::vector<std::byte> bytes(0x200);
    bytes[0x100] = std::byte{0xaa};
    bytes[0x101] = std::byte{0xbb};
    bytes[0x102] = std::byte{0xcc};
    bytes[0x103] = std::byte{0xdd};

    ps5emu::elf::Image image;
    image.tlsSegment = ps5emu::elf::Segment{
        .memorySize = 16,
        .fileSize = 4,
        .fileOffset = 0x100,
        .alignment = 16,
    };

    GuestMemory memory;

    const auto layout =
        GuestThreadMemory::Create(
            bytes,
            image,
            memory,
            GuestThreadMemoryOptions{
                .stackAddress = 0x700000,
                .stackSize = 0x1000,
                .tlsAddress = 0x710000,
            });

    assert(layout.stackAddress == 0x700000);
    assert(layout.stackSize == 0x1000);
    assert(layout.initialStackPointer == 0x701000);
    assert(layout.tlsAddress == 0x710000);
    assert(layout.tlsSize == 16);
    assert(layout.tlsAlignment == 16);

    const auto tls = memory.Read(0x710000, 16);
    assert(tls[0] == std::byte{0xaa});
    assert(tls[1] == std::byte{0xbb});
    assert(tls[2] == std::byte{0xcc});
    assert(tls[3] == std::byte{0xdd});

    for (std::size_t index = 4; index < tls.size(); ++index) {
        assert(tls[index] == std::byte{0});
    }

    {
        GuestMemory unchanged;
        const auto before = unchanged.Mappings().size();

        assert(Throws([&] {
            static_cast<void>(
                GuestThreadMemory::Create(
                    bytes,
                    image,
                    unchanged,
                    GuestThreadMemoryOptions{
                        .stackAddress = 0x720000,
                        .stackSize = 0x1000,
                    }));
        }));

        assert(unchanged.Mappings().size() == before);
    }

    {
        GuestMemory unchanged;

        assert(Throws([&] {
            static_cast<void>(
                GuestThreadMemory::Create(
                    bytes,
                    image,
                    unchanged,
                    GuestThreadMemoryOptions{
                        .stackAddress = 0x730000,
                        .stackSize = 0x1000,
                        .tlsAddress = 0x730800,
                    }));
        }));

        assert(unchanged.Mappings().empty());
    }

    {
        ps5emu::elf::Image noTls;
        GuestMemory noTlsMemory;

        const auto noTlsLayout =
            GuestThreadMemory::Create(
                bytes,
                noTls,
                noTlsMemory,
                GuestThreadMemoryOptions{
                    .stackAddress = 0x740003,
                    .stackSize = 0x1010,
                });

        assert(noTlsLayout.initialStackPointer == 0x741010);
        assert(!noTlsLayout.tlsAddress.has_value());
        assert(noTlsLayout.tlsSize == 0);
    }

    return 0;
}
