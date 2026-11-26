# Phase2 GUI 壳：Qt6 Widgets/Multimedia + vendored Qt-Fluent-Widgets（GPL-3.0，
# 与本项目许可兼容；third_party/LICENSE-QtFluentWidgets.txt）。
# 工具链硬约束：msvcrt 版 MinGW（见 docs/06 Phase2 节）。
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg Multimedia)
qt_standard_project_setup()  # AUTOMOC/AUTOUIC/AUTORCC

# vendored Fluent 库：图表模块（WebEngine/ECharts）不用，关掉去依赖
set(QFW_BUILD_CHART OFF CACHE BOOL "" FORCE)
set(QFW_ENABLE_IPO OFF CACHE BOOL "" FORCE)
# vendored 子目录经 add_subdirectory 引入时拿不到上游根 CMakeLists 的
# qt_standard_project_setup()，AUTOMOC/AUTORCC 必须在此显式开启（目录作用域继承）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/qtfluentwidgets
                 ${CMAKE_CURRENT_BINARY_DIR}/qfw)

add_executable(amvconv_gui WIN32
    main.cpp
    common.h
    main_window.h main_window.cpp
    convert_interface.h convert_interface.cpp
    media_player.h media_player.cpp
    preview_interface.h preview_interface.cpp
    preview_dialog.h preview_dialog.cpp
    about_interface.h about_interface.cpp)

target_link_libraries(amvconv_gui PRIVATE amvconv_core qtfluentwidgets
    Qt6::Widgets Qt6::Multimedia Qt6::Svg)
# 库内 windows_window_effect 用了 DwmFlush，上游 CMake 未链 dwmapi（MinGW 需显式）
if(WIN32)
    target_link_libraries(amvconv_gui PRIVATE dwmapi)
endif()
