# Copyright (C) 2017 xent
# Project is distributed under the terms of the GNU General Public License v3.0

file(GLOB SOURCE_FILES "*.c")
add_library(yaf_generic OBJECT ${SOURCE_FILES})
target_include_directories(yaf_generic PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(yaf_generic PUBLIC xcore)
