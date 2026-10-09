#include <ps5emu/crash/CrashReport.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ps5emu::crash {
namespace {

std::string EscapeLine(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    for (const char character : value) {
        switch (character) {
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\\':
            result += "\\\\";
            break;
        default:
            result.push_back(character);
            break;
        }
    }

    return result;
}

std::string Timestamp(
    std::chrono::system_clock::time_point value) {
    const auto time =
        std::chrono::system_clock::to_time_t(value);

    std::tm utc{};

#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif

    std::ostringstream output;
    output
        << std::put_time(
            &utc,
            "%Y-%m-%dT%H:%M:%SZ");

    return output.str();
}

} // namespace

std::string CrashReport::Format(
    const CrashContext& context) {
    std::ostringstream output;

    output
        << "PS5-PC-Emulator Crash Report\n"
        << "timestamp=" << Timestamp(context.timestamp) << '\n'
        << "reason=" << EscapeLine(context.reason) << '\n'
        << "detail=" << EscapeLine(context.detail) << '\n'
        << "thread_id=" << context.threadId << '\n'
        << "instruction_pointer=0x"
        << std::hex
        << context.instructionPointer
        << '\n'
        << "stack_pointer=0x"
        << context.stackPointer
        << '\n';

    return output.str();
}

void CrashReport::WriteFile(
    const std::filesystem::path& path,
    const CrashContext& context) {
    const auto parent = path.parent_path();

    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(
            parent,
            error);

        if (error) {
            throw std::runtime_error(
                "Failed to create crash report directory");
        }
    }

    std::ofstream file(
        path,
        std::ios::binary | std::ios::trunc);

    if (!file) {
        throw std::runtime_error(
            "Failed to open crash report file");
    }

    const auto report = Format(context);

    file.write(
        report.data(),
        static_cast<std::streamsize>(
            report.size()));

    if (!file) {
        throw std::runtime_error(
            "Failed to write crash report file");
    }
}

} // namespace ps5emu::crash
