cmake_minimum_required(VERSION 2.8)
PROJECT(RepartitionClientTest)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/modules")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")

include_directories(${API_DEPENDENCIES_FOLDER}/oscpack)
include_directories(${API_DEPENDENCIES_FOLDER}/bonjour-qt/headers)
include_directories(${API_HEADERS_FOLDER}/Repartition)

find_package(Qt5Core)
find_package(Qt5Network)

set(SRC
${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# TODO make it better
set(HEADERS
${API_DEPENDENCIES_FOLDER}/bonjour-qt/headers/bonjourserviceresolver.h
${API_DEPENDENCIES_FOLDER}/bonjour-qt/headers/bonjourservicebrowser.h
${API_HEADERS_FOLDER}/Repartition/session/ZeroConfClient.h
${API_HEADERS_FOLDER}/Repartition/session/ZeroConfClientThread.h)



add_executable(Client ${SRC} ${HEADERS})

target_link_libraries(Client oscpack dns_sd pthread)
target_link_libraries(Client Qt5::Core Qt5::Network)
