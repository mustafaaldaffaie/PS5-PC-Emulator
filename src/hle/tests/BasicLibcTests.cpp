#include <ps5emu/hle/BasicLibc.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
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
        const auto offset = static_cast<std::size_t>(guestAddress);
        if (offset > bytes_.size() ||
            output.size() > bytes_.size() - offset) {
            throw std::runtime_error("Test read is out of range");
        }

        std::copy_n(
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
            output.size(),
            output.begin());
    }

    void Write(std::uint64_t guestAddress,
               std::span<const std::byte> input) override {
        const auto offset = static_cast<std::size_t>(guestAddress);
        if (offset > bytes_.size() ||
            input.size() > bytes_.size() - offset) {
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

} // namespace

int main() {
    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::BasicLibc::Register(registry, "libc");

    assert(registry.Size() == 3);

    TestMemory memory(128);

    {
        const std::vector<std::byte> source{
            std::byte{1},
            std::byte{2},
            std::byte{3},
            std::byte{4},
        };
        memory.Write(16, source);

        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 32;
        frame.arguments[1] = 16;
        frame.arguments[2] = source.size();
        frame.memory = &memory;

        FindSymbol(registry, "libc", "memcpy").handler(frame);

        assert(frame.returnValue == 32);
        assert(memory.At(32) == std::byte{1});
        assert(memory.At(35) == std::byte{4});
    }

    {
        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 40;
        frame.arguments[1] = 0xab;
        frame.arguments[2] = 5;
        frame.memory = &memory;

        FindSymbol(registry, "libc", "memset").handler(frame);

        assert(frame.returnValue == 40);
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

        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 62;
        frame.arguments[1] = 60;
        frame.arguments[2] = source.size();
        frame.memory = &memory;

        FindSymbol(registry, "libc", "memmove").handler(frame);

        assert(memory.At(62) == std::byte{9});
        assert(memory.At(63) == std::byte{8});
        assert(memory.At(64) == std::byte{7});
        assert(memory.At(65) == std::byte{6});
    }

    {
        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 77;
        frame.arguments[1] = 88;
        frame.arguments[2] = 0;

        FindSymbol(registry, "libc", "memcpy").handler(frame);
        assert(frame.returnValue == 77);
    }

    {
        ps5emu::hle::HleCallFrame frame;
        frame.arguments[0] = 1;
        frame.arguments[1] = 2;
        frame.arguments[2] = 1;

        assert(ThrowsRuntimeError([&] {
            FindSymbol(registry, "libc", "memcpy").handler(frame);
        }));
    }

    return 0;
}
