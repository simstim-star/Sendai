cmake_minimum_required (VERSION 3.25)

project(__sendai LANGUAGES C)
set(CMAKE_C_STANDARD 99)

find_package(XMathC REQUIRED PATHS "C:/xmathc")

add_subdirectory(src)