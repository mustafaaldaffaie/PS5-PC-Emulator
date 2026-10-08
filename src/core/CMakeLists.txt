add_library(ps5emu_core STATIC
    src/Core.cpp
    src/elf/DynamicMetadata.cpp
    src/elf/DynamicSymbol.cpp
    src/elf/Elf64.cpp
    src/elf/ElfFileView.cpp
    src/elf/ImportTable.cpp
    src/elf/Relocation.cpp
    src/elf/SceModuleMetadata.cpp
    src/elf/SceQualifiedSymbol.cpp
    src/loader/ExecutableImageLoader.cpp
    src/loader/RelocationApplier.cpp
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
    target_link_libraries(ps5emu_elf64_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME elf64_parser COMMAND ps5emu_elf64_tests)

    add_executable(ps5emu_dynamic_metadata_tests
        tests/DynamicMetadataTests.cpp
    )
    target_link_libraries(ps5emu_dynamic_metadata_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME dynamic_metadata COMMAND ps5emu_dynamic_metadata_tests)

    add_executable(ps5emu_tls_segment_tests
        tests/TlsSegmentTests.cpp
    )
    target_link_libraries(ps5emu_tls_segment_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME tls_segment COMMAND ps5emu_tls_segment_tests)

    add_executable(ps5emu_sce_dynamic_tables_tests
        tests/SceDynamicTablesTests.cpp
    )
    target_link_libraries(ps5emu_sce_dynamic_tables_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME sce_dynamic_tables COMMAND ps5emu_sce_dynamic_tables_tests)

    add_executable(ps5emu_sce_module_metadata_tests
        tests/SceModuleMetadataTests.cpp
    )
    target_link_libraries(ps5emu_sce_module_metadata_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME sce_module_metadata COMMAND ps5emu_sce_module_metadata_tests)

    add_executable(ps5emu_linking_metadata_tests
        tests/LinkingMetadataTests.cpp
    )
    target_link_libraries(ps5emu_linking_metadata_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME linking_metadata COMMAND ps5emu_linking_metadata_tests)

    add_executable(ps5emu_guest_memory_tests
        tests/GuestMemoryTests.cpp
    )
    target_link_libraries(ps5emu_guest_memory_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME guest_memory COMMAND ps5emu_guest_memory_tests)

    add_executable(ps5emu_executable_image_loader_tests
        tests/ExecutableImageLoaderTests.cpp
    )
    target_link_libraries(ps5emu_executable_image_loader_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME executable_image_loader COMMAND ps5emu_executable_image_loader_tests)

    add_executable(ps5emu_relocation_applier_tests
        tests/RelocationApplierTests.cpp
    )
    target_link_libraries(ps5emu_relocation_applier_tests PRIVATE ps5emu_core ps5emu_test_options)
    add_test(NAME relocation_applier COMMAND ps5emu_relocation_applier_tests)
endif()
