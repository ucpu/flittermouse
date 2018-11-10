cmake_minimum_required(VERSION 3.2)
project(flittermouse CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cage/cmake")
include(cage_all)
cage_default_init()
cage_directory_link("${CMAKE_CURRENT_SOURCE_DIR}/data/flittermouse" "${CMAKE_BINARY_DIR}/result/data/flittermouse")

add_subdirectory(externals/cage)

file(GLOB_RECURSE flittermouse-sources "sources/*")
add_executable(flittermouse ${flittermouse-sources})
target_link_libraries(flittermouse cage-client)
cage_ide_category(flittermouse flittermouse)
cage_ide_sort_files(flittermouse)
cage_ide_working_dir_in_place(flittermouse)
