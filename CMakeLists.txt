cmake_minimum_required (VERSION 2.6)
project (opentesarenapp)

# recurse into src dir
add_subdirectory (src)

include_directories (src/utils src/dataextraction)

add_executable (opentesarenapp src/main.cpp)

target_link_libraries (opentesarenapp utils dataextraction)
