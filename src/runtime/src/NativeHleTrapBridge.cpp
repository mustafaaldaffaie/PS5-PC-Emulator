#include <ps5emu/runtime/NativeHleTrapBridge.hpp>

#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <csignal>
#include <sys/syscall.h>
#include <ucontext.h>
#else
#error "NativeHleTrapBridge currently supports Windows and Linux only"
#endif

namespace ps5emu::runtime {

struct NativeHleTrapBridge::Scope::State {
    std::atomic<std::uint64_t> thunkBase{0};
    std::atomic<std::uint64_t> slotSize{0};
    std::atomic<std::uint64_t> thunkCount{0};
    std::atomic<std::uint64_t> escapeRip{0};
    std::atomic<std::uint64_t> capturedRip{0};
    std::atomic<std::uint64_t> capturedBreakpoint{0};
    std::vector<std::uint64_t> additionalBreakpointStorage;
    const std::uint64_t* additionalBreakpoints = nullptr;
    std::size_t additionalBreakpointCount = 0;
};

namespace {

using TrapState = NativeHleTrapBridge::Scope::State;

static_assert(
    std::atomic<std::uint64_t>::is_always_lock_free,
    "Native trap capture requires lock-free 64-bit atomics");
static_assert(
    std::atomic<TrapState*>::is_always_lock_free,
    "Native trap capture requires lock-free pointer atomics");

constexpr std::size_t kMaxActiveTraps = 64;
constexpr std::size_t kInvalidRegistration =
    std::numeric_limits<std::size_t>::max();

struct TrapRegistration {
    std::atomic<std::uint64_t> threadToken{0};
    std::atomic<TrapState*> state{nullptr};
};

std::array<TrapRegistration, kMaxActiveTraps>
    g_activeTraps{};

std::mutex g_installMutex;
std::size_t g_installUsers = 0;

bool MatchesThunk(
    TrapState& state,
    std::uint64_t breakpointAddress) noexcept {
    const auto base =
        state.thunkBase.load(
            std::memory_order_relaxed);
    const auto slotSize =
        state.slotSize.load(
            std::memory_order_relaxed);
    const auto count =
        state.thunkCount.load(
            std::memory_order_relaxed);

    if (slotSize == 0 ||
        count == 0 ||
        breakpointAddress < base) {
        return false;
    }

    const auto delta =
        breakpointAddress - base;

    return delta % slotSize == 0 &&
        delta / slotSize < count;
}

bool MatchesAdditionalBreakpoint(
    const TrapState& state,
    std::uint64_t breakpointAddress) noexcept {
    for (std::size_t index = 0;
         index < state.additionalBreakpointCount;
         ++index) {
        if (state.additionalBreakpoints[index] ==
            breakpointAddress) {
            return true;
        }
    }

    return false;
}

bool MatchesTrap(
    TrapState& state,
    std::uint64_t breakpointAddress) noexcept {
    return MatchesThunk(
               state,
               breakpointAddress) ||
        MatchesAdditionalBreakpoint(
               state,
               breakpointAddress);
}

std::uint64_t CurrentNativeThreadToken() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(
        GetCurrentThreadId());
#elif defined(__linux__)
    long result = 0;
    __asm__ volatile(
        "syscall"
        : "=a"(result)
        : "a"(SYS_gettid)
        : "rcx", "r11", "memory");

