#pragma once

#include <cstddef>
#include <cstdint>

namespace ps5emu::runtime {

// Redirects native INT3 exceptions from known HLE thunk slots to a caller
// supplied escape address. Platform handlers only perform lock-free state
// capture and native-context redirection; HLE dispatch runs later in ordinary
// C++ execution context.
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

    private:
        friend class NativeHleTrapBridge;

        Scope(std::uint64_t thunkBase,
              std::size_t slotSize,
              std::size_t thunkCount,
              std::uint64_t escapeRip);

        State* state_ = nullptr;
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
        std::uint64_t escapeRip);
};

} // namespace ps5emu::runtime
