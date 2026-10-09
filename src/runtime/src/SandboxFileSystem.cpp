#include <ps5emu/runtime/SandboxFileSystem.hpp>

#include <cerrno>
#include <climits>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#else
#error "SandboxFileSystem currently supports Windows and Linux only"
#endif

namespace ps5emu::runtime {
namespace {

constexpr std::int32_t kGuestReadOnly = 0x00000000;
constexpr std::int32_t kGuestWriteOnly = 0x00000001;
constexpr std::int32_t kGuestReadWrite = 0x00000002;
constexpr std::int32_t kGuestAccessMode = 0x00000003;
constexpr std::int32_t kGuestNonBlock = 0x00000004;
constexpr std::int32_t kGuestAppend = 0x00000008;
constexpr std::int32_t kGuestSync = 0x00000080;
constexpr std::int32_t kGuestCreate = 0x00000200;
constexpr std::int32_t kGuestTruncate = 0x00000400;
constexpr std::int32_t kGuestExclusive = 0x00000800;
constexpr std::int32_t kGuestDirect = 0x00010000;
constexpr std::int32_t kGuestDirectory = 0x00020000;

constexpr std::int32_t kSupportedFlags =
    kGuestAccessMode |
    kGuestNonBlock |
    kGuestAppend |
    kGuestSync |
    kGuestCreate |
    kGuestTruncate |
    kGuestExclusive;

constexpr int kGuestIo = 5;
constexpr int kGuestBadFileDescriptor = 9;
constexpr int kGuestAccess = 13;
constexpr int kGuestInvalid = 22;
constexpr int kGuestTooManyFiles = 24;

bool IsWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    auto rootIt = root.begin();
    auto candidateIt = candidate.begin();

    for (; rootIt != root.end();
         ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() ||
            *candidateIt != *rootIt) {
            return false;
        }
    }

    return true;
}

std::filesystem::path CanonicalExisting(
    const std::filesystem::path& path,
    std::error_code& error) {
    auto result =
        std::filesystem::weakly_canonical(
            path,
            error);

    if (error) {
        return {};
    }

    return result;
}

} // namespace

SandboxFileSystem::SandboxFileSystem(
    std::filesystem::path root) {
    if (root.empty()) {
        throw std::invalid_argument(
            "Sandbox filesystem root cannot be empty");
    }

    std::error_code error;
    std::filesystem::create_directories(
        root,
        error);

    if (error) {
        throw std::runtime_error(
            "Failed to create sandbox filesystem root");
    }

    root_ =
        std::filesystem::weakly_canonical(
            std::filesystem::absolute(root),
            error);

    if (error ||
        !std::filesystem::is_directory(
            root_,
            error) ||
        error) {
        throw std::runtime_error(
            "Sandbox filesystem root is not a directory");
    }
}

SandboxFileSystem::~SandboxFileSystem() {
    std::lock_guard lock(mutex_);

    for (const auto& [guest, native] :
         descriptors_) {
        static_cast<void>(guest);
        NativeCloseNoThrow(native);
    }

    descriptors_.clear();
}

hle::GuestFileResult
SandboxFileSystem::Open(
    std::string_view path,
    std::int32_t flags,
    std::uint16_t mode) {
    if ((flags & ~kSupportedFlags) != 0 ||
        (flags & (kGuestDirect |
                  kGuestDirectory)) != 0) {
        return Failure(kGuestInvalid);
    }

    const bool mayCreate =
        (flags & kGuestCreate) != 0;

    const auto resolved =
        ResolvePath(
            path,
            mayCreate);

    if (resolved.errorCode != 0) {
        return {
            .errorCode = resolved.errorCode,
        };
    }

    int nativeFlags = 0;

    try {
        nativeFlags =
            MapOpenFlags(flags);
    } catch (const std::invalid_argument&) {
        return Failure(kGuestInvalid);
    }

    errno = 0;

#if defined(_WIN32)
    const auto nativeDescriptor =
        ::_wopen(
            resolved.nativePath.wstring().c_str(),
            nativeFlags,
            static_cast<int>(mode));
#else
    const auto nativeDescriptor =
        ::open(
            resolved.nativePath.c_str(),
            nativeFlags,
            static_cast<mode_t>(mode));
#endif

    if (nativeDescriptor < 0) {
        return Failure(errno);
    }

    std::int32_t guestDescriptor = -1;

    {
        std::lock_guard lock(mutex_);

        guestDescriptor =
            AllocateDescriptorLocked();

        if (guestDescriptor < 0) {
            NativeCloseNoThrow(
                nativeDescriptor);
            return Failure(
                kGuestTooManyFiles);
        }

        descriptors_.emplace(
            guestDescriptor,
            nativeDescriptor);
    }

    return {
        .value = guestDescriptor,
    };
}

