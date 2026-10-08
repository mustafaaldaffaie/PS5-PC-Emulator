#include <ps5emu/hle/KernelMutex.hpp>
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
            throw std::runtime_error("read");
        }
        const auto offset =
            static_cast<std::size_t>(address);
        if (output.size() > bytes_.size() - offset) {
            throw std::runtime_error("read");
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
            throw std::runtime_error("write");
        }
        const auto offset =
            static_cast<std::size_t>(address);
        if (input.size() > bytes_.size() - offset) {
            throw std::runtime_error("write");
        }
        std::copy(
            input.begin(),
            input.end(),
            bytes_.begin() +
                static_cast<std::ptrdiff_t>(offset));
    }

    void StoreU64(
        std::uint64_t address,
        std::uint64_t value) {
        std::array<std::byte, sizeof(value)> bytes{};
        std::memcpy(bytes.data(), &value, sizeof(value));
        Write(address, bytes);
    }

    std::uint64_t LoadU64(
        std::uint64_t address) const {
        std::array<std::byte, sizeof(std::uint64_t)> bytes{};
        Read(address, bytes);
        std::uint64_t value = 0;
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }

private:
    std::vector<std::byte> bytes_;
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
    TestMemory& memory,
    const char* name,
    std::uint64_t a0 = 0,
    std::uint64_t a1 = 0,
    std::uint64_t a2 = 0) {
    ps5emu::hle::HleCallFrame frame;
    frame.arguments[0] = a0;
    frame.arguments[1] = a1;
    frame.arguments[2] = a2;
    frame.memory = &memory;
    Find(registry, name).handler(frame);
    return frame.returnValue;
}

} // namespace

int main() {
    constexpr std::uint64_t ok = 0;
    constexpr std::uint64_t deadlock = 0x8002000bull;
    constexpr std::uint64_t fault = 0x8002000eull;
    constexpr std::uint64_t busy = 0x80020010ull;
    constexpr std::uint64_t invalid = 0x80020016ull;

    ps5emu::hle::HleRegistry registry;
    ps5emu::hle::KernelMutex::Register(
        registry,
        "libkernel");

    assert(registry.Size() == 18);

    TestMemory memory(256);
    constexpr std::uint64_t attr = 32;
    constexpr std::uint64_t mutex = 64;

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexattrInit",
            attr) == ok);
    assert(memory.LoadU64(attr) != 0);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexattrSettype",
            attr,
            0) == invalid);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexattrSettype",
            attr,
            2) == ok);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexInit",
            mutex,
            attr) == ok);
    assert(memory.LoadU64(mutex) != 0);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexLock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexLock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexUnlock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexUnlock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexDestroy",
            mutex) == ok);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexattrSettype",
            attr,
            1) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexInit",
            mutex,
            attr) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexLock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexLock",
            mutex) == deadlock);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexTrylock",
            mutex) == busy);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexDestroy",
            mutex) == busy);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexUnlock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexDestroy",
            mutex) == ok);

    memory.StoreU64(mutex, 1);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexTrylock",
            mutex) == ok);
    assert(memory.LoadU64(mutex) != 1);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexTrylock",
            mutex) == busy);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexUnlock",
            mutex) == ok);
    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexDestroy",
            mutex) == ok);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexattrDestroy",
            attr) == ok);
    assert(memory.LoadU64(attr) == 0);


    constexpr std::uint64_t posixAttr = 96;
    constexpr std::uint64_t posixMutex = 128;

    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutexattr_init",
            posixAttr) == 0);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutexattr_settype",
            posixAttr,
            1) == 0);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutex_init",
            posixMutex,
            posixAttr) == 0);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutex_lock",
            posixMutex) == 0);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutex_trylock",
            posixMutex) == 16);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutex_unlock",
            posixMutex) == 0);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutex_destroy",
            posixMutex) == 0);
    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutexattr_destroy",
            posixAttr) == 0);

    assert(
        Invoke(
            registry,
            memory,
            "pthread_mutex_init",
            0,
            0) == 14);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexInit",
            0,
            0) == fault);

    assert(
        Invoke(
            registry,
            memory,
            "scePthreadMutexLock",
            240) == invalid);

    return 0;
}
