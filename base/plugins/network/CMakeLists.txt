cmake_minimum_required(VERSION 3.0)
project(network_plugin)

if(ANDROID)
    message("TODO FIXME ON ANDROID")
    return()
endif()

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Network Xml Test)
find_package(KF5DNSSD NO_MODULE)

set(CMAKE_AUTOMOC ON)

# Files & main target
include_directories(.)

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")

file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")


foreach(item ${SRCS})
  if(${item} MATCHES "Zeroconf")
    list(REMOVE_ITEM SRCS ${item})
  endif()
endforeach()

if(${KF5DNSSD_FOUND})
    message("Will use ZeroConf")
    file(GLOB_RECURSE ZEROCONF_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Zeroconf/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Zeroconf/*.hpp")

    if(WIN32)
        include_directories(/f/kdnssd/usr/include)
        include_directories("f:/kdnssd/usr/include")
    endif()
endif()

add_library(networkplugin ${SRCS} ${ZEROCONF_SRCS})

target_link_libraries(networkplugin
                                    iscore_lib
                                    scenario_process # TODO :(
                                    Qt5::Core
                                    Qt5::Widgets
                                    Qt5::Network)

if(${KF5DNSSD_FOUND})
    target_compile_definitions(networkplugin PUBLIC USE_ZEROCONF)
    target_link_libraries(networkplugin KF5::DNSSD)
endif()

if(NOT APPLE AND NOT WIN32)
    target_link_libraries(networkplugin dns_sd)
endif()
cotire(networkplugin)
install(TARGETS networkplugin
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
