# Copyright (C) 2017 xent
# Project is distributed under the terms of the GNU General Public License v3.0

project(yaf C)
cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0069 NEW)

option(BUILD_TESTS "Enable building of test executables." OFF)
option(USE_LTO "Enable Link Time Optimization." OFF)
set(FS_NAME_LENGTH 64 CACHE STRING "Maximum length of filesystem entry name in bytes.")

# Library configuration
option(YAF_CONFIG_POOLS "Build driver with statically allocated buffers." ON)
option(YAF_CONFIG_THREADS "Enable multithreading." ON)
option(YAF_CONFIG_TIME "Build driver with time functions enabled." ON)
option(YAF_CONFIG_UNICODE "Enable support for Unicode characters." ON)
option(YAF_CONFIG_WRITE "Enable write functions." ON)
set(YAF_CONFIG_DEBUG 0 CACHE STRING "Debug level.")
set(YAF_CONFIG_SECTOR_SIZE 512 CACHE STRING "Size of a filesystem sector may be 512, 1024, 2048 or 4096 bytes.")

# Default compiler flags
set(FLAGS_DEFAULT "-fdata-sections -ffunction-sections -Wall -Wextra -Wshadow -pedantic")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS_DEFAULT} ${FLAGS_CPU}")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

find_package(xcore QUIET)
if(NOT xcore_FOUND)
    message(STATUS "Package information for xcore not found")
endif()
if(${YAF_CONFIG_THREADS})
    find_package(osw QUIET)
    if(NOT osw_FOUND)
        message(STATUS "Package information for osw not found")
    endif()
endif()

if(${USE_LTO})
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_COMPILE_OPTIONS_IPO -flto -ffat-lto-objects)
    endif()
endif()

add_subdirectory(sources)
add_library(${PROJECT_NAME} $<TARGET_OBJECTS:yaf_generic>)
target_link_libraries(${PROJECT_NAME} PUBLIC yaf_generic)

# Library installation configuration
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
