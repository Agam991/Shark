cmake_minimum_required(VERSION 3.16)

project(Shark
    VERSION 0.1.0
    DESCRIPTION "A fast, multithreaded C++ chess engine (UCI)"
    LANGUAGES CXX)

# ---- Global standards -------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Warnings: be strict, we want a correct engine.
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Release goes fast; enable native tuning unless disabled.
option(SHARK_NATIVE "Tune the build for the host CPU (-march=native)" ON)
if(SHARK_NATIVE AND NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
    if(HAS_MARCH_NATIVE)
        add_compile_options($<$<CONFIG:Release>:-march=native>)
    endif()
endif()

enable_testing()

add_subdirectory(core)
