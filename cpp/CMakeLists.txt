# libamvconv Phase1 骨架：编码内核（DCT/量化逐位对齐 numpy，熵编码复用 _entropy.c）
# 构建：cmake -S cpp -B cpp/build && cmake --build cpp/build
cmake_minimum_required(VERSION 3.24)
project(amvconv C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(amvconv_core STATIC
    src/amvenc.cpp
    # 单一事实源：直接编译 Python 侧同款 C 核，不复制不改动
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/amvconv/core/_entropy.c)
target_include_directories(amvconv_core PUBLIC src)

# 逐位浮点契约：禁 FMA 收缩与任何重排（MSVC /fp:strict 默认即精确语义，无需额外开关；
# GCC/Clang 默认 -ffp-contract=fast 会把 a*b+c 融成 fma → 必须关）
if(NOT MSVC)
    target_compile_options(amvconv_core PRIVATE -ffp-contract=off)
endif()

add_executable(enc_test tools/enc_test.cpp)
target_link_libraries(enc_test PRIVATE amvconv_core)
