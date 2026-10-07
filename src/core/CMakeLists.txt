add_library(ps5emu_core STATIC
    src/Core.cpp
)

target_include_directories(ps5emu_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
