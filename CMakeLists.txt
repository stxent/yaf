# Copyright (C) 2017 xent
# Project is distributed under the terms of the MIT License

cmake_minimum_required(VERSION 3.21)
project(yaf C)

find_package(xcore QUIET)
if(xcore_FOUND)
    message(STATUS "Found xcore package")
endif()

# Library configuration

option(BUILD_TESTING "Enable testing support." OFF)
option(YAF_THREADS "Enable multithreading." ON)
option(YAF_UNICODE "Enable support for Unicode characters." ON)
option(YAF_WRITE "Enable write functions." ON)
set(YAF_DEBUG 0 CACHE STRING "Debug level.")
set(YAF_SECTOR_SIZE 512 CACHE STRING "Size of a filesystem sector may be 512, 1024, 2048 or 4096 bytes.")

# Default compiler flags

if(BUILD_TESTING)
    set(CMAKE_C_STANDARD 23)
endif()

if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_COMPILE_OPTIONS_IPO -flto=auto -ffat-lto-objects)
    endif()
endif()

# Collect files for object libraries

add_subdirectory(sources)
add_library(${PROJECT_NAME} $<TARGET_OBJECTS:yaf_generic>)
target_link_libraries(${PROJECT_NAME} PUBLIC yaf_generic)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Configure library installation

install(TARGETS ${PROJECT_NAME}
        EXPORT yaf-targets
        ARCHIVE DESTINATION lib
)
install(EXPORT yaf-targets
        FILE yaf-targets.cmake
        DESTINATION lib/yaf
)
install(FILES "${PROJECT_SOURCE_DIR}/config.cmake.in"
        DESTINATION lib/yaf
        RENAME yaf-config.cmake
)
