#include <ps5emu/hle/KernelMutex.hpp>

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

namespace ps5emu::hle {
namespace {

constexpr std::uint64_t kSceOk = 0;
constexpr std::uint64_t kSceErrorPerm = 0x80020001ull;
constexpr std::uint64_t kSceErrorDeadlock = 0x8002000bull;
constexpr std::uint64_t kSceErrorFault = 0x8002000eull;
constexpr std::uint64_t kSceErrorBusy = 0x80020010ull;
constexpr std::uint64_t kSceErrorInvalid = 0x80020016ull;

constexpr std::uint64_t kStaticAdaptiveMutex = 1;
constexpr std::uint64_t kHandleBase = 0x00007ffb00000000ull;
constexpr std::uint64_t kHandleStride = 0x100ull;

enum class MutexType : std::uint32_t {
    ErrorCheck = 1,
    Recursive = 2,
    Normal = 3,
    Adaptive = 4,
};

struct MutexAttribute {
    MutexType type = MutexType::ErrorCheck;
};

struct MutexObject {
    explicit MutexObject(MutexType value)
        : type(value) {
    }

    MutexType type;
    std::mutex lock;
    std::condition_variable condition;
    std::thread::id owner;
    std::uint32_t recursion = 0;
    bool held = false;
};

struct MutexState {
    std::mutex lock;
    std::uint64_t nextHandle = kHandleBase;
    std::unordered_map<std::uint64_t, MutexAttribute> attributes;
    std::unordered_map<std::uint64_t, std::shared_ptr<MutexObject>> mutexes;

    std::uint64_t AllocateHandle() {
        const auto handle = nextHandle;
        nextHandle += kHandleStride;
        if (nextHandle < handle) {
            throw std::overflow_error(
                "Synthetic mutex handle space is exhausted");
        }
        return handle;
    }
};

bool ReadHandle(HleCallFrame& frame,
                std::uint64_t address,
                std::uint64_t& value) {
    if (address == 0 || frame.memory == nullptr) {
        return false;
    }

    std::array<std::byte, sizeof(value)> bytes{};
    try {
        frame.memory->Read(address, bytes);
    } catch (const std::exception&) {
        return false;
    }

    std::memcpy(&value, bytes.data(), sizeof(value));
    return true;
}

bool WriteHandle(HleCallFrame& frame,
                 std::uint64_t address,
                 std::uint64_t value) {
    if (address == 0 || frame.memory == nullptr) {
        return false;
    }

    std::array<std::byte, sizeof(value)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));

    try {
        frame.memory->Write(address, bytes);
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

std::shared_ptr<MutexObject> ResolveMutex(
    HleCallFrame& frame,
    MutexState& state,
    std::uint64_t pointerAddress,
    bool initializeStatic) {
    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointerAddress, handle)) {
        return {};
    }

    std::scoped_lock stateLock(state.lock);

    if (handle == kStaticAdaptiveMutex && initializeStatic) {
        handle = state.AllocateHandle();
        state.mutexes.emplace(
            handle,
            std::make_shared<MutexObject>(
                MutexType::Adaptive));

        if (!WriteHandle(frame, pointerAddress, handle)) {
            state.mutexes.erase(handle);
            return {};
        }
    }

    const auto found = state.mutexes.find(handle);
    return found == state.mutexes.end()
        ? std::shared_ptr<MutexObject>{}
        : found->second;
}

