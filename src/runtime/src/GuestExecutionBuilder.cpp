#include <ps5emu/runtime/GuestExecutionBuilder.hpp>

#include <ps5emu/elf/Elf64.hpp>
#include <ps5emu/hle/BuiltinServices.hpp>

#include <utility>

namespace ps5emu::runtime {

PreparedGuestExecution GuestExecutionBuilder::Prepare(
    std::span<const std::byte> executableBytes,
    const GuestExecutionOptions& options) {
    auto registry =
        hle::BuiltinServices::CreateRegistry();

    memory::GuestMemory memory;
    HleThunkTable thunks(options.thunks);

    auto preparedImage =
        SceExecutablePreparer::Prepare(
            executableBytes,
            memory,
            registry,
            thunks,
            ScePrepareOptions{
                .loadBias = options.loadBias,
            });

    const auto image =
        elf::Elf64::Parse(executableBytes);

    auto threadMemory =
        GuestThreadMemory::Create(
            executableBytes,
            image,
            memory,
            options.threadMemory);

    SysvGuestContext context;
    context.rip =
        preparedImage.linked.loaded.entryPoint;

    GuestThreadMemory::ApplyToContext(
        threadMemory,
        context);

    return PreparedGuestExecution{
        .registry = std::move(registry),
        .memory = std::move(memory),
        .thunks = std::move(thunks),
        .image = std::move(preparedImage),
        .threadMemory = std::move(threadMemory),
        .context = context,
    };
}

} // namespace ps5emu::runtime
