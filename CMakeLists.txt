cmake_minimum_required(VERSION 3.10)
project(mucc-next LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pt
pt.cpp
file.cpp
file.h
part.cpp
part.h
codegen.cpp
codegen.h
partdef.cpp
mucc_def.h
part_buffer.h
reserved.h
)


# Visual Studio
if(MSVC)
    target_compile_options(pt
    PRIVATE
    /utf-8 /D_CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_options(pt
    PRIVATE
    -O2 -Wall -Wextra
    )
endif()
