# Phase2 GUI 壳：Qt6 Widgets/Multimedia + Qt-Fluent-Widgets（submodule，GPL-3.0，
# 与本项目许可兼容；上游 LICENSE 随 submodule 自带）。
# 工具链硬约束：msvcrt 版 MinGW（见 docs/06 Phase2 节）。
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg Multimedia)
qt_standard_project_setup()  # AUTOMOC/AUTOUIC/AUTORCC

set(QFW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/qt-fluent-widgets)
if(NOT EXISTS ${QFW_SRC}/qtfluentwidgets/CMakeLists.txt)
    message(FATAL_ERROR
        "Qt-Fluent-Widgets submodule 未拉取：git submodule update --init src/cpp/third_party/qt-fluent-widgets")
endif()

# vendoring 补丁（幂等）：剔除 chat_math.qrc 13MB matplotlib 数据 + Q_INIT_RESOURCE 置空
set(QFW_PATCH ${QFW_SRC}/../patches/01-drop-chat-math-qrc.patch)
file(READ ${QFW_SRC}/qtfluentwidgets/common/chat_math.cpp _qfw_chatmath)
string(FIND "${_qfw_chatmath}" "vendoring 补丁" _qfw_patched)
if(_qfw_patched EQUAL -1)
    execute_process(COMMAND git apply --ignore-whitespace ${QFW_PATCH}
        WORKING_DIRECTORY ${QFW_SRC} RESULT_VARIABLE _qfw_apply_rc)
    if(NOT _qfw_apply_rc EQUAL 0)
        message(FATAL_ERROR "Qt-Fluent-Widgets 补丁应用失败（submodule commit 可能已漂移）")
    endif()
endif()

# vendored 子目录经 add_subdirectory 引入时拿不到上游根 CMakeLists 的
# qt_standard_project_setup()，AUTOMOC/AUTORCC 必须在此显式开启（目录作用域继承）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# 图表模块（WebEngine/ECharts）不用，关掉去依赖；IPO 关（MinGW LTO 慢且无收益）
set(QFW_BUILD_CHART OFF CACHE BOOL "" FORCE)
set(QFW_ENABLE_IPO OFF CACHE BOOL "" FORCE)
add_subdirectory(${QFW_SRC}/qtfluentwidgets ${CMAKE_CURRENT_BINARY_DIR}/qfw)

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
