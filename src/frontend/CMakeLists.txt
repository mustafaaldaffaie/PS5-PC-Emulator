find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

qt_add_executable(ps5emu_frontend
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
)

target_link_libraries(ps5emu_frontend
    PRIVATE
        ps5emu_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

qt_add_translations(ps5emu_frontend
    TS_FILES
        ${CMAKE_SOURCE_DIR}/locales/ps5emu_en.ts
        ${CMAKE_SOURCE_DIR}/locales/ps5emu_ar.ts
)
