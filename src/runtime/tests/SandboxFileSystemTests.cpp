#include <ps5emu/runtime/SandboxFileSystem.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

int main() {
    using ps5emu::runtime::SandboxFileSystem;

    const auto root =
        std::filesystem::temp_directory_path() /
        "ps5emu-sandbox-filesystem-test";

    std::error_code error;
    std::filesystem::remove_all(
        root,
        error);
    error.clear();

    std::filesystem::create_directories(
        root / "app0",
        error);
    assert(!error);

    {
        SandboxFileSystem files(root);

        constexpr std::int32_t readWrite =
            0x00000002;
        constexpr std::int32_t create =
            0x00000200;
        constexpr std::int32_t truncate =
            0x00000400;

        const auto opened =
            files.Open(
                "/app0/test.bin",
                readWrite |
                    create |
                    truncate,
                0644);

        assert(opened.errorCode == 0);
        assert(opened.value >= 3);

        const auto descriptor =
            static_cast<std::int32_t>(
                opened.value);

        const std::array<std::byte, 3> payload{
            std::byte{'p'},
            std::byte{'s'},
            std::byte{'5'},
        };

        const auto written =
            files.Write(
                descriptor,
                payload);

        assert(written.errorCode == 0);
        assert(written.value == 3);

        const auto seek =
            files.Seek(
                descriptor,
                0,
                0);

        assert(seek.errorCode == 0);
        assert(seek.value == 0);

        std::array<std::byte, 3> output{};
        const auto read =
            files.Read(
                descriptor,
                output);

        assert(read.errorCode == 0);
        assert(read.value == 3);
        assert(output == payload);

        const auto closed =
            files.Close(descriptor);

        assert(closed.errorCode == 0);

        assert(
            std::filesystem::exists(
                root /
                "app0" /
                "test.bin"));

        const auto secondClose =
            files.Close(descriptor);
        assert(
            secondClose.errorCode ==
            0x80020009ull);

        const auto traversal =
            files.Open(
                "/app0/../outside.bin",
                readWrite |
                    create,
                0644);

        assert(
            traversal.errorCode ==
            0x8002000dull);

        const auto windowsEscape =
            files.Open(
                "C:/outside.bin",
                readWrite |
                    create,
                0644);

        assert(
            windowsEscape.errorCode ==
            0x8002000dull);

        const auto backslashEscape =
            files.Open(
                "app0\\outside.bin",
                readWrite |
                    create,
                0644);

        assert(
            backslashEscape.errorCode ==
            0x8002000dull);

        const auto missing =
            files.Open(
                "/app0/missing.bin",
                0,
                0);

        assert(missing.errorCode != 0);

        const auto unsupported =
            files.Open(
                "/app0/test.bin",
                0x00020000,
                0);

        assert(
            unsupported.errorCode ==
            0x80020016ull);
    }

    std::filesystem::remove_all(
        root,
        error);

    return 0;
}
