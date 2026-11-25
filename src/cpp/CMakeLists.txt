# libamvconv Phase1 骨架：编码内核（DCT/量化逐位对齐 numpy，熵编码复用 _entropy.c）
# 构建：cmake -S src/cpp -B src/cpp/build && cmake --build src/cpp/build
cmake_minimum_required(VERSION 3.24)
project(amvconv C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(amvconv_core STATIC
    src/amvenc.cpp
    src/container.cpp
    src/ffproc.cpp
    # 熵核单一事实源（PySide6 预言机分支同源文件，逐位一致性由 CI 语料比对强制）
    src/_entropy.c)
target_include_directories(amvconv_core PUBLIC src)

# 逐位浮点契约：禁 FMA 收缩与任何重排（MSVC /fp:strict 默认即精确语义，无需额外开关；
# GCC/Clang 默认 -ffp-contract=fast 会把 a*b+c 融成 fma → 必须关）
if(NOT MSVC)
    target_compile_options(amvconv_core PRIVATE -ffp-contract=off)
endif()

add_executable(enc_test tools/enc_test.cpp)
target_link_libraries(enc_test PRIVATE amvconv_core)

add_executable(amvconv_cli tools/amvconv_cli.cpp)
target_link_libraries(amvconv_cli PRIVATE amvconv_core)
