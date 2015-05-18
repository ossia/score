cmake_minimum_required(VERSION 3.0)
project(iscore_lib_base)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets )

if(NOT Boost_FOUND)
        message("i-score requires boost to build")
        return()
endif()

# Files & main target
include_directories(.)

file(GLOB_RECURSE SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iscore/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iscore/*.cpp)

add_library(iscore_lib_base ${SRCS})
target_link_libraries(iscore_lib_base Qt5::Core Qt5::Widgets)

if(USE_COTIRE)
        cotire(iscore_lib_base)
endif()
install(TARGETS iscore_lib_base
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
