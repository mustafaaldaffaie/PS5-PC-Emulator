add_library(ps5emu_runtime STATIC
    src/ExecutableLinker.cpp
    src/ImportResolver.cpp
)

target_include_directories(ps5emu_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ps5emu_runtime
    PUBLIC
        ps5emu_core
        ps5emu_hle
)

if(PS5EMU_BUILD_TESTS)
    add_executable(ps5emu_executable_linker_tests
        tests/ExecutableLinkerTests.cpp
    )
    target_link_libraries(ps5emu_executable_linker_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME executable_linker COMMAND ps5emu_executable_linker_tests)

    add_executable(ps5emu_import_resolver_tests
        tests/ImportResolverTests.cpp
    )
    target_link_libraries(ps5emu_import_resolver_tests PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME import_resolver COMMAND ps5emu_import_resolver_tests)
endif()
