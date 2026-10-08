#include <ps5emu/runtime/NativeInstructionGuard.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

template <typename Function>
std::string RuntimeError(Function&& function) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        return error.what();
    }

    return {};
}

} // namespace

int main() {
    using ps5emu::memory::GuestMemory;
    using ps5emu::memory::Protection;
    using ps5emu::runtime::NativeInstructionGuard;

    {
        GuestMemory memory;
        memory.Map(
            0x400000,
            0x100,
            Protection::Read |
                Protection::Execute);

        const std::array<std::byte, 5> code{
            std::byte{0x90},
            std::byte{0x48},
            std::byte{0x31},
            std::byte{0xc0},
            std::byte{0xc3},
        };
        memory.Initialize(
            0x400000,
            code);

        NativeInstructionGuard::Validate(memory);
    }

    for (const auto& test :
         std::array{
             std::pair{
                 std::array{
                     std::byte{0x0f},
                     std::byte{0x05}},
                 "SYSCALL"},
             std::pair{
                 std::array{
                     std::byte{0x0f},
                     std::byte{0x34}},
                 "SYSENTER"},
             std::pair{
                 std::array{
                     std::byte{0xcd},
                     std::byte{0x80}},
                 "INT 0x80"}}) {
        GuestMemory memory;
        memory.Map(
            0x500000,
            0x100,
            Protection::Read |
                Protection::Execute);
        memory.Initialize(
            0x500020,
            test.first);

        const auto message =
            RuntimeError([&] {
                NativeInstructionGuard::Validate(
                    memory);
            });

        assert(!message.empty());
        assert(
            message.find(test.second) !=
            std::string::npos);
        assert(
            message.find("0x500020") !=
            std::string::npos);
    }

    {
        GuestMemory memory;
        memory.Map(
            0x600000,
            0x100,
            Protection::Read |
                Protection::Write);

        const std::array<std::byte, 2> data{
            std::byte{0x0f},
            std::byte{0x05},
        };
        memory.Initialize(
            0x600000,
            data);

        NativeInstructionGuard::Validate(memory);
    }

    return 0;
}
