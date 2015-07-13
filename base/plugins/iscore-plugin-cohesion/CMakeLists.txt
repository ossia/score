cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_cohesion)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
cmake_policy(SET CMP0042 NEW)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Xml PrintSupport Test)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build iscore cohesion plugin")
    return()
endif()


include_directories(${ISCORE_ROOT_SOURCE_DIR})
# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${DEVICEEXPLORER_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../iscore-plugin-scenario/source")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../iscore-plugin-curve")
# TODO put this in CMake interface of iscore_lib

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/CreateCurveFromStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/CreateCurvesFromAddresses.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/IScoreCohesion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/IScoreCohesionControl.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_deviceexplorer
                        Jamoma::Foundation Jamoma::Modular Jamoma::Score)
if(ISCORE_COTIRE)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(${PROJECT_NAME} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
    cotire(${PROJECT_NAME})
endif()

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        COMPONENT DynamicRuntime)