hle::GuestFileResult
SandboxFileSystem::Close(
    std::int32_t descriptor) {
    int native = -1;

    {
        std::lock_guard lock(mutex_);

        const auto found =
            descriptors_.find(
                descriptor);

        if (found == descriptors_.end()) {
            return Failure(
                kGuestBadFileDescriptor);
        }

        native = found->second;
        descriptors_.erase(found);
    }

    errno = 0;

#if defined(_WIN32)
    const auto result =
        ::_close(native);
#else
    const auto result =
        ::close(native);
#endif

    if (result != 0) {
        return Failure(errno);
    }

    return {};
}

hle::GuestFileResult
SandboxFileSystem::Read(
    std::int32_t descriptor,
    std::span<std::byte> output) {
    std::lock_guard lock(mutex_);

    const auto found =
        descriptors_.find(descriptor);

    if (found == descriptors_.end()) {
        return Failure(
            kGuestBadFileDescriptor);
    }

#if defined(_WIN32)
    if (output.size() >
        std::numeric_limits<unsigned int>::max()) {
        return Failure(kGuestInvalid);
    }

    errno = 0;
    const auto count =
        ::_read(
            found->second,
            output.data(),
            static_cast<unsigned int>(
                output.size()));
#else
    errno = 0;
    const auto count =
        ::read(
            found->second,
            output.data(),
            output.size());
#endif

    if (count < 0) {
        return Failure(errno);
    }

    return {
        .value =
            static_cast<std::int64_t>(
                count),
    };
}

hle::GuestFileResult
SandboxFileSystem::Write(
    std::int32_t descriptor,
    std::span<const std::byte> input) {
    std::lock_guard lock(mutex_);

    const auto found =
        descriptors_.find(descriptor);

    if (found == descriptors_.end()) {
        return Failure(
            kGuestBadFileDescriptor);
    }

#if defined(_WIN32)
    if (input.size() >
        std::numeric_limits<unsigned int>::max()) {
        return Failure(kGuestInvalid);
    }

    errno = 0;
    const auto count =
        ::_write(
            found->second,
            input.data(),
            static_cast<unsigned int>(
                input.size()));
#else
    errno = 0;
    const auto count =
        ::write(
            found->second,
            input.data(),
            input.size());
#endif

    if (count < 0) {
        return Failure(errno);
    }

    return {
        .value =
            static_cast<std::int64_t>(
                count),
    };
}

hle::GuestFileResult
SandboxFileSystem::Seek(
    std::int32_t descriptor,
    std::int64_t offset,
    std::int32_t whence) {
    if (whence < 0 || whence > 2) {
        return Failure(kGuestInvalid);
    }

    std::lock_guard lock(mutex_);

    const auto found =
        descriptors_.find(descriptor);

    if (found == descriptors_.end()) {
        return Failure(
            kGuestBadFileDescriptor);
    }

    errno = 0;

#if defined(_WIN32)
    const auto result =
        ::_lseeki64(
            found->second,
            offset,
            whence);
#else
    const auto result =
        ::lseek(
            found->second,
            static_cast<off_t>(offset),
            whence);
#endif

    if (result < 0) {
        return Failure(errno);
    }

    return {
        .value =
            static_cast<std::int64_t>(
                result),
    };
}

const std::filesystem::path&
SandboxFileSystem::Root() const noexcept {
    return root_;
}

