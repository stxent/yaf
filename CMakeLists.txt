# Copyright (C) 2017 xent
# Project is distributed under the terms of the GNU General Public License v3.0

project(yaf C)
cmake_minimum_required(VERSION 3.6)
cmake_policy(SET CMP0054 NEW)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build: Debug Release." FORCE)
endif()

option(USE_LTO "Enable Link Time Optimization." OFF)

set(PATH_XCORE "${PROJECT_SOURCE_DIR}/../xcore" CACHE STRING "Path to the XCORE library.")
set(PATH_OSW "${PROJECT_SOURCE_DIR}/../osw" CACHE STRING "Path to the OSW library.")

# Library configuration
option(CONFIG_FLAG_POOLS "Build driver with statically allocated buffers." ON)
option(CONFIG_FLAG_THREADS "Enable multithreading." ON)
option(CONFIG_FLAG_TIME "Build driver with time functions enabled." ON)
option(CONFIG_FLAG_UNICODE "Enable support for Unicode characters." ON)
option(CONFIG_FLAG_WRITE "Enable write functions." ON)
option(CONFIG_FLAG_DEBUG "Enable debug output." OFF)

set(CONFIG_DEBUG_LEVEL 0 CACHE STRING "Debug level.")
set(CONFIG_NAME_LENGTH 64 CACHE STRING "Maximum length of entry name in bytes.")
set(CONFIG_SECTOR_SIZE 512 CACHE STRING "Size of a filesystem sector may be 512, 1024, 2048 or 4096 bytes.")

# Convert configuration into compiler flags
get_cmake_property(CONFIG_DATA VARIABLES)

foreach(_entry ${CONFIG_DATA})
    string(FIND "${_entry}" "CONFIG_" _entry_is_config)
    if(${_entry_is_config} EQUAL 0)
        string(FIND "${_entry}" "CONFIG_FLAG_" _entry_is_config_flag)
        if(${_entry_is_config_flag} EQUAL 0)
            if(${${_entry}})
                add_definitions(-D${_entry})
            endif()
        else()
            add_definitions(-D${_entry}=${${_entry}})
        endif()
    endif()
endforeach()

# Default compiler flags
string(TOLOWER ${CMAKE_SYSTEM_NAME} OS_TYPE)
set(FLAGS_DEFAULT "-fdata-sections -ffunction-sections -Wall -Wextra -Wshadow -pedantic")
set(FLAGS_PLATFORM "-DOS_TYPE=${OS_TYPE}")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS_DEFAULT} ${FLAGS_CPU} ${FLAGS_PLATFORM} ${FLAGS_LIBRARY}")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

if(${USE_LTO})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto -ffat-lto-objects")
endif()

include_directories("${PATH_XCORE}/include")
include_directories("${PATH_OSW}/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

# Gather files for the library
add_subdirectory(sources)
add_library(${PROJECT_NAME} $<TARGET_OBJECTS:yaf_generic>)
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION lib)
