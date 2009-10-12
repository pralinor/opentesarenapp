cmake_minimum_required (VERSION 2.6 FATAL_ERROR)
project (opentesarenapp)

# boost
link_directories ( ${Boost_LIBRARY_DIRS} )

# recurse into src dir
add_subdirectory (src)

include_directories (src/utils src/dataextraction ${Boost_INCLUDE_DIRS})

add_executable (opentesarenapp src/main.cpp)

target_link_libraries (opentesarenapp utils dataextraction ${Boost_LIBRARIES})
