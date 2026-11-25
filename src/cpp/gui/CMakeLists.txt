# Phase2 GUI 壳：Qt6 Widgets + Multimedia（MinGW 工具链与 core 同源）
# 工具链硬约束：必须用 msvcrt 版 MinGW（Qt mingw_64 归档基于 msvcrt CRT；
# ucrt 工具链混链会跨堆释放崩溃，实测定案，见 docs/06）。
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Multimedia)
qt_standard_project_setup()  # AUTOMOC/AUTOUIC/AUTORCC + C++17 基线（core 的 C++20 目标不受影响）

add_executable(amvconv_gui WIN32
    main.cpp
    common.h
    main_window.h main_window.cpp
    convert_interface.h convert_interface.cpp
    media_player.h media_player.cpp
    preview_interface.h preview_interface.cpp
    preview_dialog.h preview_dialog.cpp
    about_interface.h about_interface.cpp
    fluent/style.h fluent/style.cpp
    fluent/icons.h fluent/icons.cpp
    fluent/widgets.h fluent/widgets.cpp
    fluent/nav_window.h fluent/nav_window.cpp)

target_link_libraries(amvconv_gui PRIVATE amvconv_core Qt6::Widgets Qt6::Multimedia)
