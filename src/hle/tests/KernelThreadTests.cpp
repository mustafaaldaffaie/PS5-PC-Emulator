#include <ps5emu/hle/GuestThreadAccess.hpp>
#include <ps5emu/hle/KernelThread.hpp>
#include <ps5emu/hle/Nid.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

class TestMemory final
    : public ps5emu::hle::GuestMemoryAccess {
public:
    explicit TestMemory(std::size_t size)
        : bytes_(size, std::byte{0}) {
    }

    void Read(
        std::uint64_t address,
        std::span<std::byte> output) const override {
        if (address > bytes_.size()) {
            throw std::out_of_range("read");
        }

        const auto offset =
            static_cast<std::size_t>(address);

        if (output.size() >
            bytes_.size() - offset) {
            throw std::out_of_range("read");
        }

        std::copy_n(
            bytes_.begin() +
                static_cast<std::ptrdiff_t>(offset),
            output.size(),
            output.begin());
    }

    void Write(
        std::uint64_t address,
        std::span<const std::byte> input) override {
        if (address > bytes_.size()) {
            throw std::out_of_range("write");
        }

        const auto offset =
            static_cast<std::size_t>(address);

        if (input.size() >
            bytes_.size() - offset) {
            throw std::out_of_range("write");
        }

        std::copy(
            input.begin(),
            input.end(),
            bytes_.begin() +
                static_cast<std::ptrdiff_t>(offset));
    }

    [[nodiscard]] std::uint64_t
    LoadU64(std::uint64_t address) const {
        std::array<std::byte, sizeof(std::uint64_t)> bytes{};
        Read(address, bytes);

        std::uint64_t value = 0;
        std::memcpy(
            &value,
            bytes.data(),
            sizeof(value));
        return value;
    }

private:
    std::vector<std::byte> bytes_;
};

class TestThreads final
    : public ps5emu::hle::GuestThreadAccess {
public:
    ps5emu::hle::GuestThreadCreateResult
    Create(
        const ps5emu::hle::GuestThreadCreateRequest& request) override {
        lastCreate = request;

        return ps5emu::hle::GuestThreadCreateResult{
            .errorCode = createError,
            .handle = createHandle,
        };
    }

    ps5emu::hle::GuestThreadJoinResult
    Join(std::uint64_t handle) override {
        lastJoinHandle = handle;

        return ps5emu::hle::GuestThreadJoinResult{
            .errorCode = joinError,
            .returnValue = joinValue,
        };
    }

    std::uint64_t
    CurrentThreadHandle() const noexcept override {
        return currentHandle;
    }

    ps5emu::hle::GuestThreadCreateRequest lastCreate{};
    std::uint64_t lastJoinHandle = 0;
    std::uint64_t currentHandle = 0x7000;
    std::uint64_t createError = 0;
    std::uint64_t createHandle = 0x7100;
    std::uint64_t joinError = 0;
    std::uint64_t joinValue = 0x7200;
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
    ps5emu::hle::GuestMemoryAccess* memory = nullptr,
    ps5emu::hle::GuestThreadAccess* threads = nullptr,
    std::array<std::uint64_t, 5> arguments = {}) {
    ps5emu::hle::HleCallFrame frame;
    for (std::size_t index = 0;
         index < arguments.size();
         ++index) {
        frame.arguments[index] =
            arguments[index];
    }

    frame.memory = memory;
    frame.threads = threads;

    Find(registry, name).handler(frame);
    return frame.returnValue;
}

} // namespace

