#include <ps5emu/hle/KernelMutex.hpp>

#include <array>
#include <chrono>
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
constexpr std::uint64_t kSceErrorTimedOut = 0x8002003cull;

constexpr std::uint64_t kStaticAdaptiveMutex = 1;
constexpr std::uint64_t kDestroyedCondition = 2;
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

struct ConditionAttribute {
    std::int32_t clockId = 0;
};

struct ConditionObject {
    explicit ConditionObject(std::int32_t value)
        : clockId(value) {
    }

    std::int32_t clockId = 0;
    std::mutex lock;
    std::condition_variable condition;
    std::size_t waiters = 0;
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
    std::unordered_map<std::uint64_t, ConditionAttribute> conditionAttributes;
    std::unordered_map<std::uint64_t, std::shared_ptr<ConditionObject>> conditions;
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



std::shared_ptr<ConditionObject> ResolveCondition(
    HleCallFrame& frame,
    MutexState& state,
    std::uint64_t pointerAddress,
    bool initializeStatic) {
    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointerAddress, handle)) {
        return {};
    }

    std::scoped_lock stateLock(state.lock);

    if (handle == kDestroyedCondition) {
        return {};
    }

    if (handle == 0 && initializeStatic) {
        handle = state.AllocateHandle();
        state.conditions.emplace(
            handle,
            std::make_shared<ConditionObject>(0));

        if (!WriteHandle(frame, pointerAddress, handle)) {
            state.conditions.erase(handle);
            return {};
        }
    }

    const auto found = state.conditions.find(handle);
    return found == state.conditions.end()
        ? std::shared_ptr<ConditionObject>{}
        : found->second;
}