    return result > 0
        ? static_cast<std::uint64_t>(result)
        : 0;
#endif
}

TrapState* ActiveTrapForCurrentThread() noexcept {
    const auto token =
        CurrentNativeThreadToken();

    if (token == 0) {
        return nullptr;
    }

    for (auto& registration : g_activeTraps) {
        if (registration.threadToken.load(
                std::memory_order_acquire) != token) {
            continue;
        }

        return registration.state.load(
            std::memory_order_acquire);
    }

    return nullptr;
}

std::size_t RegisterTrap(
    TrapState* state) {
    const auto token =
        CurrentNativeThreadToken();

    if (token == 0) {
        throw std::runtime_error(
            "Failed to identify the host thread for native trap routing");
    }

    for (const auto& registration : g_activeTraps) {
        if (registration.threadToken.load(
                std::memory_order_acquire) == token) {
            throw std::runtime_error(
                "A native HLE trap is already armed on this host thread");
        }
    }

    for (std::size_t index = 0;
         index < g_activeTraps.size();
         ++index) {
        auto& registration =
            g_activeTraps[index];

        std::uint64_t expected = 0;
        if (!registration.threadToken.compare_exchange_strong(
                expected,
                token,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            continue;
        }

        registration.state.store(
            state,
            std::memory_order_release);
        return index;
    }

    throw std::runtime_error(
        "Native HLE trap registration capacity is exhausted");
}

void UnregisterTrap(
    std::size_t index,
    TrapState* state) noexcept {
    if (index == kInvalidRegistration ||
        index >= g_activeTraps.size()) {
        return;
    }

    auto& registration =
        g_activeTraps[index];

    auto* expected = state;
    static_cast<void>(
        registration.state.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel,
            std::memory_order_relaxed));

    registration.threadToken.store(
        0,
        std::memory_order_release);
}

#if defined(_WIN32)

void* g_vectoredHandler = nullptr;

LONG CALLBACK VectoredTrapHandler(
    EXCEPTION_POINTERS* exception) noexcept {
    if (exception == nullptr ||
        exception->ExceptionRecord == nullptr ||
        exception->ContextRecord == nullptr ||
        exception->ExceptionRecord->ExceptionCode !=
            EXCEPTION_BREAKPOINT) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    auto* state =
        ActiveTrapForCurrentThread();

    if (state == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto breakpointAddress =
        reinterpret_cast<std::uintptr_t>(
            exception->ExceptionRecord->ExceptionAddress);

    if (!MatchesTrap(
            *state,
            breakpointAddress)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (breakpointAddress ==
        std::numeric_limits<std::uint64_t>::max()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto guestRip =
        static_cast<std::uint64_t>(
            breakpointAddress) + 1;

    state->capturedRip.store(
        guestRip,
        std::memory_order_relaxed);
    state->capturedBreakpoint.store(
        breakpointAddress,
        std::memory_order_relaxed);

    exception->ContextRecord->Rip =
        state->escapeRip.load(
            std::memory_order_relaxed);

    return EXCEPTION_CONTINUE_EXECUTION;
}

void InstallPlatformHandler() {
    g_vectoredHandler =
        AddVectoredExceptionHandler(
            1,
            VectoredTrapHandler);

    if (g_vectoredHandler == nullptr) {
        throw std::runtime_error(
            "Failed to install the Windows HLE breakpoint handler");
    }
}

void UninstallPlatformHandler() noexcept {
    if (g_vectoredHandler == nullptr) {
        return;
    }

    static_cast<void>(
        RemoveVectoredExceptionHandler(
            g_vectoredHandler));

    g_vectoredHandler = nullptr;
}

#elif defined(__linux__)

struct sigaction g_previousTrapAction {};

void ForwardLinuxTrap(
    int signalNumber,
    siginfo_t* information,
    void* nativeContext) noexcept {
    if (g_previousTrapAction.sa_handler == SIG_IGN) {
        return;
    }

    if (g_previousTrapAction.sa_handler == SIG_DFL) {
        static_cast<void>(
            ::signal(
                signalNumber,
                SIG_DFL));
        static_cast<void>(
            ::raise(signalNumber));
        return;
    }

    if ((g_previousTrapAction.sa_flags & SA_SIGINFO) != 0) {
        if (g_previousTrapAction.sa_sigaction != nullptr) {
            g_previousTrapAction.sa_sigaction(
                signalNumber,
                information,
                nativeContext);
        }
        return;
    }

    if (g_previousTrapAction.sa_handler != nullptr) {
        g_previousTrapAction.sa_handler(
            signalNumber);
    }
}

void LinuxTrapHandler(
    int signalNumber,
    siginfo_t* information,
    void* nativeContext) noexcept {
    if (signalNumber != SIGTRAP ||
        nativeContext == nullptr) {
        ForwardLinuxTrap(
            signalNumber,
            information,
            nativeContext);
        return;
    }

    auto* state =
        ActiveTrapForCurrentThread();

    if (state == nullptr) {
        ForwardLinuxTrap(
            signalNumber,
            information,
            nativeContext);
        return;
    }

    auto* context =
        static_cast<ucontext_t*>(
            nativeContext);

    const auto guestRip =
        static_cast<std::uint64_t>(
            context->uc_mcontext.gregs[REG_RIP]);

    if (guestRip == 0) {
        ForwardLinuxTrap(
            signalNumber,
            information,
            nativeContext);
        return;
    }

    const auto breakpointAddress =
        guestRip - 1;

    if (!MatchesTrap(
            *state,
            breakpointAddress)) {
        ForwardLinuxTrap(
            signalNumber,
            information,
            nativeContext);
        return;
    }

    state->capturedRip.store(
        guestRip,
        std::memory_order_relaxed);
    state->capturedBreakpoint.store(
        breakpointAddress,
        std::memory_order_relaxed);

    context->uc_mcontext.gregs[REG_RIP] =
        static_cast<greg_t>(
            state->escapeRip.load(
                std::memory_order_relaxed));
}

void InstallPlatformHandler() {
    struct sigaction action {};
    action.sa_sigaction =
        LinuxTrapHandler;
    action.sa_flags =
        SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    if (sigaction(
            SIGTRAP,
            &action,
            &g_previousTrapAction) != 0) {
        throw std::runtime_error(
            "Failed to install the Linux HLE SIGTRAP handler");
    }
}

void UninstallPlatformHandler() noexcept {
    static_cast<void>(
        sigaction(
            SIGTRAP,
            &g_previousTrapAction,
            nullptr));
}

#endif

void AcquirePlatformHandler() {
    std::lock_guard lock(
        g_installMutex);

    if (g_installUsers == 0) {
        InstallPlatformHandler();
    }

    ++g_installUsers;
}

void ReleasePlatformHandler() noexcept {
    std::lock_guard lock(
        g_installMutex);

    if (g_installUsers == 0) {
        return;
    }

    --g_installUsers;

    if (g_installUsers == 0) {
        UninstallPlatformHandler();
    }
}

} // namespace

NativeHleTrapBridge::Scope::Scope(
    std::uint64_t thunkBase,
    std::size_t slotSize,
    std::size_t thunkCount,
    std::uint64_t escapeRip,
    std::span<const std::uint64_t> additionalBreakpoints) {
    if (thunkCount != 0 &&
        slotSize == 0) {
        throw std::invalid_argument(
            "Native HLE trap slot size cannot be zero");
    }

    if (thunkCount == 0 &&
        additionalBreakpoints.empty()) {
        throw std::invalid_argument(
            "Native trap scope must contain at least one trap");
    }

    if (escapeRip == 0) {
        throw std::invalid_argument(
            "Native HLE trap escape RIP cannot be zero");
    }

    if (slotSize >
        std::numeric_limits<std::uint64_t>::max() ||
        thunkCount >
        std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error(
            "Native HLE trap geometry exceeds x86-64 address width");
    }

    const auto slotSize64 =
        static_cast<std::uint64_t>(
            slotSize);
    const auto count64 =
        static_cast<std::uint64_t>(
            thunkCount);

    if (count64 != 0 &&
        count64 >
            std::numeric_limits<std::uint64_t>::max() /
                slotSize64) {
        throw std::overflow_error(
            "Native HLE trap arena size overflows");
    }

    const auto arenaSize =
        count64 * slotSize64;

    if (thunkBase >
        std::numeric_limits<std::uint64_t>::max() -
            arenaSize) {
        throw std::overflow_error(
            "Native HLE trap arena address range overflows");
    }

    state_ = new State;
    state_->additionalBreakpointStorage.assign(
        additionalBreakpoints.begin(),
        additionalBreakpoints.end());
    state_->additionalBreakpoints =
        state_->additionalBreakpointStorage.data();
    state_->additionalBreakpointCount =
        state_->additionalBreakpointStorage.size();
    state_->thunkBase.store(
        thunkBase,
        std::memory_order_relaxed);
    state_->slotSize.store(
        slotSize64,
        std::memory_order_relaxed);
    state_->thunkCount.store(
        count64,
        std::memory_order_relaxed);
    state_->escapeRip.store(
        escapeRip,
        std::memory_order_relaxed);

    try {
        registrationIndex_ =
            RegisterTrap(state_);
    } catch (...) {
        delete state_;
        state_ = nullptr;
        throw;
    }
}

NativeHleTrapBridge::Scope::~Scope() {
    if (state_ == nullptr) {
        return;
    }

    UnregisterTrap(
        registrationIndex_,
        state_);

    registrationIndex_ =
        kInvalidRegistration;

    delete state_;
    state_ = nullptr;
}

bool NativeHleTrapBridge::Scope::Captured() const noexcept {
    return state_ != nullptr &&
        state_->capturedRip.load(
            std::memory_order_relaxed) != 0;
}

std::uint64_t
NativeHleTrapBridge::Scope::CapturedRip() const noexcept {
    if (state_ == nullptr) {
        return 0;
    }

    return state_->capturedRip.load(
        std::memory_order_relaxed);
}

std::uint64_t
NativeHleTrapBridge::Scope::CapturedBreakpointAddress() const noexcept {
    if (state_ == nullptr) {
        return 0;
    }

    return state_->capturedBreakpoint.load(
        std::memory_order_relaxed);
}

NativeHleTrapBridge::NativeHleTrapBridge() {
    AcquirePlatformHandler();
}

NativeHleTrapBridge::~NativeHleTrapBridge() {
    ReleasePlatformHandler();
}

NativeHleTrapBridge::Scope
NativeHleTrapBridge::Arm(
    std::uint64_t thunkBase,
    std::size_t slotSize,
    std::size_t thunkCount,
    std::uint64_t escapeRip,
    std::span<const std::uint64_t> additionalBreakpoints) {
    return Scope(
        thunkBase,
        slotSize,
        thunkCount,
        escapeRip,
        additionalBreakpoints);
}

} // namespace ps5emu::runtime