int main() {
    constexpr std::uint64_t sceDeadlock =
        0x8002000bull;
    constexpr std::uint64_t sceFault =
        0x8002000eull;
    constexpr std::uint64_t sceInvalid =
        0x80020016ull;

    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelThread::Register(
        registry,
        "libkernel");

    assert(registry.Size() == 10);

    const auto self =
        Invoke(
            registry,
            "scePthreadSelf");

    assert(self != 0);
    assert(
        Invoke(
            registry,
            "scePthreadSelf") ==
        self);
    assert(
        Invoke(
            registry,
            "pthread_self") ==
        self);

    assert(
        Invoke(
            registry,
            "scePthreadEqual",
            nullptr,
            nullptr,
            {self, self}) == 1);
    assert(
        Invoke(
            registry,
            "pthread_equal",
            nullptr,
            nullptr,
            {self, self + 0x100}) == 0);

    assert(
        Invoke(
            registry,
            "sched_yield") == 0);
    assert(
        Invoke(
            registry,
            "scePthreadYield") == 0);

    std::uint64_t otherThread = 0;
    std::thread thread(
        [&] {
            otherThread =
                Invoke(
                    registry,
                    "scePthreadSelf");
        });
    thread.join();

    assert(otherThread != 0);
    assert(otherThread != self);

    TestMemory memory(256);
    TestThreads threads;

    assert(
        Invoke(
            registry,
            "scePthreadSelf",
            &memory,
            &threads) ==
        threads.currentHandle);

    assert(
        Invoke(
            registry,
            "scePthreadCreate",
            &memory,
            &threads,
            {32, 0x1000, 0x401000, 0x55, 0x2000}) ==
        0);

    assert(memory.LoadU64(32) == threads.createHandle);
    assert(threads.lastCreate.attributeAddress == 0x1000);
    assert(threads.lastCreate.entryPoint == 0x401000);
    assert(threads.lastCreate.argument == 0x55);
    assert(threads.lastCreate.nameAddress == 0x2000);

    assert(
        Invoke(
            registry,
            "pthread_create",
            &memory,
            &threads,
            {40, 0x1100, 0x402000, 0x66}) ==
        0);

    assert(memory.LoadU64(40) == threads.createHandle);
    assert(threads.lastCreate.attributeAddress == 0x1100);
    assert(threads.lastCreate.entryPoint == 0x402000);
    assert(threads.lastCreate.argument == 0x66);
    assert(threads.lastCreate.nameAddress == 0);

    threads.joinValue = 0x12345678;
    assert(
        Invoke(
            registry,
            "scePthreadJoin",
            &memory,
            &threads,
            {threads.createHandle, 48}) ==
        0);

    assert(threads.lastJoinHandle == threads.createHandle);
    assert(memory.LoadU64(48) == 0x12345678);

    threads.joinValue = 0x87654321;
    assert(
        Invoke(
            registry,
            "pthread_join",
            &memory,
            &threads,
            {threads.createHandle, 56}) ==
        0);

    assert(memory.LoadU64(56) == 0x87654321);

    assert(
        Invoke(
            registry,
            "scePthreadCreate",
            &memory,
            nullptr,
            {64, 0, 0x401000, 0}) ==
        sceInvalid);

    assert(
        Invoke(
            registry,
            "scePthreadCreate",
            nullptr,
            &threads,
            {64, 0, 0x401000, 0}) ==
        sceFault);

    assert(
        Invoke(
            registry,
            "scePthreadCreate",
            &memory,
            &threads,
            {64, 0, 0, 0}) ==
        sceInvalid);

    threads.createError =
        0x80020010ull;
    assert(
        Invoke(
            registry,
            "pthread_create",
            &memory,
            &threads,
            {64, 0, 0x401000, 0}) ==
        16);
    threads.createError = 0;

    assert(
        Invoke(
            registry,
            "scePthreadJoin",
            &memory,
            &threads,
            {threads.currentHandle, 0}) ==
        sceDeadlock);

    assert(
        Invoke(
            registry,
            "pthread_join",
            &memory,
            &threads,
            {threads.currentHandle, 0}) ==
        11);

    assert(
        Invoke(
            registry,
            "scePthreadJoin",
            &memory,
            nullptr,
            {threads.createHandle, 0}) ==
        sceInvalid);

    threads.joinError =
        0x80020016ull;
    assert(
        Invoke(
            registry,
            "pthread_join",
            &memory,
            &threads,
            {threads.createHandle, 0}) ==
        22);

    return 0;
}
