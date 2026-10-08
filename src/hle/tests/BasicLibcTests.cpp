#include <ps5emu/hle/BasicLibc.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

class TestMemory final : public ps5emu::hle::GuestMemoryAccess {
public:
    explicit TestMemory(std::size_t size)
        : bytes_(size, std::byte{0}) {
    }

    void Read(std::uint64_t guestAddress,
              std::span<std::byte> output) const override {
        if (guestAddress > bytes_.size()) {
            throw std::runtime_error("Test read is out of range");
        }

        const auto offset =
            static_cast<std::size_t>(guestAddress);

        if (output.size() > bytes_.size() - offset) {
            throw std::runtime_error("Test read is out of range");
        }

        std::copy_n(
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
            output.size(),
            output.begin());
    }

    void Write(std::uint64_t guestAddress,
               std::span<const std::byte> input) override {
        if (guestAddress > bytes_.size()) {
            throw std::runtime_error("Test write is out of range");
        }

        const auto offset =
            static_cast<std::size_t>(guestAddress);

        if (input.size() > bytes_.size() - offset) {
            throw std::runtime_error("Test write is out of range");
        }

        std::copy(
            input.begin(),
            input.end(),
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    [[nodiscard]] std::byte At(std::size_t index) const {
        return bytes_.at(index);
    }

private:
    std::vector<std::byte> bytes_;
};

template <typename Function>
bool ThrowsRuntimeError(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

const ps5emu::hle::HleService& FindSymbol(
    const ps5emu::hle::HleRegistry& registry,
    const char* module,
    const char* name) {
    const auto nid = ps5emu::hle::Nid::Compute(name);
    const auto* service = registry.Find(module, nid);
    assert(service != nullptr);
    return *service;
}

std::uint64_t Invoke(
    const ps5emu::hle::HleRegistry& registry,
    const char* name,
    TestMemory& memory,
    std::uint64_t argument0,
    std::uint64_t argument1 = 0,
    std::uint64_t argument2 = 0) {
    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = argument0;
    frame.arguments[1] = argument1;
    frame.arguments[2] = argument2;
    frame.memory = &memory;

    FindSymbol(registry, "libc", name).handler(frame);
    return frame.returnValue;
}

} // namespace

int main() {
    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::BasicLibc::Register(registry, "libc");

    assert(registry.Size() == 6);

    TestMemory memory(128);

    {
        const std::vector<std::byte> source{
            std::byte{1},
            std::byte{2},
            std::byte{3},
            std::byte{4},
        };
        memory.Write(16, source);

        assert(Invoke(
            registry,
            "memcpy",
            memory,
            32,
            16,
            source.size()) == 32);

        assert(memory.At(32) == std::byte{1});
        assert(memory.At(35) == std::byte{4});
    }

    {
        assert(Invoke(
            registry,
            "memset",
            memory,
            40,
            0xab,
            5) == 40);

        for (std::size_t index = 40; index < 45; ++index) {
            assert(memory.At(index) == std::byte{0xab});
        }
    }

    {
        const std::vector<std::byte> source{
            std::byte{9},
            std::byte{8},
            std::byte{7},
            std::byte{6},
        };
        memory.Write(60, source);

        static_cast<void>(
            Invoke(
                registry,
                "memmove",
                memory,
                62,
                60,
                source.size()));

        assert(memory.At(62) == std::byte{9});
        assert(memory.At(63) == std::byte{8});
        assert(memory.At(64) == std::byte{7});
        assert(memory.At(65) == std::byte{6});
    }

    {
        const std::vector<std::byte> left{
            std::byte{1},
            std::byte{2},
            std::byte{3},
        };
        const std::vector<std::byte> right{
            std::byte{1},
            std::byte{2},
            std::byte{4},
        };
        memory.Write(70, left);
        memory.Write(80, right);

        assert(Invoke(
            registry,
            "memcmp",
            memory,
            70,
            70,
            3) == 0);

        const auto less =
            Invoke(
                registry,
                "memcmp",
                memory,
                70,
                80,
                3);
        assert(
            static_cast<std::int64_t>(less) < 0);

        const auto greater =
            Invoke(
                registry,
                "memcmp",
                memory,
                80,
                70,
                3);
        assert(
            static_cast<std::int64_t>(greater) > 0);
    }

    {
        const std::vector<std::byte> text{
            std::byte{'h'},
            std::byte{'e'},
            std::byte{'l'},
            std::byte{'l'},
            std::byte{'o'},
            std::byte{0},
        };
        memory.Write(90, text);

        assert(Invoke(
            registry,
            "strlen",
            memory,
            90) == 5);

        assert(Invoke(
            registry,
            "strnlen",
            memory,
            90,
            3) == 3);

        assert(Invoke(
            registry,
            "strnlen",
            memory,
            90,
            8) == 5);

        assert(Invoke(
            registry,
            "strnlen",
            memory,
            std::numeric_limits<std::uint64_t>::max(),
            0) == 0);
    }

    {
        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 77;
        frame.arguments[1] = 88;
        frame.arguments[2] = 0;

        FindSymbol(registry, "libc", "memcpy").handler(frame);
        assert(frame.returnValue == 77);

        FindSymbol(registry, "libc", "memcmp").handler(frame);
        assert(frame.returnValue == 0);
    }

    {
        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 1;
        frame.arguments[1] = 2;
        frame.arguments[2] = 1;

        assert(ThrowsRuntimeError([&] {
            FindSymbol(registry, "libc", "memcpy").handler(frame);
        }));

        assert(ThrowsRuntimeError([&] {
            FindSymbol(registry, "libc", "memcmp").handler(frame);
        }));

        assert(ThrowsRuntimeError([&] {
            FindSymbol(registry, "libc", "strlen").handler(frame);
        }));
    }

    {
        const std::vector<std::byte> noTerminator{
            std::byte{'x'},
            std::byte{'y'},
        };
        memory.Write(126, noTerminator);

        assert(ThrowsRuntimeError([&] {
            static_cast<void>(
                Invoke(
                    registry,
                    "strlen",
                    memory,
                    126));
        }));
    }

    return 0;
}
