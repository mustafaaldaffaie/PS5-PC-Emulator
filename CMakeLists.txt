cmake_minimum_required(VERSION 3.24)
project(PS5PCEmulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PS5EMU_BUILD_FRONTEND "Build the Qt desktop frontend" OFF)
option(PS5EMU_BUILD_TESTS "Build project tests" ON)

if(PS5EMU_BUILD_TESTS)
    enable_testing()
    # Test assertions must remain active in Release and multi-config builds.
    add_library(ps5emu_test_options INTERFACE)
    if(MSVC)
        target_compile_options(ps5emu_test_options INTERFACE /UNDEBUG)
    else()
        target_compile_options(ps5emu_test_options INTERFACE -UNDEBUG)
    endif()
endif()

add_subdirectory(src/core)
add_subdirectory(src/hle)
add_subdirectory(src/runtime)
add_subdirectory(src/graphics)
add_subdirectory(src/app)

if(PS5EMU_BUILD_FRONTEND)
    add_subdirectory(src/frontend)
endif()
