# Copyright (C) 2017 xent
# Project is distributed under the terms of the GNU General Public License v3.0

file(GLOB SOURCE_FILES "*.c")
add_library(yaf_generic OBJECT ${SOURCE_FILES})
target_include_directories(yaf_generic PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(yaf_generic PUBLIC xcore)

string(TOLOWER ${CMAKE_SYSTEM_NAME} OS_TYPE)
target_compile_definitions(yaf_generic PRIVATE
      -DOS_TYPE=${OS_TYPE}
      -DCONFIG_DEBUG=${YAF_CONFIG_DEBUG}
      -DCONFIG_NAME_LENGTH=${FS_NAME_LENGTH}
      -DCONFIG_SECTOR_SIZE=${YAF_CONFIG_SECTOR_SIZE})
if(${YAF_CONFIG_POOLS})
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_POOLS)
endif()
if(${YAF_CONFIG_THREADS})
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_THREADS)
endif()
if(${YAF_CONFIG_TIME})
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_TIME)
endif()
if(${YAF_CONFIG_UNICODE})
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_UNICODE)
endif()
if(${YAF_CONFIG_WRITE})
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_WRITE)
endif()
