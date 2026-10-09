add_library(ps5emu_graphics STATIC
    src/VulkanDeviceProbe.cpp
    src/VulkanInstanceProbe.cpp
    src/VulkanRuntimeLoader.cpp
)

target_include_directories(ps5emu_graphics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX AND NOT APPLE)
    target_link_libraries(ps5emu_graphics
        PRIVATE
            ${CMAKE_DL_LIBS}
    )
endif()

if(PS5EMU_BUILD_TESTS)
    add_executable(ps5emu_vulkan_runtime_loader_tests
        tests/VulkanRuntimeLoaderTests.cpp
    )
    target_link_libraries(ps5emu_vulkan_runtime_loader_tests
        PRIVATE
            ps5emu_graphics
            ps5emu_test_options
    )
    add_test(
        NAME vulkan_runtime_loader
        COMMAND ps5emu_vulkan_runtime_loader_tests
    )

    add_executable(ps5emu_vulkan_instance_probe_tests
        tests/VulkanInstanceProbeTests.cpp
    )
    target_link_libraries(ps5emu_vulkan_instance_probe_tests
        PRIVATE
            ps5emu_graphics
            ps5emu_test_options
    )
    add_test(
        NAME vulkan_instance_probe
        COMMAND ps5emu_vulkan_instance_probe_tests
    )

    add_executable(ps5emu_vulkan_device_probe_tests
        tests/VulkanDeviceProbeTests.cpp
    )
    target_link_libraries(ps5emu_vulkan_device_probe_tests
        PRIVATE
            ps5emu_graphics
            ps5emu_test_options
    )
    add_test(
        NAME vulkan_device_probe
        COMMAND ps5emu_vulkan_device_probe_tests
    )
endif()
