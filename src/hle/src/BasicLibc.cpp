#include <ps5emu/hle/BasicLibc.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ps5emu::hle {
namespace {

std::size_t CheckedSize(std::uint64_t value) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "Guest libc memory operation size is too large for this host");
    }

    return static_cast<std::size_t>(value);
}

GuestMemoryAccess& RequireMemory(HleCallFrame& frame) {
    if (frame.memory == nullptr) {
        throw std::runtime_error(
            "Guest libc memory operation requires guest memory access");
    }

    return *frame.memory;
}

void CopyMemory(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto source = frame.arguments[1];
    const auto size = CheckedSize(frame.arguments[2]);

    frame.returnValue = destination;

    if (size == 0) {
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size);
    memory.Read(source, buffer);
    memory.Write(destination, buffer);
}

void SetMemory(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto value =
        static_cast<std::byte>(frame.arguments[1] & 0xffu);
    const auto size = CheckedSize(frame.arguments[2]);

    frame.returnValue = destination;

    if (size == 0) {
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size, value);
    memory.Write(destination, buffer);
}

std::byte ReadByte(GuestMemoryAccess& memory,
                   std::uint64_t address) {
    std::array<std::byte, 1> value{};
    memory.Read(address, value);
    return value[0];
}

std::uint64_t CheckedAddress(std::uint64_t base,
                             std::uint64_t offset) {
    if (offset >
        std::numeric_limits<std::uint64_t>::max() - base) {
        throw std::runtime_error(
            "Guest libc string address overflows");
    }

    return base + offset;
}

void CompareMemory(HleCallFrame& frame) {
    const auto left = frame.arguments[0];
    const auto right = frame.arguments[1];
    const auto size = CheckedSize(frame.arguments[2]);

    if (size == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);

    std::vector<std::byte> leftBytes(size);
    std::vector<std::byte> rightBytes(size);
    memory.Read(left, leftBytes);
    memory.Read(right, rightBytes);

    for (std::size_t index = 0; index < size; ++index) {
        const auto lhs =
            std::to_integer<unsigned int>(leftBytes[index]);
        const auto rhs =
            std::to_integer<unsigned int>(rightBytes[index]);

        if (lhs == rhs) {
            continue;
        }

        const std::int64_t result =
            lhs < rhs ? -1 : 1;
        frame.returnValue =
            static_cast<std::uint64_t>(result);
        return;
    }

    frame.returnValue = 0;
}


std::int64_t CompareByteValues(std::byte lhs,
                               std::byte rhs) {
    const auto left =
        std::to_integer<unsigned int>(lhs);
    const auto right =
        std::to_integer<unsigned int>(rhs);

    if (left == right) {
        return 0;
    }

    return left < right ? -1 : 1;
}

void FindMemoryByte(HleCallFrame& frame) {
    const auto address = frame.arguments[0];
    const auto value =
        static_cast<std::byte>(frame.arguments[1] & 0xffu);
    const auto size = CheckedSize(frame.arguments[2]);

    if (size == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size);
    memory.Read(address, buffer);

    for (std::size_t index = 0; index < buffer.size(); ++index) {
        if (buffer[index] == value) {
            frame.returnValue =
                CheckedAddress(
                    address,
                    static_cast<std::uint64_t>(index));
            return;
        }
    }

    frame.returnValue = 0;
}

void CompareStrings(HleCallFrame& frame) {
    const auto left = frame.arguments[0];
    const auto right = frame.arguments[1];
    auto& memory = RequireMemory(frame);

    for (std::uint64_t index = 0;; ++index) {
        const auto lhs =
            ReadByte(
                memory,
                CheckedAddress(left, index));
        const auto rhs =
            ReadByte(
                memory,
                CheckedAddress(right, index));

        const auto comparison =
            CompareByteValues(lhs, rhs);

        if (comparison != 0) {
            frame.returnValue =
                static_cast<std::uint64_t>(comparison);
            return;
        }

        if (lhs == std::byte{0}) {
            frame.returnValue = 0;
            return;
        }

        if (index ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc string comparison overflows");
        }
    }
}

