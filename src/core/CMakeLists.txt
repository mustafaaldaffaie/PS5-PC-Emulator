add_library(ps5emu_core STATIC
    src/Core.cpp
    src/elf/Elf64.cpp
    src/loader/ExecutableImageLoader.cpp
    src/memory/GuestMemory.cpp
)

target_include_directories(ps5emu_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(PS5EMU_BUILD_TESTS)
    add_executable(ps5emu_elf64_tests
        tests/Elf64Tests.cpp
    )
    target_link_libraries(ps5emu_elf64_tests PRIVATE ps5emu_core)
    add_test(NAME elf64_parser COMMAND ps5emu_elf64_tests)

    add_executable(ps5emu_guest_memory_tests
        tests/GuestMemoryTests.cpp
    )
    target_link_libraries(ps5emu_guest_memory_tests PRIVATE ps5emu_core)
    add_test(NAME guest_memory COMMAND ps5emu_guest_memory_tests)

    add_executable(ps5emu_executable_image_loader_tests
        tests/ExecutableImageLoaderTests.cpp
    )
    target_link_libraries(ps5emu_executable_image_loader_tests PRIVATE ps5emu_core)
    add_test(NAME executable_image_loader COMMAND ps5emu_executable_image_loader_tests)
endif()
