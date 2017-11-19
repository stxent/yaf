#Copyright (C) 2017 xent
#Project is distributed under the terms of the GNU General Public License v3.0

project(yaf C)
cmake_minimum_required(VERSION 3.6)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build: Debug Release." FORCE)
endif()

option(USE_LTO "Enable Link Time Optimization." OFF)

set(PATH_XCORE "${PROJECT_SOURCE_DIR}/../xcore" CACHE STRING "Path to the XCORE library.")
set(PATH_OSW "${PROJECT_SOURCE_DIR}/../osw" CACHE STRING "Path to the OSW library.")

#Library configuration
option(CONFIG_FLAG_POOLS "Build driver with statically allocated buffers." ON)
option(CONFIG_FLAG_THREADS "Enable multithreading." ON)
option(CONFIG_FLAG_TIME "Build driver with time functions enabled." ON)
option(CONFIG_FLAG_UNICODE "Enable support for Unicode characters." ON)
option(CONFIG_FLAG_WRITE "Enable write functions." ON)
option(CONFIG_FLAG_DEBUG "Enable debug output." OFF)

set(CONFIG_DEBUG_LEVEL 0 CACHE STRING "Debug level.")
set(CONFIG_NAME_LENGTH 64 CACHE STRING "Maximum length of entry name in bytes.")
set(CONFIG_SECTOR_SIZE 512 CACHE STRING "Size of a filesystem sector may be 512, 1024, 2048 or 4096 bytes.")

#Convert configuration into compiler flags
get_cmake_property(CONFIG_DATA VARIABLES)

set(FLAGS_LIBRARY "")
foreach(ENTRY ${CONFIG_DATA})
    string(FIND "${ENTRY}" "CONFIG_" ENTRY_IS_CONFIG)
    if(${ENTRY_IS_CONFIG} EQUAL 0)
        string(FIND "${ENTRY}" "CONFIG_FLAG_" ENTRY_IS_CONFIG_FLAG)
        if(${ENTRY_IS_CONFIG_FLAG} EQUAL 0)
            if(${${ENTRY}})
                set(FLAGS_LIBRARY "${FLAGS_LIBRARY} -D${ENTRY}")
            endif()
        else()
            set(FLAGS_LIBRARY "${FLAGS_LIBRARY} -D${ENTRY}=${${ENTRY}}")
        endif()
    endif()
endforeach()

#Default compiler flags
string(TOLOWER ${CMAKE_SYSTEM_NAME} OS_TYPE)
set(FLAGS_DEFAULT "-fdata-sections -ffunction-sections -fmessage-length=0 -Wall -Wextra -Wshadow -pedantic")
set(FLAGS_PLATFORM "-DOS_TYPE=${OS_TYPE}")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS_DEFAULT} ${FLAGS_CPU} ${FLAGS_PLATFORM} ${FLAGS_LIBRARY}")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

if(${USE_LTO})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto -ffat-lto-objects")
endif()

include_directories("${PATH_XCORE}/include")
include_directories("${PATH_OSW}/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

#Gather files for the library
include("${PROJECT_SOURCE_DIR}/libyaf/CMakeLists.txt")

add_library(${PROJECT_NAME} ${LIBRARY_SOURCES})
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION lib)
