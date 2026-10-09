#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/hle/GuestThreadAccess.hpp>
#include <ps5emu/hle/HleRegistry.hpp>
#include <ps5emu/runtime/HleThunkTable.hpp>
#include <ps5emu/runtime/NativeHleExecutor.hpp>
#include <ps5emu/runtime/NativeImageMaterializer.hpp>
#include <ps5emu/runtime/NativeSyscallInterceptor.hpp>

namespace ps5emu::runtime {

struct NativeGuestThreadRuntimeOptions {
    std::uint64_t workerStackBase = 0x0000201000000000ull;
    std::uint64_t workerStackStride = 0x01000000ull;
    std::size_t workerStackSize = 8u * 1024u * 1024u;

    std::uint64_t workerTlsBase = 0x0000202000000000ull;
    std::uint64_t workerTlsStride = 0x00100000ull;

    std::uint64_t stackGuard = 0;

    std::uint64_t handleBase = 0x00007ffa00000000ull;
    std::uint64_t handleStride = 0x100ull;
};

class NativeGuestThreadRuntime final
    : public hle::GuestThreadAccess {
public:
    NativeGuestThreadRuntime(
        std::span<const std::byte> executableBytes,
        elf::Image image,
        NativeImage& nativeImage,
        const hle::HleRegistry& registry,
        const HleThunkTable& thunks,
        std::span<const NativeSyscallTrap> syscallTraps = {},
        NativeGuestThreadRuntimeOptions options = {},
        hle::GuestFileSystemAccess* files = nullptr);

    NativeGuestThreadRuntime(
        const NativeGuestThreadRuntime&) = delete;
    NativeGuestThreadRuntime& operator=(
        const NativeGuestThreadRuntime&) = delete;
    NativeGuestThreadRuntime(
        NativeGuestThreadRuntime&&) = delete;
    NativeGuestThreadRuntime& operator=(
        NativeGuestThreadRuntime&&) = delete;

    ~NativeGuestThreadRuntime() override;

    [[nodiscard]] hle::GuestThreadCreateResult
    Create(
        const hle::GuestThreadCreateRequest& request) override;

    [[nodiscard]] hle::GuestThreadJoinResult
    Join(std::uint64_t handle) override;

    [[nodiscard]] std::uint64_t
    CurrentThreadHandle() const noexcept override;

    [[nodiscard]] std::uint64_t
    MainThreadHandle() const noexcept;

private:
    struct ThreadRecord;

    [[nodiscard]] std::uint64_t
    AddressForOrdinal(
        std::uint64_t base,
        std::uint64_t stride,
        std::uint64_t ordinal) const;

    [[nodiscard]] std::uint64_t
    HandleForOrdinal(
        std::uint64_t ordinal) const;

    void JoinRemainingThreads() noexcept;

    std::vector<std::byte> executableBytes_;
    elf::Image image_;
    NativeImage& nativeImage_;
    const hle::HleRegistry& registry_;
    const HleThunkTable& thunks_;
    std::vector<NativeSyscallTrap> syscallTraps_;
    NativeGuestThreadRuntimeOptions options_;
    hle::GuestFileSystemAccess* files_ = nullptr;

    mutable std::mutex mutex_;
    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<ThreadRecord>>
        threads_;

    std::uint64_t nextOrdinal_ = 1;
    std::uint64_t mainThreadHandle_ = 0;

    const NativeGuestThreadRuntime*
        previousRuntime_ = nullptr;
    std::uint64_t previousHandle_ = 0;
};

} // namespace ps5emu::runtime