void CondAttrInit(HleCallFrame& frame,
                  MutexState& state) {
    const auto output = frame.arguments[0];
    if (output == 0 || frame.memory == nullptr) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::scoped_lock lock(state.lock);
    const auto handle = state.AllocateHandle();
    state.conditionAttributes.emplace(
        handle,
        ConditionAttribute{});

    if (!WriteHandle(frame, output, handle)) {
        state.conditionAttributes.erase(handle);
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void CondAttrDestroy(HleCallFrame& frame,
                     MutexState& state) {
    const auto pointer = frame.arguments[0];
    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    {
        std::scoped_lock lock(state.lock);
        if (state.conditionAttributes.erase(handle) == 0) {
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

void CondAttrSetClock(HleCallFrame& frame,
                      MutexState& state) {
    const auto pointer = frame.arguments[0];
    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::scoped_lock lock(state.lock);
    const auto found =
        state.conditionAttributes.find(handle);
    if (found == state.conditionAttributes.end()) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    found->second.clockId =
        static_cast<std::int32_t>(frame.arguments[1]);
    frame.returnValue = kSceOk;
}

void CondInit(HleCallFrame& frame,
              MutexState& state) {
    const auto pointer = frame.arguments[0];
    const auto attrPointer = frame.arguments[1];

    if (pointer == 0 || frame.memory == nullptr) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    std::int32_t clockId = 0;

    std::scoped_lock lock(state.lock);

    if (attrPointer != 0) {
        std::uint64_t attrHandle = 0;
        if (!ReadHandle(frame, attrPointer, attrHandle)) {
            frame.returnValue = kSceErrorFault;
            return;
        }

        const auto found =
            state.conditionAttributes.find(attrHandle);
        if (found == state.conditionAttributes.end()) {
            frame.returnValue = kSceErrorInvalid;
            return;
        }

        clockId = found->second.clockId;
    }

    const auto handle = state.AllocateHandle();
    state.conditions.emplace(
        handle,
        std::make_shared<ConditionObject>(clockId));

    if (!WriteHandle(frame, pointer, handle)) {
        state.conditions.erase(handle);
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void CondDestroy(HleCallFrame& frame,
                 MutexState& state) {
    const auto pointer = frame.arguments[0];
    std::uint64_t handle = 0;
    if (!ReadHandle(frame, pointer, handle)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    if (handle == kDestroyedCondition) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    if (handle == 0) {
        if (!WriteHandle(
                frame,
                pointer,
                kDestroyedCondition)) {
            frame.returnValue = kSceErrorFault;
            return;
        }

        frame.returnValue = kSceOk;
        return;
    }

    std::shared_ptr<ConditionObject> condition;
    {
        std::scoped_lock lock(state.lock);
        const auto found = state.conditions.find(handle);
        if (found == state.conditions.end()) {
            frame.returnValue = kSceErrorInvalid;
            return;
        }
        condition = found->second;
    }

    {
        std::scoped_lock lock(condition->lock);
        if (condition->waiters != 0) {
            frame.returnValue = kSceErrorBusy;
            return;
        }
    }

    {
        std::scoped_lock lock(state.lock);
        state.conditions.erase(handle);
    }

    if (!WriteHandle(
            frame,
            pointer,
            kDestroyedCondition)) {
        frame.returnValue = kSceErrorFault;
        return;
    }

    frame.returnValue = kSceOk;
}

void CondSignal(HleCallFrame& frame,
                MutexState& state,
                bool broadcast) {
    const auto condition =
        ResolveCondition(
            frame,
            state,
            frame.arguments[0],
            true);

    if (!condition) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    std::scoped_lock lock(condition->lock);

    if (broadcast) {
        condition->condition.notify_all();
    } else {
        condition->condition.notify_one();
    }

    frame.returnValue = kSceOk;
}

void ReacquireMutex(
    const std::shared_ptr<MutexObject>& mutex,
    std::thread::id owner,
    std::uint32_t recursion) {
    std::unique_lock lock(mutex->lock);
    mutex->condition.wait(
        lock,
        [&] { return !mutex->held; });

    mutex->held = true;
    mutex->owner = owner;
    mutex->recursion = recursion;
}

void CondWait(HleCallFrame& frame,
              MutexState& state,
              bool timed) {
    const auto condition =
        ResolveCondition(
            frame,
            state,
            frame.arguments[0],
            true);
    const auto mutex =
        ResolveMutex(
            frame,
            state,
            frame.arguments[1],
            true);

    if (!condition || !mutex) {
        frame.returnValue = kSceErrorInvalid;
        return;
    }

    const auto current =
        std::this_thread::get_id();

    std::unique_lock conditionLock(
        condition->lock);

    std::uint32_t savedRecursion = 0;
    {
        std::unique_lock mutexLock(mutex->lock);

        if (!mutex->held ||
            mutex->owner != current) {
            frame.returnValue = kSceErrorPerm;
            return;
        }

        savedRecursion = mutex->recursion;
        mutex->held = false;
        mutex->owner = {};
        mutex->recursion = 0;
    }

    mutex->condition.notify_one();

    ++condition->waiters;

    bool timedOut = false;

    if (timed) {
        const auto raw =
            frame.arguments[2];
        const auto maximum =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max());
        const auto bounded =
            static_cast<std::int64_t>(
                raw > maximum ? maximum : raw);

        timedOut =
            condition->condition.wait_for(
                conditionLock,
                std::chrono::microseconds(bounded)) ==
            std::cv_status::timeout;
    } else {
        condition->condition.wait(
            conditionLock);
    }

    --condition->waiters;
    conditionLock.unlock();

    ReacquireMutex(
        mutex,
        current,
        savedRecursion);

    frame.returnValue =
        timedOut
            ? kSceErrorTimedOut
            : kSceOk;
}


struct GuestTimespec {
    std::int64_t seconds = 0;
    std::int64_t nanoseconds = 0;
};

bool ReadTimespec(HleCallFrame& frame,
                  std::uint64_t address,
                  GuestTimespec& value) {
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

bool IsMonotonicClock(std::int32_t clockId) noexcept {
    switch (clockId) {
    case 4:
    case 5:
    case 7:
    case 8:
    case 11:
    case 12:
        return true;
    default:
        return false;
    }
}

bool IsRealtimeClock(std::int32_t clockId) noexcept {
    switch (clockId) {
    case 0:
    case 9:
    case 10:
    case 13:
        return true;
    default:
        return false;
    }
}

std::optional<std::uint64_t> AbsoluteTimeoutMicroseconds(
    std::int32_t clockId,
    const GuestTimespec& absolute) {
    if (absolute.seconds < 0 ||
        absolute.nanoseconds < 0 ||
        absolute.nanoseconds >= 1000000000ll) {
        return std::nullopt;
    }

    const auto seconds =
        static_cast<std::uint64_t>(absolute.seconds);
    const auto nanoseconds =
        static_cast<std::uint64_t>(absolute.nanoseconds);

    if (seconds >
        std::numeric_limits<std::uint64_t>::max() /
            1000000000ull) {
        return std::nullopt;
    }

    const auto base =
        seconds * 1000000000ull;

    if (base >
        std::numeric_limits<std::uint64_t>::max() -
            nanoseconds) {
        return std::nullopt;
    }

    const auto target = base + nanoseconds;

    std::uint64_t now = 0;

    if (IsMonotonicClock(clockId)) {
        const auto value =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now()
                    .time_since_epoch());
        if (value.count() < 0) {
            now = 0;
        } else {
            now = static_cast<std::uint64_t>(value.count());
        }
    } else if (IsRealtimeClock(clockId)) {
        const auto value =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now()
                    .time_since_epoch());
        if (value.count() < 0) {
            return std::nullopt;
        }
        now = static_cast<std::uint64_t>(value.count());
    } else {
        return std::nullopt;
    }

    if (target <= now) {
        return 0;
    }

    const auto remaining = target - now;
    return remaining / 1000ull +
        (remaining % 1000ull != 0 ? 1ull : 0ull);
}

std::uint64_t ToPosixError(std::uint64_t value) {
    if (value == 0) {
        return 0;
    }

    if ((value & 0xffff0000ull) == 0x80020000ull) {
        return value & 0xffffull;
    }

    return value;
}


void PosixCondTimedwait(HleCallFrame& frame,
                        MutexState& state) {
    const auto condition =
        ResolveCondition(
            frame,
            state,
            frame.arguments[0],
            true);

    if (!condition) {
        frame.returnValue = 22;
        return;
    }

    GuestTimespec absolute;
    if (!ReadTimespec(
            frame,
            frame.arguments[2],
            absolute)) {
        frame.returnValue = 14;
        return;
    }

    const auto timeout =
        AbsoluteTimeoutMicroseconds(
            condition->clockId,
            absolute);

    if (!timeout.has_value()) {
        frame.returnValue = 22;
        return;
    }

    const auto saved = frame.arguments[2];
    frame.arguments[2] = *timeout;
    CondWait(frame, state, true);
    frame.arguments[2] = saved;
    frame.returnValue =
        ToPosixError(frame.returnValue);
}

template <typename Function>
void InvokePosix(HleCallFrame& frame,
                 MutexState& state,
                 Function&& function) {
    function(frame, state);
    frame.returnValue =
        ToPosixError(frame.returnValue);
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
        moduleName,
        "scePthreadMutexUnlock",
        [state](HleCallFrame& frame) {
            MutexUnlock(frame, *state);
        });


    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondattrInit",
        [state](HleCallFrame& frame) {
            CondAttrInit(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondattrDestroy",
        [state](HleCallFrame& frame) {
            CondAttrDestroy(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondattrSetclock",
        [state](HleCallFrame& frame) {
            CondAttrSetClock(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondInit",
        [state](HleCallFrame& frame) {
            CondInit(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondDestroy",
        [state](HleCallFrame& frame) {
            CondDestroy(frame, *state);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondSignal",
        [state](HleCallFrame& frame) {
            CondSignal(frame, *state, false);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondBroadcast",
        [state](HleCallFrame& frame) {
            CondSignal(frame, *state, true);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondWait",
        [state](HleCallFrame& frame) {
            CondWait(frame, *state, false);
        });
    registry.RegisterSymbol(
        moduleName,
        "scePthreadCondTimedwait",
        [state](HleCallFrame& frame) {
            CondWait(frame, *state, true);
        });


    registry.RegisterSymbol(
        moduleName,
        "pthread_condattr_init",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, CondAttrInit);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_condattr_destroy",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, CondAttrDestroy);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_condattr_setclock",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, CondAttrSetClock);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_cond_init",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, CondInit);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_cond_destroy",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, CondDestroy);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_cond_signal",
        [state](HleCallFrame& frame) {
            CondSignal(frame, *state, false);
            frame.returnValue =
                ToPosixError(frame.returnValue);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_cond_broadcast",
        [state](HleCallFrame& frame) {
            CondSignal(frame, *state, true);
            frame.returnValue =
                ToPosixError(frame.returnValue);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_cond_wait",
        [state](HleCallFrame& frame) {
            CondWait(frame, *state, false);
            frame.returnValue =
                ToPosixError(frame.returnValue);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_cond_timedwait",
        [state](HleCallFrame& frame) {
            PosixCondTimedwait(frame, *state);
        });

    registry.RegisterSymbol(
        moduleName,
        "pthread_mutexattr_init",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, AttrInit);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutexattr_destroy",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, AttrDestroy);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutexattr_settype",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, AttrSetType);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutexattr_setprotocol",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, AttrSetProtocol);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutex_init",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, MutexInit);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutex_destroy",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, MutexDestroy);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutex_lock",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, MutexLock);
        });
    registry.RegisterSymbol(
        moduleName,
        "pthread_mutex_trylock",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, MutexTrylock);
        });
    registry.RegisterSymbol(
        std::move(module),
        "pthread_mutex_unlock",
        [state](HleCallFrame& frame) {
            InvokePosix(frame, *state, MutexUnlock);
        });
}

} // namespace ps5emu::hle
