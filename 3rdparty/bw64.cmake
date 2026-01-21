# BW64 and ADM libraries for reading BWF files with Audio Definition Model metadata

add_library(bw64 INTERFACE)
target_include_directories(bw64 INTERFACE "${3RDPARTY_FOLDER}/libbw64/include")
if(0)
  
# libbw64 - header-only BW64 reader
set(BW64_EXAMPLES OFF CACHE INTERNAL "")
set(BW64_UNIT_TESTS OFF CACHE INTERNAL "")
set(BW64_PACKAGE_AND_INSTALL OFF CACHE INTERNAL "")
add_subdirectory("${3RDPARTY_FOLDER}/libbw64" "${CMAKE_CURRENT_BINARY_DIR}/libbw64")

# libadm - ADM metadata library (requires Boost)
set(ADM_EXAMPLES OFF CACHE INTERNAL "")
set(ADM_UNIT_TESTS OFF CACHE INTERNAL "")
set(ADM_PACKAGE_AND_INSTALL OFF CACHE INTERNAL "")
set(ADM_HIDE_INTERNAL_SYMBOLS ON CACHE INTERNAL "")
add_subdirectory("${3RDPARTY_FOLDER}/libadm" "${CMAKE_CURRENT_BINARY_DIR}/libadm")
endif()
