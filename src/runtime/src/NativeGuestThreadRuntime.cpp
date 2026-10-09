#include <ps5emu/runtime/NativeGuestThreadRuntime.hpp>

#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

#include <ps5emu/memory/GuestMemory.hpp>
#include <ps5emu/runtime/GuestThreadMemory.hpp>

namespace ps5emu::runtime {
namespace {

constexpr std::uint64_t kSceOk =
    0;
constexpr std::uint64_t kSceErrorNoSuchThread =
    0x80020003ull;
constexpr std::uint64_t kSceErrorIo =
    0x80020005ull;
constexpr std::uint64_t kSceErrorDeadlock =
    0x8002000bull;
constexpr std::uint64_t kSceErrorInvalid =
    0x80020016ull;
constexpr std::uint64_t kSceErrorAgain =
    0x80020023ull;
constexpr std::uint64_t kSceErrorNotImplemented =
    0x8002004eull;

thread_local const NativeGuestThreadRuntime*
    g_currentRuntime = nullptr;
thread_local std::uint64_t
    g_currentHandle = 0;

std::uint64_t CheckedOrdinalAddress(
    std::uint64_t base,
    std::uint64_t stride,
    std::uint64_t ordinal,
    const char* message) {
    if (ordinal != 0 &&
        stride >
            std::numeric_limits<std::uint64_t>::max() /
                ordinal) {
        throw std::overflow_error(message);
    }

    const auto offset =
        stride * ordinal;

    if (base >
        std::numeric_limits<std::uint64_t>::max() -
            offset) {
        throw std::overflow_error(message);
    }

    return base + offset;
}

} // namespace

struct NativeGuestThreadRuntime::ThreadRecord {
    std::uint64_t handle = 0;
    std::thread worker;