void CompareStringsBounded(HleCallFrame& frame) {
    const auto left = frame.arguments[0];
    const auto right = frame.arguments[1];
    const auto maximum = frame.arguments[2];

    if (maximum == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);

    for (std::uint64_t index = 0;
         index < maximum;
         ++index) {
        const auto lhs =
            ReadByte(
                memory,
                CheckedAddress(left, index));
        const auto rhs =
            ReadByte(
                memory,
                CheckedAddress(right, index));

        const auto comparison =
            CompareByteValues(lhs, rhs);

        if (comparison != 0) {
            frame.returnValue =
                static_cast<std::uint64_t>(comparison);
            return;
        }

        if (lhs == std::byte{0}) {
            frame.returnValue = 0;
            return;
        }
    }

    frame.returnValue = 0;
}

void CopyString(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto source = frame.arguments[1];
    auto& memory = RequireMemory(frame);

    std::vector<std::byte> buffer;

    for (std::uint64_t index = 0;; ++index) {
        const auto value =
            ReadByte(
                memory,
                CheckedAddress(source, index));
        buffer.push_back(value);

        if (value == std::byte{0}) {
            break;
        }

        if (index ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc string copy overflows");
        }
    }

    memory.Write(destination, buffer);
    frame.returnValue = destination;
}

void CopyStringBounded(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto source = frame.arguments[1];
    const auto size = CheckedSize(frame.arguments[2]);

    frame.returnValue = destination;

    if (size == 0) {
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size, std::byte{0});

    bool terminated = false;

    for (std::size_t index = 0;
         index < size && !terminated;
         ++index) {
        const auto value =
            ReadByte(
                memory,
                CheckedAddress(
                    source,
                    static_cast<std::uint64_t>(index)));
        buffer[index] = value;
        terminated = value == std::byte{0};
    }

    memory.Write(destination, buffer);
}

void ZeroMemory(HleCallFrame& frame) {
    const auto destination = frame.arguments[0];
    const auto size = CheckedSize(frame.arguments[1]);

    if (size == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size, std::byte{0});
    memory.Write(destination, buffer);
    frame.returnValue = 0;
}

void CopyMemoryBsd(HleCallFrame& frame) {
    const auto source = frame.arguments[0];
    const auto destination = frame.arguments[1];
    const auto size = CheckedSize(frame.arguments[2]);

    if (size == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);
    std::vector<std::byte> buffer(size);
    memory.Read(source, buffer);
    memory.Write(destination, buffer);
    frame.returnValue = 0;
}


void FindStringByte(HleCallFrame& frame,
                    bool last) {
    const auto address = frame.arguments[0];
    const auto target =
        static_cast<std::byte>(frame.arguments[1] & 0xffu);
    auto& memory = RequireMemory(frame);

    std::uint64_t found = 0;

    for (std::uint64_t index = 0;; ++index) {
        const auto currentAddress =
            CheckedAddress(address, index);
        const auto value =
            ReadByte(memory, currentAddress);

        if (value == target) {
            found = currentAddress;
            if (!last) {
                frame.returnValue = found;
                return;
            }
        }

        if (value == std::byte{0}) {
            frame.returnValue = found;
            return;
        }

        if (index ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc string search overflows");
        }
    }
}

void FindSubstring(HleCallFrame& frame) {
    const auto haystack = frame.arguments[0];
    const auto needle = frame.arguments[1];
    auto& memory = RequireMemory(frame);

    if (ReadByte(memory, needle) == std::byte{0}) {
        frame.returnValue = haystack;
        return;
    }

    for (std::uint64_t start = 0;; ++start) {
        const auto first =
            ReadByte(
                memory,
                CheckedAddress(haystack, start));

        if (first == std::byte{0}) {
            frame.returnValue = 0;
            return;
        }

        bool matched = true;

        for (std::uint64_t index = 0;; ++index) {
            const auto needleByte =
                ReadByte(
                    memory,
                    CheckedAddress(needle, index));

            if (needleByte == std::byte{0}) {
                break;
            }

            const auto haystackByte =
                ReadByte(
                    memory,
                    CheckedAddress(
                        CheckedAddress(haystack, start),
                        index));

            if (haystackByte != needleByte) {
                matched = false;
                break;
            }

            if (haystackByte == std::byte{0}) {
                matched = false;
                break;
            }
        }

        if (matched) {
            frame.returnValue =
                CheckedAddress(haystack, start);
            return;
        }

        if (start ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc substring search overflows");
        }
    }
}

