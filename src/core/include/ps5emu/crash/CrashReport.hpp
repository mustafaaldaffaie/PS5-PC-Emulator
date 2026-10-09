#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace ps5emu::crash {

struct CrashContext {
    std::chrono::system_clock::time_point timestamp =
        std::chrono::system_clock::now();
    std::string reason;
    std::string detail;
    std::uint64_t threadId = 0;
    std::uint64_t instructionPointer = 0;
    std::uint64_t stackPointer = 0;
};

class CrashReport final {
public:
    [[nodiscard]] static std::string
    Format(const CrashContext& context);

    static void
    WriteFile(const std::filesystem::path& path,
              const CrashContext& context);
};

} // namespace ps5emu::crash
