add_library(ps5emu_hle STATIC
    src/BasicLibc.cpp
    src/HleRegistry.cpp
    src/KernelTime.cpp
    src/Nid.cpp
    src/NidNameDatabase.cpp
)

target_include_directories(ps5emu_hle
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(PS5EMU_BUILD_TESTS)
    add_executable(ps5emu_hle_registry_tests
        tests/HleRegistryTests.cpp
    )
    target_link_libraries(ps5emu_hle_registry_tests PRIVATE ps5emu_hle ps5emu_test_options)
    add_test(NAME hle_registry COMMAND ps5emu_hle_registry_tests)

    add_executable(ps5emu_nid_tests
        tests/NidTests.cpp
    )
    target_link_libraries(ps5emu_nid_tests PRIVATE ps5emu_hle ps5emu_test_options)
    add_test(NAME nid_hash COMMAND ps5emu_nid_tests)

    add_executable(ps5emu_nid_name_database_tests
        tests/NidNameDatabaseTests.cpp
    )
    target_link_libraries(ps5emu_nid_name_database_tests
        PRIVATE ps5emu_hle ps5emu_test_options)
    add_test(NAME nid_name_database COMMAND ps5emu_nid_name_database_tests)

    add_executable(ps5emu_basic_libc_tests
        tests/BasicLibcTests.cpp
    )
    target_link_libraries(ps5emu_basic_libc_tests
        PRIVATE ps5emu_hle ps5emu_test_options)
    add_test(NAME basic_libc COMMAND ps5emu_basic_libc_tests)

    add_executable(ps5emu_kernel_time_tests
        tests/KernelTimeTests.cpp
    )
    target_link_libraries(ps5emu_kernel_time_tests
        PRIVATE ps5emu_hle ps5emu_test_options)
    add_test(NAME kernel_time COMMAND ps5emu_kernel_time_tests)
endif()
