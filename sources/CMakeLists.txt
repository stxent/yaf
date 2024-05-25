# Copyright (C) 2017 xent
# Project is distributed under the terms of the MIT License

file(GLOB SOURCE_FILES "*.c")
add_library(yaf_generic OBJECT ${SOURCE_FILES})
set_target_properties(yaf_generic PROPERTIES PUBLIC_HEADER
        "${PROJECT_SOURCE_DIR}/include/yaf/fat32.h;${PROJECT_SOURCE_DIR}/include/yaf/utils.h")
target_include_directories(yaf_generic PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(yaf_generic PUBLIC xcore)
install(TARGETS yaf_generic EXPORT yaf-targets PUBLIC_HEADER DESTINATION include/yaf)

target_compile_definitions(yaf_generic PRIVATE
        -DCONFIG_DEBUG=${YAF_DEBUG}
        -DCONFIG_SECTOR_SIZE=${YAF_SECTOR_SIZE}
)
if(YAF_THREADS)
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_THREADS)
endif()
if(YAF_UNICODE)
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_UNICODE)
endif()
if(YAF_WRITE)
    target_compile_definitions(yaf_generic PRIVATE -DCONFIG_WRITE)
endif()
if(BUILD_TESTING)
    target_compile_options(yaf_generic PRIVATE --coverage)
endif()