    std::uint64_t returnValue = 0;
    std::uint64_t executionError = 0;
    bool joinStarted = false;
};

NativeGuestThreadRuntime::NativeGuestThreadRuntime(
    std::span<const std::byte> executableBytes,
    elf::Image image,
    NativeImage& nativeImage,
    const hle::HleRegistry& registry,
    const HleThunkTable& thunks,
    std::span<const NativeSyscallTrap> syscallTraps,
    NativeGuestThreadRuntimeOptions options,
    hle::GuestFileSystemAccess* files)
    : executableBytes_(
          executableBytes.begin(),
          executableBytes.end()),
      image_(std::move(image)),
      nativeImage_(nativeImage),
      registry_(registry),
      thunks_(thunks),
      syscallTraps_(
          syscallTraps.begin(),
          syscallTraps.end()),
      options_(options),
      files_(files) {
    if (options_.workerStackSize < 16) {
        throw std::invalid_argument(
            "Worker guest stack size must be at least 16 bytes");
    }

    if (options_.workerStackStride <
        options_.workerStackSize) {
        throw std::invalid_argument(
            "Worker guest stack stride is smaller than the stack size");
    }

    if (options_.workerStackStride == 0 ||
        options_.workerTlsStride == 0 ||
        options_.handleStride == 0) {
        throw std::invalid_argument(
            "Guest thread runtime strides cannot be zero");
    }

    mainThreadHandle_ =
        HandleForOrdinal(0);

    previousRuntime_ =
        g_currentRuntime;
    previousHandle_ =
        g_currentHandle;

    g_currentRuntime = this;
    g_currentHandle =
        mainThreadHandle_;
}

NativeGuestThreadRuntime::~NativeGuestThreadRuntime() {
    JoinRemainingThreads();

    if (g_currentRuntime == this) {
        g_currentRuntime =
            previousRuntime_;
        g_currentHandle =
            previousHandle_;
    }
}

hle::GuestThreadCreateResult
NativeGuestThreadRuntime::Create(
    const hle::GuestThreadCreateRequest& request) {
    if (request.entryPoint == 0) {
        return {
            .errorCode = kSceErrorInvalid,
        };
    }

    const auto entryMapping =
        nativeImage_.FindMapping(
            request.entryPoint,
            1);

    if (!entryMapping.has_value() ||
        !memory::HasProtection(
            entryMapping->protection,
            memory::Protection::Execute)) {
        return {
            .errorCode = kSceErrorInvalid,
        };
    }

    // Attribute parsing will be added with the pthread attribute layer.
    // Reject non-default attributes rather than silently ignoring them.
    if (request.attributeAddress != 0) {
        return {
            .errorCode = kSceErrorInvalid,
        };
    }

    std::uint64_t ordinal = 0;
    std::uint64_t handle = 0;

    try {
        std::lock_guard lock(mutex_);

        ordinal =
            nextOrdinal_++;

        handle =
            HandleForOrdinal(
                ordinal);
    } catch (const std::exception&) {
        return {
            .errorCode = kSceErrorAgain,
        };
    }

    memory::GuestMemory threadMemory;
    GuestThreadMemoryLayout layout;

    try {
        GuestThreadMemoryOptions memoryOptions;
        memoryOptions.stackAddress =
            AddressForOrdinal(
                options_.workerStackBase,
                options_.workerStackStride,
                ordinal - 1);
        memoryOptions.stackSize =
            options_.workerStackSize;
        memoryOptions.stackGuard =
            options_.stackGuard;

        if (image_.tlsSegment.has_value() &&
            image_.tlsSegment->memorySize != 0) {
            memoryOptions.tlsAddress =
                AddressForOrdinal(
                    options_.workerTlsBase,
                    options_.workerTlsStride,
                    ordinal - 1);
        }

        layout =
            GuestThreadMemory::Create(
                executableBytes_,
                image_,
                threadMemory,
                memoryOptions);

        nativeImage_.AddMappings(
            threadMemory);
    } catch (const std::exception&) {
        return {
            .errorCode = kSceErrorAgain,
        };
    }

    SysvGuestContext context;
    context.rip =
        request.entryPoint;
    context.rdi =
        request.argument;

    GuestThreadMemory::ApplyToContext(
        layout,
        context);

    auto record =
        std::make_shared<ThreadRecord>();
    record->handle =
        handle;

    {
        std::lock_guard lock(mutex_);

        const auto inserted =
            threads_.emplace(
                handle,
                record);

        if (!inserted.second) {
            return {
                .errorCode = kSceErrorAgain,
            };
        }
    }

    try {
        record->worker =
            std::thread(
                [this, record, context]() mutable {
                    const auto* previousRuntime =
                        g_currentRuntime;
                    const auto previousHandle =
                        g_currentHandle;

                    g_currentRuntime = this;
                    g_currentHandle =
                        record->handle;

                    try {
                        NativeHleExecutor executor;

                        const auto result =
                            executor.Run(
                                context,
                                nativeImage_,
                                registry_,
                                thunks_,
                                syscallTraps_,
                                this,
                                files_);

                        if (result.interceptedSyscall) {
                            record->executionError =
                                kSceErrorNotImplemented;
                        } else {
                            record->returnValue =
                                context.rax;
                        }
                    } catch (const std::exception&) {
                        record->executionError =
                            kSceErrorIo;
                    } catch (...) {
                        record->executionError =
                            kSceErrorIo;
                    }

                    g_currentRuntime =
                        previousRuntime;
                    g_currentHandle =
                        previousHandle;
                });
    } catch (const std::exception&) {
        std::lock_guard lock(mutex_);
        threads_.erase(handle);

        return {
            .errorCode = kSceErrorAgain,
        };
    }

    return {
        .errorCode = kSceOk,
        .handle = handle,
    };
}

hle::GuestThreadJoinResult
NativeGuestThreadRuntime::Join(
    std::uint64_t handle) {
    if (handle == 0) {
        return {
            .errorCode = kSceErrorInvalid,
        };
    }

    if (handle ==
        CurrentThreadHandle()) {
        return {
            .errorCode = kSceErrorDeadlock,
        };
    }

    std::shared_ptr<ThreadRecord> record;

    {
        std::lock_guard lock(mutex_);

        const auto found =
            threads_.find(handle);

        if (found ==
            threads_.end()) {
            return {
                .errorCode =
                    kSceErrorNoSuchThread,
            };
        }

        record =
            found->second;

        if (record->joinStarted) {
            return {
                .errorCode =
                    kSceErrorInvalid,
            };
        }

        record->joinStarted =
            true;
    }

    try {
        if (record->worker.joinable()) {
            record->worker.join();
        }
    } catch (const std::exception&) {
        return {
            .errorCode = kSceErrorIo,
        };
    }

    {
        std::lock_guard lock(mutex_);
        threads_.erase(handle);
    }

    if (record->executionError != 0) {
        return {
            .errorCode =
                record->executionError,
        };
    }

    return {
        .errorCode = kSceOk,
        .returnValue =
            record->returnValue,
    };
}

std::uint64_t
NativeGuestThreadRuntime::CurrentThreadHandle()
    const noexcept {
    if (g_currentRuntime != this) {
        return 0;
    }

    return g_currentHandle;
}

std::uint64_t
NativeGuestThreadRuntime::MainThreadHandle()
    const noexcept {
    return mainThreadHandle_;
}

std::uint64_t
NativeGuestThreadRuntime::AddressForOrdinal(
    std::uint64_t base,
    std::uint64_t stride,
    std::uint64_t ordinal) const {
    return CheckedOrdinalAddress(
        base,
        stride,
        ordinal,
        "Guest thread address arena overflows");
}

std::uint64_t
NativeGuestThreadRuntime::HandleForOrdinal(
    std::uint64_t ordinal) const {
    return CheckedOrdinalAddress(
        options_.handleBase,
        options_.handleStride,
        ordinal,
        "Guest thread handle space overflows");
}

void NativeGuestThreadRuntime::JoinRemainingThreads()
    noexcept {
    std::vector<
        std::shared_ptr<ThreadRecord>>
        records;

    {
        std::lock_guard lock(mutex_);

        records.reserve(
            threads_.size());

        for (auto& [handle, record] :
             threads_) {
            static_cast<void>(handle);
            records.push_back(record);
        }

        threads_.clear();
    }

    for (auto& record : records) {
        if (!record->worker.joinable()) {
            continue;
        }

        if (record->worker.get_id() ==
            std::this_thread::get_id()) {
            record->worker.detach();
            continue;
        }

        try {
            record->worker.join();
        } catch (...) {
            // Destruction cannot propagate host thread cleanup failures.
        }
    }
}

} // namespace ps5emu::runtime
