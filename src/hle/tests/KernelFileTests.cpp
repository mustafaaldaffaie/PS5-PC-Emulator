#include <ps5emu/hle/KernelFile.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace {

class TestMemory final : public ps5emu::hle::GuestMemoryAccess {
public:
    explicit TestMemory(std::size_t size)
        : bytes_(size, std::byte{0}) {}

    void Read(std::uint64_t address,
              std::span<std::byte> output) const override {
        assert(address <= bytes_.size());
        const auto offset = static_cast<std::size_t>(address);
        assert(output.size() <= bytes_.size() - offset);
        std::copy_n(
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
            output.size(),
            output.begin());
    }

    void Write(std::uint64_t address,
               std::span<const std::byte> input) override {
        assert(address <= bytes_.size());
        const auto offset = static_cast<std::size_t>(address);
        assert(input.size() <= bytes_.size() - offset);
        std::copy(
            input.begin(),
            input.end(),
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    void StoreString(std::uint64_t address,
                     const char* value) {
        const auto size = std::strlen(value) + 1;
        Write(
            address,
            std::span(
                reinterpret_cast<const std::byte*>(value),
                size));
    }

    [[nodiscard]] std::byte At(std::size_t index) const {
        return bytes_.at(index);
    }

private:
    std::vector<std::byte> bytes_;
};

class TestFiles final : public ps5emu::hle::GuestFileSystemAccess {
public:
    ps5emu::hle::GuestFileResult
    Open(std::string_view path,
         std::int32_t flags,
         std::uint16_t mode) override {
        lastPath = path;
        lastFlags = flags;
        lastMode = mode;
        return openResult;
    }

    ps5emu::hle::GuestFileResult
    Close(std::int32_t descriptor) override {
        lastDescriptor = descriptor;
        return closeResult;
    }

    ps5emu::hle::GuestFileResult
    Read(std::int32_t descriptor,
         std::span<std::byte> output) override {
        lastDescriptor = descriptor;
        const auto count =
            std::min(output.size(), readData.size());
        std::copy_n(readData.begin(), count, output.begin());
        return {
            .errorCode = readError,
            .value = readError == 0
                ? static_cast<std::int64_t>(count)
                : 0,
        };
    }

    ps5emu::hle::GuestFileResult
    Write(std::int32_t descriptor,
          std::span<const std::byte> input) override {
        lastDescriptor = descriptor;
        written.assign(input.begin(), input.end());
        return writeResult;
    }

    ps5emu::hle::GuestFileResult
    Seek(std::int32_t descriptor,
         std::int64_t offset,
         std::int32_t whence) override {
        lastDescriptor = descriptor;
        lastOffset = offset;
        lastWhence = whence;
        return seekResult;
    }

    std::string lastPath;
    std::int32_t lastFlags = 0;
    std::uint16_t lastMode = 0;
    std::int32_t lastDescriptor = 0;
    std::int64_t lastOffset = 0;
    std::int32_t lastWhence = 0;

    ps5emu::hle::GuestFileResult openResult{.value = 7};
    ps5emu::hle::GuestFileResult closeResult{};
    ps5emu::hle::GuestFileResult writeResult{.value = 3};
    ps5emu::hle::GuestFileResult seekResult{.value = 123};

    std::uint64_t readError = 0;
    std::vector<std::byte> readData{
        std::byte{'a'},
        std::byte{'b'},
        std::byte{'c'},
    };
    std::vector<std::byte> written;
};

const ps5emu::hle::HleService& Find(
    const ps5emu::hle::HleRegistry& registry,
    const char* name) {
    const auto* service =
        registry.Find(
            "libkernel",
            ps5emu::hle::Nid::Compute(name));
    assert(service != nullptr);
    return *service;
}

std::uint64_t Invoke(
    const ps5emu::hle::HleRegistry& registry,
    const char* name,
    TestMemory* memory,
    TestFiles* files,
    std::array<std::uint64_t, 3> arguments) {
    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = arguments[0];
    frame.arguments[1] = arguments[1];
    frame.arguments[2] = arguments[2];
    frame.memory = memory;
    frame.files = files;

    Find(registry, name).handler(frame);
    return frame.returnValue;
}

} // namespace

int main() {
    constexpr std::uint64_t sceFault = 0x8002000eull;
    constexpr std::uint64_t sceInvalid = 0x80020016ull;
    constexpr std::uint64_t sceNotImplemented = 0x8002004eull;

    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelFile::Register(registry, "libkernel");

    assert(registry.Size() == 5);

    TestMemory memory(256);
    TestFiles files;
    memory.StoreString(16, "/app0/test.bin");

    assert(Invoke(
        registry,
        "sceKernelOpen",
        &memory,
        &files,
        {16, 0x1234, 0644}) == 7);
    assert(files.lastPath == "/app0/test.bin");
    assert(files.lastFlags == 0x1234);
    assert(files.lastMode == 0644);

    assert(Invoke(
        registry,
        "sceKernelRead",
        &memory,
        &files,
        {7, 64, 3}) == 3);
    assert(memory.At(64) == std::byte{'a'});
    assert(memory.At(66) == std::byte{'c'});

    const std::array<std::byte, 3> writeData{
        std::byte{'x'}, std::byte{'y'}, std::byte{'z'}
    };
    memory.Write(80, writeData);

    assert(Invoke(
        registry,
        "sceKernelWrite",
        &memory,
        &files,
        {7, 80, 3}) == 3);
    assert(files.written.size() == 3);
    assert(files.written[0] == std::byte{'x'});
    assert(files.written[2] == std::byte{'z'});

    assert(Invoke(
        registry,
        "sceKernelLseek",
        &memory,
        &files,
        {7,
         std::bit_cast<std::uint64_t>(std::int64_t{-4}),
         2}) == 123);
    assert(files.lastOffset == -4);
    assert(files.lastWhence == 2);

    assert(Invoke(
        registry,
        "sceKernelClose",
        &memory,
        &files,
        {7, 0, 0}) == 0);
    assert(files.lastDescriptor == 7);

    assert(Invoke(
        registry,
        "sceKernelRead",
        nullptr,
        &files,
        {7, 64, 1}) == sceFault);

    assert(Invoke(
        registry,
        "sceKernelLseek",
        &memory,
        &files,
        {7, 0, 9}) == sceInvalid);

    assert(Invoke(
        registry,
        "sceKernelOpen",
        &memory,
        nullptr,
        {16, 0, 0}) == sceNotImplemented);

    files.readError = 0x80020009ull;
    assert(Invoke(
        registry,
        "sceKernelRead",
        &memory,
        &files,
        {7, 64, 3}) == 0x80020009ull);

    return 0;
}
