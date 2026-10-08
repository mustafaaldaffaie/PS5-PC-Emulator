add_library(ps5emu_runtime STATIC
    src/ExecutableLinker.cpp
    src/GuestCallDispatcher.cpp
    src/GuestExecutionBuilder.cpp
    src/GuestThreadMemory.cpp
    src/HleThunkTable.cpp
    src/HleTrapHandler.cpp
    src/ImportResolver.cpp
    src/NativeContextAdapter.cpp
    src/NativeExecutionBuilder.cpp
    src/NativeImageMaterializer.cpp
    src/NativeLeafExecutor.cpp
    src/NativeMemoryRegion.cpp
    src/SceExecutablePreparer.cpp
    src/SceImportResolver.cpp
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

    add_executable(ps5emu_sce_import_resolver_tests
        tests/SceImportResolverTests.cpp
    )
    target_link_libraries(ps5emu_sce_import_resolver_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME sce_import_resolver COMMAND ps5emu_sce_import_resolver_tests)

    add_executable(ps5emu_sce_executable_preparer_tests
        tests/SceExecutablePreparerTests.cpp
    )
    target_link_libraries(ps5emu_sce_executable_preparer_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME sce_executable_preparer COMMAND ps5emu_sce_executable_preparer_tests)

    add_executable(ps5emu_guest_call_dispatcher_tests
        tests/GuestCallDispatcherTests.cpp
    )
    target_link_libraries(ps5emu_guest_call_dispatcher_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME guest_call_dispatcher COMMAND ps5emu_guest_call_dispatcher_tests)

    add_executable(ps5emu_guest_thread_memory_tests
        tests/GuestThreadMemoryTests.cpp
    )
    target_link_libraries(ps5emu_guest_thread_memory_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME guest_thread_memory COMMAND ps5emu_guest_thread_memory_tests)

    add_executable(ps5emu_guest_execution_builder_tests
        tests/GuestExecutionBuilderTests.cpp
    )
    target_link_libraries(ps5emu_guest_execution_builder_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME guest_execution_builder COMMAND ps5emu_guest_execution_builder_tests)

    add_executable(ps5emu_hle_trap_handler_tests
        tests/HleTrapHandlerTests.cpp
    )
    target_link_libraries(ps5emu_hle_trap_handler_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME hle_trap_handler COMMAND ps5emu_hle_trap_handler_tests)

    add_executable(ps5emu_native_context_adapter_tests
        tests/NativeContextAdapterTests.cpp
    )
    target_link_libraries(ps5emu_native_context_adapter_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME native_context_adapter COMMAND ps5emu_native_context_adapter_tests)

    add_executable(ps5emu_native_memory_region_tests
        tests/NativeMemoryRegionTests.cpp
    )
    target_link_libraries(ps5emu_native_memory_region_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME native_memory_region COMMAND ps5emu_native_memory_region_tests)

    add_executable(ps5emu_native_image_materializer_tests
        tests/NativeImageMaterializerTests.cpp
    )
    target_link_libraries(ps5emu_native_image_materializer_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME native_image_materializer COMMAND ps5emu_native_image_materializer_tests)

    add_executable(ps5emu_native_execution_builder_tests
        tests/NativeExecutionBuilderTests.cpp
    )
    target_link_libraries(ps5emu_native_execution_builder_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME native_execution_builder COMMAND ps5emu_native_execution_builder_tests)

    add_executable(ps5emu_native_leaf_executor_tests
        tests/NativeLeafExecutorTests.cpp
    )
    target_link_libraries(ps5emu_native_leaf_executor_tests
        PRIVATE ps5emu_runtime ps5emu_test_options)
    add_test(NAME native_leaf_executor COMMAND ps5emu_native_leaf_executor_tests)
endif()
