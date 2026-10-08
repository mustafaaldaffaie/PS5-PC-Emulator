#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ps5emu::runtime {

// Redirects native INT3 exceptions from known HLE thunk slots to a caller
// supplied escape address. Platform handlers only perform lock-free state
// capture and native-context redirection; HLE dispatch runs later in ordinary
// C++ execution context. One scope may be armed per host thread. Linux trap
// routing identifies the host thread with a raw gettid syscall so the signal
// handler never touches host TLS while FS temporarily points at guest TLS.
class NativeHleTrapBridge final {
public:
    class Scope final {
    public:
        struct State;

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;

        ~Scope();

        [[nodiscard]] bool Captured() const noexcept;
        [[nodiscard]] std::uint64_t CapturedRip() const noexcept;
        [[nodiscard]] std::uint64_t CapturedBreakpointAddress() const noexcept;

    private:
        friend class NativeHleTrapBridge;

        Scope(std::uint64_t thunkBase,
              std::size_t slotSize,
              std::size_t thunkCount,
              std::uint64_t escapeRip,
              std::span<const std::uint64_t> additionalBreakpoints);

        State* state_ = nullptr;
        std::size_t registrationIndex_ =
            static_cast<std::size_t>(-1);
    };

    NativeHleTrapBridge();

    NativeHleTrapBridge(const NativeHleTrapBridge&) = delete;
    NativeHleTrapBridge& operator=(const NativeHleTrapBridge&) = delete;
    NativeHleTrapBridge(NativeHleTrapBridge&&) = delete;
    NativeHleTrapBridge& operator=(NativeHleTrapBridge&&) = delete;

    ~NativeHleTrapBridge();

    [[nodiscard]] Scope
    Arm(std::uint64_t thunkBase,
        std::size_t slotSize,
        std::size_t thunkCount,
        std::uint64_t escapeRip,
        std::span<const std::uint64_t> additionalBreakpoints = {});
};

} // namespace ps5emu::runtime
