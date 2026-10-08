#include <ps5emu/hle/KernelThread.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ps5emu::hle {
namespace {

constexpr std::uint64_t kSceOk = 0;
constexpr std::uint64_t kSceErrorDeadlock = 0x8002000bull;
constexpr std::uint64_t kSceErrorFault = 0x8002000eull;
constexpr std::uint64_t kSceErrorInvalid = 0x80020016ull;

constexpr std::uint64_t kThreadHandleBase =
    0x00007ffc00000000ull;
constexpr std::uint64_t kThreadHandleStride =
    0x100ull;

std::atomic<std::uint64_t> g_nextThreadOrdinal{1};

std::uint64_t AllocateThreadHandle() {
    const auto ordinal =
        g_nextThreadOrdinal.fetch_add(
            1,
            std::memory_order_relaxed);

    if (ordinal >
        (std::numeric_limits<std::uint64_t>::max() -
         kThreadHandleBase) /
            kThreadHandleStride) {
        throw std::overflow_error(
            "Synthetic guest thread handle space is exhausted");
    }

    return kThreadHandleBase +
        ordinal * kThreadHandleStride;
}

std::uint64_t SyntheticCurrentThreadHandle() {
    // HLE dispatch runs only after the native executor restores the host FS
    // base, so host TLS is valid here even when the guest itself uses FS.
    thread_local const std::uint64_t handle =
        AllocateThreadHandle();
    return handle;
}

std::uint64_t CurrentThreadHandle(
    const HleCallFrame& frame) {
    if (frame.threads != nullptr) {
        const auto handle =
            frame.threads->CurrentThreadHandle();

        if (handle != 0) {
            return handle;
        }
    }

    return SyntheticCurrentThreadHandle();
}

bool ValidateWritableU64(
    HleCallFrame& frame,
    std::uint64_t address) {
    if (frame.memory == nullptr || address == 0) {
        return false;
    }

    std::array<std::byte, sizeof(std::uint64_t)> bytes{};

    try {
        frame.memory->Read(address, bytes);
        frame.memory->Write(address, bytes);
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

bool WriteU64(
    HleCallFrame& frame,
    std::uint64_t address,
    std::uint64_t value) {
    if (frame.memory == nullptr || address == 0) {
        return false;
    }

    std::array<std::byte, sizeof(value)> bytes{};
    std::memcpy(
        bytes.data(),
        &value,
        sizeof(value));

    try {
        frame.memory->Write(
            address,
            bytes);
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

std::uint64_t ToPosixError(
    std::uint64_t value) {
    if (value == 0) {
        return 0;
    }

    if ((value & 0xffff0000ull) ==
        0x80020000ull) {
        return value & 0xffffull;
    }

    return value;
}

void ThreadSelf(HleCallFrame& frame) {
    frame.returnValue =
        CurrentThreadHandle(frame);
}

void ThreadEqual(HleCallFrame& frame) {
    frame.returnValue =
        frame.arguments[0] ==
                frame.arguments[1]
            ? 1u
            : 0u;
}

void ThreadYield(HleCallFrame& frame) {
    std::this_thread::yield();
    frame.returnValue = 0;
}

void ThreadCreate(
    HleCallFrame& frame,
    bool posix) {
    const auto outputAddress =
        frame.arguments[0];
    const auto entryPoint =
        frame.arguments[2];

    if (outputAddress == 0 ||
        frame.memory == nullptr) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorFault)
                : kSceErrorFault;
        return;
    }

    if (entryPoint == 0 ||
        frame.threads == nullptr) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorInvalid)
                : kSceErrorInvalid;
        return;
    }

    if (!ValidateWritableU64(
            frame,
            outputAddress)) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorFault)
                : kSceErrorFault;
        return;
    }

    const auto result =
        frame.threads->Create(
            GuestThreadCreateRequest{
                .attributeAddress =
                    frame.arguments[1],
                .entryPoint = entryPoint,
                .argument = frame.arguments[3],
                .nameAddress =
                    posix ? 0 : frame.arguments[4],
            });

    if (result.errorCode != 0) {
        frame.returnValue =
            posix
                ? ToPosixError(result.errorCode)
                : result.errorCode;
        return;
    }

    if (result.handle == 0) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorInvalid)
                : kSceErrorInvalid;
        return;
    }

    if (!WriteU64(
            frame,
            outputAddress,
            result.handle)) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorFault)
                : kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void ThreadJoin(
    HleCallFrame& frame,
    bool posix) {
    const auto handle =
        frame.arguments[0];
    const auto returnValueAddress =
        frame.arguments[1];

    if (handle == 0 ||
        frame.threads == nullptr) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorInvalid)
                : kSceErrorInvalid;
        return;
    }

    if (handle ==
        CurrentThreadHandle(frame)) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorDeadlock)
                : kSceErrorDeadlock;
        return;
    }

    if (returnValueAddress != 0 &&
        !ValidateWritableU64(
            frame,
            returnValueAddress)) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorFault)
                : kSceErrorFault;
        return;
    }

    const auto result =
        frame.threads->Join(handle);

    if (result.errorCode != 0) {
        frame.returnValue =
            posix
                ? ToPosixError(result.errorCode)
                : result.errorCode;
        return;
    }

    if (returnValueAddress != 0 &&
        !WriteU64(
            frame,
            returnValueAddress,
            result.returnValue)) {
        frame.returnValue =
            posix
                ? ToPosixError(kSceErrorFault)
                : kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

} // namespace

void KernelThread::Register(
    HleRegistry& registry,
    std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Kernel thread module name cannot be empty");
    }

    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "scePthreadSelf",
        ThreadSelf);

    registry.RegisterSymbol(
        moduleName,
        "scePthreadEqual",
        ThreadEqual);

    registry.RegisterSymbol(
        moduleName,
        "scePthreadYield",
        ThreadYield);

    registry.RegisterSymbol(
        moduleName,
        "scePthreadCreate",
        [](HleCallFrame& frame) {
            ThreadCreate(
                frame,
                false);
        });

    registry.RegisterSymbol(
        moduleName,
        "scePthreadJoin",
        [](HleCallFrame& frame) {
            ThreadJoin(
                frame,
                false);
        });

    registry.RegisterSymbol(
        moduleName,
        "pthread_self",
        ThreadSelf);

    registry.RegisterSymbol(
        moduleName,
        "pthread_equal",
        ThreadEqual);

    registry.RegisterSymbol(
        moduleName,
        "pthread_create",
        [](HleCallFrame& frame) {
            ThreadCreate(
                frame,
                true);
        });

    registry.RegisterSymbol(
        moduleName,
        "pthread_join",
        [](HleCallFrame& frame) {
            ThreadJoin(
                frame,
                true);
        });

    registry.RegisterSymbol(
        std::move(module),
        "sched_yield",
        ThreadYield);
}

} // namespace ps5emu::hle
