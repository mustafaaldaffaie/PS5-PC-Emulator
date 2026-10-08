add_executable(ps5emu
    main.cpp
)

target_link_libraries(ps5emu PRIVATE ps5emu_runtime)

if(PS5EMU_BUILD_TESTS)
    add_test(NAME cli_usage COMMAND ps5emu)
    set_tests_properties(cli_usage PROPERTIES
        PASS_REGULAR_EXPRESSION "prepare <elf-file>")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME cli_prepare
            COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tools/test_cli.py $<TARGET_FILE:ps5emu>)
    endif()
endif()
