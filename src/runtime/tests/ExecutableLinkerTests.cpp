#include <ps5emu/runtime/ExecutableLinker.hpp>

#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    assert(offset + sizeof(T) <= bytes.size());
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Dynamic(std::vector<std::byte>& bytes, std::size_t index,
             std::int64_t tag, std::uint64_t value) {
    Write(bytes, 0x180 + index * 16, tag);
    Write(bytes, 0x188 + index * 16, value);
}

void Symbol(std::vector<std::byte>& bytes, std::size_t index,
            std::uint32_t name, std::uint8_t info,
            std::uint16_t section, std::uint64_t value,
            std::uint64_t size = 0) {
    const auto offset = 0x300 + index * 24;
    Write(bytes, offset, name);
    Write(bytes, offset + 4, info);
    Write(bytes, offset + 6, section);
    Write(bytes, offset + 8, value);
    Write(bytes, offset + 16, size);
}

void Relocation(std::vector<std::byte>& bytes, std::size_t index,
                std::uint64_t target, std::uint32_t symbol,
                std::uint32_t type, std::int64_t addend = 0) {
    const auto offset = 0x400 + index * 24;
    Write(bytes, offset, target);
    Write(bytes, offset + 8, (std::uint64_t{symbol} << 32) | type);
    Write(bytes, offset + 16, addend);
}

void ProgramHeader(std::vector<std::byte>& bytes,
                   std::size_t index,
                   std::uint32_t type,
                   std::uint32_t flags,
                   std::uint64_t fileOffset,
                   std::uint64_t virtualAddress,
                   std::uint64_t fileSize,
                   std::uint64_t memorySize,
                   std::uint64_t alignment) {
    const auto offset = 64 + index * 56;
    Write<std::uint32_t>(bytes, offset + 0, type);
    Write<std::uint32_t>(bytes, offset + 4, flags);
    Write<std::uint64_t>(bytes, offset + 8, fileOffset);
    Write<std::uint64_t>(bytes, offset + 16, virtualAddress);
    Write<std::uint64_t>(bytes, offset + 24, 0);
    Write<std::uint64_t>(bytes, offset + 32, fileSize);
    Write<std::uint64_t>(bytes, offset + 40, memorySize);
    Write<std::uint64_t>(bytes, offset + 48, alignment);
}

std::vector<std::byte> MakeElf() {
    std::vector<std::byte> bytes(0x800);
    bytes[0] = std::byte{0x7f};
    bytes[1] = std::byte{'E'};
    bytes[2] = std::byte{'L'};
    bytes[3] = std::byte{'F'};
    bytes[4] = std::byte{2};
    bytes[5] = std::byte{1};
    bytes[6] = std::byte{1};
    Write<std::uint16_t>(bytes, 16, 3);
    Write<std::uint16_t>(bytes, 18, 62);
    Write<std::uint32_t>(bytes, 20, 1);
    Write<std::uint64_t>(bytes, 24, 0x1010);
    Write<std::uint64_t>(bytes, 32, 64);
    Write<std::uint16_t>(bytes, 52, 64);
    Write<std::uint16_t>(bytes, 54, 56);
    Write<std::uint16_t>(bytes, 56, 3);

    ProgramHeader(bytes, 0, 1, 5, 0x100, 0x1000, 0x700, 0x800, 0x100);
    ProgramHeader(bytes, 1, 2, 4, 0x180, 0x1080, 0x80, 0x80, 8);
    ProgramHeader(bytes, 2, 7, 4, 0x680, 0, 4, 0x20, 16);

    Dynamic(bytes, 0, 5, 0x1500);
    Dynamic(bytes, 1, 10, 16);
    Dynamic(bytes, 2, 6, 0x1200);
    Dynamic(bytes, 3, 11, 24);
    Dynamic(bytes, 4, 7, 0x1300);
    Dynamic(bytes, 5, 8, 10 * 24);
    Dynamic(bytes, 6, 9, 24);
    Dynamic(bytes, 7, 0, 0);

    const char names[] = "\0foo\0bar\0tls\0";
    std::memcpy(bytes.data() + 0x600, names, sizeof(names));
    Symbol(bytes, 1, 1, 0x12, 0, 0);
    Symbol(bytes, 2, 0, 0x11, 1, 0x1020);
    Symbol(bytes, 3, 0, 0x11, 0xfff1, 0x42);
    Symbol(bytes, 4, 5, 0x22, 0, 0);
    Symbol(bytes, 5, 9, 0x16, 1, 8, 8);

    bytes[0x680] = std::byte{0xaa};
    bytes[0x681] = std::byte{0xbb};
    bytes[0x682] = std::byte{0xcc};
    bytes[0x683] = std::byte{0xdd};

    Relocation(bytes, 0, 0x1600, 0, 8, 0x1000);
    Relocation(bytes, 1, 0x1608, 1, 7);
    Relocation(bytes, 2, 0x1610, 2, 1, 4);
    Relocation(bytes, 3, 0x1618, 3, 1, 1);
    Relocation(bytes, 4, 0x1620, 4, 6);
    Relocation(bytes, 5, 0x1628, 1, 6);
    Relocation(bytes, 6, 0x1630, 0, 1, 7);
    Relocation(bytes, 7, 0x1638, 5, 16);
    Relocation(bytes, 8, 0x1640, 5, 17, 4);
    Relocation(bytes, 9, 0x1648, 5, 18);
    return bytes;
}

