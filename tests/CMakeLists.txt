# Copyright (C) 2020 xent
# Project is distributed under the terms of the MIT License

include(FindPkgConfig)

find_package(Threads REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

file(GLOB_RECURSE SHARED_SOURCES "shared/*.c")
add_library(yaf_shared ${SHARED_SOURCES})
target_compile_options(yaf_shared PUBLIC --coverage)
target_include_directories(yaf_shared PUBLIC "shared")
target_link_libraries(yaf_shared PUBLIC xcore yaf check gcov Threads::Threads)

string(TOLOWER ${CMAKE_SYSTEM_NAME} OS_TYPE)
target_compile_definitions(yaf_shared PUBLIC
        -DOS_TYPE=${OS_TYPE}
        -DCONFIG_NAME_LENGTH=${FS_NAME_LENGTH}
        -DCONFIG_SECTOR_SIZE=${YAF_CONFIG_SECTOR_SIZE})
if(YAF_CONFIG_THREADS)
    target_compile_definitions(yaf_shared PUBLIC -DCONFIG_THREADS)
    target_link_libraries(yaf_shared PUBLIC osw)
endif()
if(YAF_CONFIG_UNICODE)
    target_compile_definitions(yaf_shared PUBLIC -DCONFIG_UNICODE)
endif()
if(YAF_CONFIG_WRITE)
    target_compile_definitions(yaf_shared PUBLIC -DCONFIG_WRITE)
endif()

# Generate test executables

set(TEST_LIST
        context_failures
        dir_read
        handle_failures
        handle_usage
        make_fs
        node_read
        node_read_failures
        pool_failures
        system_funcs
        virtual_mem)

if(YAF_CONFIG_THREADS)
    list(APPEND TEST_LIST mutex_failures)
endif()

if(YAF_CONFIG_UNICODE)
    list(APPEND TEST_LIST unicode_failures)
else()
    list(APPEND TEST_LIST unicode_unsupported)
endif()

if(YAF_CONFIG_WRITE)
    list(APPEND TEST_LIST dir_write)
    list(APPEND TEST_LIST dir_write_failures)
    list(APPEND TEST_LIST handle_funcs)
    list(APPEND TEST_LIST node_write)
    list(APPEND TEST_LIST node_write_failures)
endif()

if(YAF_CONFIG_UNICODE AND YAF_CONFIG_WRITE)
    list(APPEND TEST_LIST unicode_write)
    list(APPEND TEST_LIST unicode_write_failures)
endif()

foreach(TEST_NAME ${TEST_LIST})
    file(GLOB_RECURSE TEST_SOURCES "${TEST_NAME}/*.c")
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    add_test(${TEST_NAME} ${TEST_NAME})
    target_link_libraries(${TEST_NAME} PRIVATE yaf_shared)
endforeach()