void AttrInit(HleCallFrame& frame,
              MutexState& state) {
    const auto output = frame.arguments[0];
    if (output == 0 || frame.memory == nullptr) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::scoped_lock lock(state.lock);
    const auto handle = state.AllocateHandle();
    state.attributes.emplace(handle, MutexAttribute{});

    if (!WriteHandle(frame, output, handle)) {
        state.attributes.erase(handle);
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void AttrDestroy(HleCallFrame& frame,
                 MutexState& state) {
    const auto pointer = frame.arguments[0];
    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    {
        std::scoped_lock lock(state.lock);
        if (state.attributes.erase(handle) == 0) {
            frame.returnValue = kSceErrorInvalid;
            return;
        }
    }

    if (!WriteHandle(frame, pointer, 0)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void AttrSetType(HleCallFrame& frame,
                 MutexState& state) {
    const auto pointer = frame.arguments[0];
    const auto rawType =
        static_cast<std::uint32_t>(frame.arguments[1]);

    if (rawType < 1 || rawType > 4) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::scoped_lock lock(state.lock);
    const auto found = state.attributes.find(handle);
    if (found == state.attributes.end()) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    found->second.type =
        static_cast<MutexType>(rawType);
    frame.returnValue = kSceOk;
}

void AttrSetProtocol(HleCallFrame& frame,
                     MutexState& state) {
    const auto pointer = frame.arguments[0];
    const auto protocol =
        static_cast<std::uint32_t>(frame.arguments[1]);

    if (protocol > 1) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::scoped_lock lock(state.lock);
    if (!state.attributes.contains(handle)) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    frame.returnValue = kSceOk;
}

void MutexInit(HleCallFrame& frame,
               MutexState& state) {
    const auto mutexPointer = frame.arguments[0];
    const auto attrPointer = frame.arguments[1];

    if (mutexPointer == 0 || frame.memory == nullptr) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    MutexType type = MutexType::ErrorCheck;

    std::scoped_lock lock(state.lock);

    if (attrPointer != 0) {
        std::uint64_t attrHandle = 0;
        if (!ReadHandle(frame, attrPointer, attrHandle)) {
            frame.returnValue = kSceErrorFault;
            return;
        }

        const auto found =
            state.attributes.find(attrHandle);
        if (found == state.attributes.end()) {
            frame.returnValue = kSceErrorInvalid;
            return;
        }
        type = found->second.type;
    }

    const auto handle = state.AllocateHandle();
    state.mutexes.emplace(
        handle,
        std::make_shared<MutexObject>(type));

    if (!WriteHandle(frame, mutexPointer, handle)) {
        state.mutexes.erase(handle);
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void MutexDestroy(HleCallFrame& frame,
                  MutexState& state) {
    const auto pointer = frame.arguments[0];

    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    if (handle == kStaticAdaptiveMutex) {
        if (!WriteHandle(frame, pointer, 0)) {
            frame.returnValue = kSceErrorFault;
            return;
        }
        frame.returnValue = kSceOk;
        return;
    }

    std::shared_ptr<MutexObject> mutex;
    {
        std::scoped_lock lock(state.lock);
        const auto found = state.mutexes.find(handle);
        if (found == state.mutexes.end()) {
            frame.returnValue = kSceErrorInvalid;
            return;
        }
        mutex = found->second;
    }

    {
        std::scoped_lock mutexLock(mutex->lock);
        if (mutex->held) {
            frame.returnValue = kSceErrorBusy;
            return;
        }
    }

    {
        std::scoped_lock lock(state.lock);
        state.mutexes.erase(handle);
    }

    if (!WriteHandle(frame, pointer, 0)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void MutexLock(HleCallFrame& frame,
               MutexState& state) {
    const auto mutex =
        ResolveMutex(
            frame,
            state,
            frame.arguments[0],
            true);

    if (!mutex) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    const auto current = std::this_thread::get_id();
    std::unique_lock lock(mutex->lock);

    if (mutex->held && mutex->owner == current) {
        if (mutex->type == MutexType::Recursive) {
            ++mutex->recursion;
            frame.returnValue = kSceOk;
            return;
        }

        frame.returnValue = kSceErrorDeadlock;
        return;
    }

    mutex->condition.wait(
        lock,
        [&] { return !mutex->held; });

    mutex->held = true;
    mutex->owner = current;
    mutex->recursion = 1;
    frame.returnValue = kSceOk;
}

void MutexTrylock(HleCallFrame& frame,
                  MutexState& state) {
    const auto mutex =
        ResolveMutex(
            frame,
            state,
            frame.arguments[0],
            true);

    if (!mutex) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    const auto current = std::this_thread::get_id();
    std::scoped_lock lock(mutex->lock);

    if (mutex->held) {
        if (mutex->owner == current &&
            mutex->type == MutexType::Recursive) {
            ++mutex->recursion;
            frame.returnValue = kSceOk;
            return;
        }

        frame.returnValue = kSceErrorBusy;
        return;
    }

    mutex->held = true;
    mutex->owner = current;
    mutex->recursion = 1;
    frame.returnValue = kSceOk;
}

void MutexUnlock(HleCallFrame& frame,
                 MutexState& state) {
    const auto mutex =
        ResolveMutex(
            frame,
            state,
            frame.arguments[0],
            false);

    if (!mutex) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    const auto current = std::this_thread::get_id();
    std::unique_lock lock(mutex->lock);

    if (!mutex->held ||
        mutex->owner != current) {
        frame.returnValue = kSceErrorPerm;
        return;
    }

    if (mutex->type == MutexType::Recursive &&
        mutex->recursion > 1) {
        --mutex->recursion;
        frame.returnValue = kSceOk;
        return;
    }

    mutex->held = false;
    mutex->owner = {};
    mutex->recursion = 0;
    lock.unlock();
    mutex->condition.notify_one();

    frame.returnValue = kSceOk;
}

} // namespace

void KernelMutex::Register(
    HleRegistry& registry,
    std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Kernel mutex module name cannot be empty");
    }

    auto state =
        std::make_shared<MutexState>();
    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexattrInit",
        [state](HleCallFrame& frame) {
            AttrInit(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexattrDestroy",
        [state](HleCallFrame& frame) {
            AttrDestroy(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexattrSettype",
        [state](HleCallFrame& frame) {
            AttrSetType(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexattrSetprotocol",
        [state](HleCallFrame& frame) {
            AttrSetProtocol(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexInit",
        [state](HleCallFrame& frame) {
            MutexInit(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexDestroy",
        [state](HleCallFrame& frame) {
            MutexDestroy(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexLock",
        [state](HleCallFrame& frame) {
            MutexLock(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadMutexTrylock",
        [state](HleCallFrame& frame) {
            MutexTrylock(frame, *state);
        });
    registry.RegisterSymbol(
        std::move(module),
        "scePthreadMutexUnlock",
        [state](HleCallFrame& frame) {
            MutexUnlock(frame, *state);
        });
}

} // namespace ps5emu::hle