std::uint64_t ReadU64(const ps5emu::memory::GuestMemory& memory,
                      std::uint64_t address) {
    std::uint64_t result = 0;
    const auto bytes = memory.Read(address, sizeof(result));
    std::memcpy(&result, bytes.data(), sizeof(result));
    return result;
}

void ExpectFailure(const std::vector<std::byte>& bytes,
                   const ps5emu::runtime::LinkOptions& options) {
    using namespace ps5emu::memory;
    GuestMemory memory;
    memory.Map(0x9000, 8, Protection::Read);
    const std::byte marker{0x5a};
    memory.Initialize(0x9000, std::span(&marker, 1));
    bool threw = false;
    try {
        static_cast<void>(ps5emu::runtime::ExecutableLinker::Load(
            bytes, memory, options));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    assert(memory.Mappings().size() == 1);
    assert(memory.Read(0x9000, 1)[0] == marker);
}

} // namespace

int main() {
    using namespace ps5emu;
    const auto bytes = MakeElf();
    runtime::LinkOptions options;
    options.loadBias = 0x500000;
    std::size_t calls = 0;
    options.resolveExternal = [&](const elf::DynamicSymbol& symbol)
        -> std::optional<std::uint64_t> {
        ++calls;
        if (symbol.name == "foo") {
            return 0x700000;
        }
        return std::nullopt;
    };

    memory::GuestMemory memory;
    const auto linked = runtime::ExecutableLinker::Load(bytes, memory, options);
    assert(linked.loaded.entryPoint == 0x501010);
    assert(linked.loaded.mappedSegmentCount == 1);
    assert(linked.appliedRelocationCount == 10);
    assert(linked.resolvedExternalSymbolCount == 1);
    assert(linked.unresolvedWeakSymbolCount == 1);
    assert(calls == 2);
    assert(ReadU64(memory, 0x501600) == 0x501000);
    assert(ReadU64(memory, 0x501608) == 0x700000);
    assert(ReadU64(memory, 0x501610) == 0x501024);
    assert(ReadU64(memory, 0x501618) == 0x43);
    assert(ReadU64(memory, 0x501620) == 0);
    assert(ReadU64(memory, 0x501628) == 0x700000);
    assert(ReadU64(memory, 0x501630) == 7);
    assert(ReadU64(memory, 0x501638) == 1);
    assert(ReadU64(memory, 0x501640) == 12);
    assert(ReadU64(memory, 0x501648) ==
           std::numeric_limits<std::uint64_t>::max() - 0x17);
    assert(!memory::HasProtection(
        memory.Mappings()[0].protection,
        memory::Protection::Write));

    ExpectFailure(bytes, {});
    auto changed = bytes;
    Relocation(changed, 9, 0x1648, 0, 999);
    ExpectFailure(changed, options);
    changed = bytes;
    Relocation(changed, 9, 0x9000, 0, 8);
    ExpectFailure(changed, {});
    changed = bytes;
    Relocation(changed, 9, 0x17fc, 0, 8);
    ExpectFailure(changed, options);
    changed = bytes;
    Relocation(changed, 6, 0x1630, 5, 1);
    ExpectFailure(changed, options);
    changed = bytes;
    Symbol(changed, 5, 9, 0x12, 1, 8, 8);
    ExpectFailure(changed, options);
    changed = bytes;
    Symbol(changed, 5, 9, 0x16, 0, 0, 8);
    ExpectFailure(changed, options);
    changed = bytes;
    Symbol(changed, 5, 9, 0x16, 1, 0x20, 1);
    ExpectFailure(changed, options);
    changed = bytes;
    Relocation(changed, 7, 0x1638, 5, 16, 1);
    ExpectFailure(changed, options);
    changed = bytes;
    Symbol(changed, 2, 0, 0x11, 0xfff2, 0x1020);
    ExpectFailure(changed, options);
    auto overflow = options;
    overflow.loadBias = std::numeric_limits<std::uint64_t>::max();
    ExpectFailure(bytes, overflow);
    auto throwing = options;
    throwing.resolveExternal = [](const elf::DynamicSymbol&)
        -> std::optional<std::uint64_t> {
        throw std::runtime_error("Resolver failed");
    };
    ExpectFailure(bytes, throwing);
    return 0;
}