std::uint64_t FindStringEnd(
    GuestMemoryAccess& memory,
    std::uint64_t address) {
    for (std::uint64_t index = 0;; ++index) {
        const auto current =
            CheckedAddress(address, index);

        if (ReadByte(memory, current) == std::byte{0}) {
            return current;
        }

        if (index ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc string end search overflows");
        }
    }
}

void AppendString(HleCallFrame& frame,
                  bool bounded) {
    const auto destination = frame.arguments[0];
    const auto source = frame.arguments[1];
    const auto maximum =
        bounded ? frame.arguments[2]
                : std::numeric_limits<std::uint64_t>::max();

    auto& memory = RequireMemory(frame);
    const auto appendAddress =
        FindStringEnd(memory, destination);

    std::vector<std::byte> buffer;

    for (std::uint64_t index = 0;
         index < maximum;
         ++index) {
        const auto value =
            ReadByte(
                memory,
                CheckedAddress(source, index));

        if (value == std::byte{0}) {
            break;
        }

        buffer.push_back(value);
    }

    buffer.push_back(std::byte{0});
    memory.Write(appendAddress, buffer);
    frame.returnValue = destination;
}

void StringLength(HleCallFrame& frame) {
    const auto address = frame.arguments[0];
    auto& memory = RequireMemory(frame);

    std::uint64_t length = 0;
    while (true) {
        const auto currentAddress =
            CheckedAddress(address, length);

        if (ReadByte(memory, currentAddress) == std::byte{0}) {
            frame.returnValue = length;
            return;
        }

        if (length ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error(
                "Guest libc string length overflows");
        }

        ++length;
    }
}

void StringLengthBounded(HleCallFrame& frame) {
    const auto address = frame.arguments[0];
    const auto maximum = frame.arguments[1];

    if (maximum == 0) {
        frame.returnValue = 0;
        return;
    }

    auto& memory = RequireMemory(frame);

    for (std::uint64_t length = 0;
         length < maximum;
         ++length) {
        const auto currentAddress =
            CheckedAddress(address, length);

        if (ReadByte(memory, currentAddress) == std::byte{0}) {
            frame.returnValue = length;
            return;
        }
    }

    frame.returnValue = maximum;
}

} // namespace

void BasicLibc::Register(HleRegistry& registry,
                         std::string module) {
    if (module.empty()) {
        throw std::invalid_argument(
            "Basic libc module name cannot be empty");
    }

    const auto moduleName = module;

    registry.RegisterSymbol(
        moduleName,
        "memcpy",
        CopyMemory);

    registry.RegisterSymbol(
        moduleName,
        "memmove",
        CopyMemory);

    registry.RegisterSymbol(
        moduleName,
        "memset",
        SetMemory);

    registry.RegisterSymbol(
        moduleName,
        "memcmp",
        CompareMemory);


    registry.RegisterSymbol(
        moduleName,
        "memchr",
        FindMemoryByte);

    registry.RegisterSymbol(
        moduleName,
        "strcmp",
        CompareStrings);

    registry.RegisterSymbol(
        moduleName,
        "strncmp",
        CompareStringsBounded);

    registry.RegisterSymbol(
        moduleName,
        "strcpy",
        CopyString);

    registry.RegisterSymbol(
        moduleName,
        "strncpy",
        CopyStringBounded);

    registry.RegisterSymbol(
        moduleName,
        "bzero",
        ZeroMemory);

    registry.RegisterSymbol(
        moduleName,
        "bcopy",
        CopyMemoryBsd);


    registry.RegisterSymbol(
        moduleName,
        "strchr",
        [](HleCallFrame& frame) {
            FindStringByte(frame, false);
        });

    registry.RegisterSymbol(
        moduleName,
        "strrchr",
        [](HleCallFrame& frame) {
            FindStringByte(frame, true);
        });

    registry.RegisterSymbol(
        moduleName,
        "strstr",
        FindSubstring);

    registry.RegisterSymbol(
        moduleName,
        "strcat",
        [](HleCallFrame& frame) {
            AppendString(frame, false);
        });

    registry.RegisterSymbol(
        moduleName,
        "strncat",
        [](HleCallFrame& frame) {
            AppendString(frame, true);
        });

    registry.RegisterSymbol(
        moduleName,
        "strlen",
        StringLength);

    registry.RegisterSymbol(
        std::move(module),
        "strnlen",
        StringLengthBounded);
}

} // namespace ps5emu::hle
