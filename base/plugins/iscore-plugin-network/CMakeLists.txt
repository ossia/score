cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_network)

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

include_directories("${ISCORE_ROOT_SOURCE_DIR}")
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

add_library(${PROJECT_NAME} ${SRCS} ${ZEROCONF_SRCS})

target_link_libraries(${PROJECT_NAME}
                                    iscore_lib_base
                                    iscore_plugin_scenario # TODO :(
                                    Qt5::Core
                                    Qt5::Widgets
                                    Qt5::Network)

if(${KF5DNSSD_FOUND})
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_ZEROCONF)
    target_link_libraries(${PROJECT_NAME} KF5::DNSSD)
endif()

if(NOT APPLE AND NOT WIN32)
    target_link_libraries(${PROJECT_NAME} dns_sd)
endif()
if(USE_COTIRE)
        cotire(${PROJECT_NAME})
endif()
install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
