cmake_minimum_required(VERSION 2.8)
project(iscore_lib_state)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE LIBRARY_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/State/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp")

add_library(${PROJECT_NAME} ${LIBRARY_SRCS})
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Widgets)

set_property(TARGET ${PROJECT_NAME} APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

if(USE_COTIRE)
        cotire(${PROJECT_NAME})
endif()
install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION static_lib/)

add_subdirectory(Tests)
