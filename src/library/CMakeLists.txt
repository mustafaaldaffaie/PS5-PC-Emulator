add_library(ps5emu_library STATIC
    src/GameLibrary.cpp
)

target_include_directories(ps5emu_library
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(PS5EMU_BUILD_TESTS)
    add_executable(ps5emu_game_library_tests
        tests/GameLibraryTests.cpp
    )
    target_link_libraries(ps5emu_game_library_tests
        PRIVATE
            ps5emu_library
            ps5emu_test_options
    )
    add_test(
        NAME game_library
        COMMAND ps5emu_game_library_tests
    )
endif()