SandboxFileSystem::ResolvedPath
SandboxFileSystem::ResolvePath(
    std::string_view guestPath,
    bool allowMissingLeaf) const {
    if (guestPath.empty()) {
        return {
            .errorCode =
                ErrorFromErrno(kGuestInvalid),
        };
    }

    if (guestPath.find('\0') !=
        std::string_view::npos ||
        guestPath.find('\\') !=
        std::string_view::npos ||
        guestPath.find(':') !=
        std::string_view::npos) {
        return {
            .errorCode =
                ErrorFromErrno(kGuestAccess),
        };
    }

    std::filesystem::path relative;

    std::size_t cursor = 0;
    while (cursor < guestPath.size()) {
        while (cursor < guestPath.size() &&
               guestPath[cursor] == '/') {
            ++cursor;
        }

        const auto start = cursor;

        while (cursor < guestPath.size() &&
               guestPath[cursor] != '/') {
            ++cursor;
        }

        if (start == cursor) {
            continue;
        }

        const auto component =
            guestPath.substr(
                start,
                cursor - start);

        if (component == ".") {
            continue;
        }

        if (component == "..") {
            return {
                .errorCode =
                    ErrorFromErrno(
                        kGuestAccess),
            };
        }

        relative /=
            std::string(component);
    }

    auto candidate =
        root_ / relative;

    std::error_code error;

    const auto parent =
        candidate.has_parent_path()
            ? candidate.parent_path()
            : root_;

    const auto canonicalParent =
        CanonicalExisting(
            parent,
            error);

    if (error ||
        canonicalParent.empty() ||
        !IsWithin(
            root_,
            canonicalParent)) {
        return {
            .errorCode =
                ErrorFromErrno(
                    kGuestAccess),
        };
    }

    if (!allowMissingLeaf ||
        std::filesystem::exists(
            candidate,
            error)) {
        error.clear();

        const auto canonicalCandidate =
            CanonicalExisting(
                candidate,
                error);

        if (error ||
            canonicalCandidate.empty() ||
            !IsWithin(
                root_,
                canonicalCandidate)) {
            return {
                .errorCode =
                    ErrorFromErrno(
                        kGuestAccess),
            };
        }

        candidate =
            canonicalCandidate;
    } else {
        candidate =
            canonicalParent /
            candidate.filename();
    }

    return {
        .nativePath =
            std::move(candidate),
    };
}

std::int32_t
SandboxFileSystem::AllocateDescriptorLocked() {
    constexpr auto maximum =
        std::numeric_limits<std::int32_t>::max();

    for (std::int64_t attempts = 0;
         attempts <
             static_cast<std::int64_t>(
                 maximum) - 2;
         ++attempts) {
        if (nextDescriptor_ < 3) {
            nextDescriptor_ = 3;
        }

        const auto candidate =
            nextDescriptor_;

        if (nextDescriptor_ == maximum) {
            nextDescriptor_ = 3;
        } else {
            ++nextDescriptor_;
        }

        if (!descriptors_.contains(
                candidate)) {
            return candidate;
        }
    }

    return -1;
}

std::uint64_t
SandboxFileSystem::ErrorFromErrno(
    int error) noexcept {
    const auto guest =
        error > 0 && error <= 34
            ? error
            : kGuestIo;

    return 0x80020000ull |
        static_cast<std::uint64_t>(
            guest);
}

hle::GuestFileResult
SandboxFileSystem::Failure(
    int error) noexcept {
    return {
        .errorCode =
            ErrorFromErrno(error),
    };
}

int SandboxFileSystem::MapOpenFlags(
    std::int32_t guestFlags) {
    int result = 0;

    switch (guestFlags &
            kGuestAccessMode) {
    case kGuestReadOnly:
#if defined(_WIN32)
        result |= _O_RDONLY;
#else
        result |= O_RDONLY;
#endif
        break;

    case kGuestWriteOnly:
#if defined(_WIN32)
        result |= _O_WRONLY;
#else
        result |= O_WRONLY;
#endif
        break;

    case kGuestReadWrite:
#if defined(_WIN32)
        result |= _O_RDWR;
#else
        result |= O_RDWR;
#endif
        break;

    default:
        throw std::invalid_argument(
            "Invalid guest file access mode");
    }

#if defined(_WIN32)
    result |= _O_BINARY;

    if ((guestFlags & kGuestAppend) != 0) {
        result |= _O_APPEND;
    }
    if ((guestFlags & kGuestCreate) != 0) {
        result |= _O_CREAT;
    }
    if ((guestFlags & kGuestTruncate) != 0) {
        result |= _O_TRUNC;
    }
    if ((guestFlags & kGuestExclusive) != 0) {
        result |= _O_EXCL;
    }
#else
    if ((guestFlags & kGuestNonBlock) != 0) {
        result |= O_NONBLOCK;
    }
    if ((guestFlags & kGuestAppend) != 0) {
        result |= O_APPEND;
    }
    if ((guestFlags & kGuestSync) != 0) {
        result |= O_SYNC;
    }
    if ((guestFlags & kGuestCreate) != 0) {
        result |= O_CREAT;
    }
    if ((guestFlags & kGuestTruncate) != 0) {
        result |= O_TRUNC;
    }
    if ((guestFlags & kGuestExclusive) != 0) {
        result |= O_EXCL;
    }
#endif

    return result;
}

void SandboxFileSystem::NativeCloseNoThrow(
    int descriptor) noexcept {
#if defined(_WIN32)
    static_cast<void>(
        ::_close(descriptor));
#else
    static_cast<void>(
        ::close(descriptor));
#endif
}

} // namespace ps5emu::runtime
