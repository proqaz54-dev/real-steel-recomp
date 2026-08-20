cmake_minimum_required(VERSION 3.16)

project(real_steel_recomp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(real_steel_recomp
    src/main.cpp
    src/xex.cpp
    src/ppc32_decode.cpp
    src/arm64_emit.cpp
)

target_compile_options(real_steel_recomp PRIVATE -Wall -Wextra)