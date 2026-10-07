add_executable(ps5emu
    main.cpp
)

target_link_libraries(ps5emu PRIVATE ps5emu_core)
