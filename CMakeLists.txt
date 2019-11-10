cmake_minimum_required(VERSION 3.2)
project(flittermouse CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cage/cmake")
include(cage_all)
cage_default_init()
cage_directory_link("${CMAKE_CURRENT_SOURCE_DIR}/data/flittermouse" "${CMAKE_BINARY_DIR}/result/data/flittermouse")

add_subdirectory(externals/cage)

file(GLOB_RECURSE flittermouse-sources "sources/*")
add_executable(flittermouse ${flittermouse-sources})
target_include_directories(flittermouse PRIVATE "externals/dualmc/include" "externals/xatlas")
target_sources(flittermouse PRIVATE "externals/dualmc/include/dualmc.h" "externals/xatlas/xatlas.h" "externals/xatlas/xatlas.cpp")
set_source_files_properties("externals/xatlas/xatlas.cpp" PROPERTIES COMPILE_DEFINITIONS "XA_MULTITHREADED=0")
target_link_libraries(flittermouse cage-engine)
cage_ide_category(flittermouse flittermouse)
cage_ide_sort_files(flittermouse)
cage_ide_working_dir_in_place(flittermouse)
