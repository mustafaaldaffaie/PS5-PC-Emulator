#include <ps5emu/crash/CrashReport.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main() {
    using ps5emu::crash::CrashContext;
    using ps5emu::crash::CrashReport;

    const auto timestamp =
        std::chrono::system_clock::from_time_t(
            0);

    const CrashContext context{
        .timestamp = timestamp,
        .reason = "Unhandled exception",
        .detail = "line1\nline2\\tail",
        .threadId = 42,
        .instructionPointer = 0x1234abcd,
        .stackPointer = 0x70001000,
    };

    const auto report =
        CrashReport::Format(context);

    assert(
        report.find(
            "PS5-PC-Emulator Crash Report\n") == 0);
    assert(
        report.find(
            "timestamp=1970-01-01T00:00:00Z\n") !=
        std::string::npos);
    assert(
        report.find(
            "reason=Unhandled exception\n") !=
        std::string::npos);
    assert(
        report.find(
            "detail=line1\\nline2\\\\tail\n") !=
        std::string::npos);
    assert(
        report.find(
            "thread_id=42\n") !=
        std::string::npos);
    assert(
        report.find(
            "instruction_pointer=0x1234abcd\n") !=
        std::string::npos);
    assert(
        report.find(
            "stack_pointer=0x70001000\n") !=
        std::string::npos);

    const auto path =
        std::filesystem::temp_directory_path() /
        "ps5emu-crash-report-test" /
        "report.txt";

    std::error_code error;
    std::filesystem::remove_all(
        path.parent_path(),
        error);

    CrashReport::WriteFile(
        path,
        context);

    std::ifstream file(
        path,
        std::ios::binary);

    assert(file.good());

    const std::string written(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    assert(written == report);

    file.close();

    std::filesystem::remove_all(
        path.parent_path(),
        error);

    return 0;
}
