cmake_minimum_required(VERSION 3.0)
enable_testing()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)

add_subdirectory(iscore-lib-state)
add_subdirectory(iscore-lib-process)
add_subdirectory(iscore-lib-device)

add_subdirectory(iscore-plugin-pluginsettings)
add_subdirectory(iscore-plugin-inspector)
add_subdirectory(iscore-plugin-deviceexplorer)
add_subdirectory(iscore-plugin-curve)
add_subdirectory(iscore-plugin-automation)
add_subdirectory(iscore-plugin-scenario)
add_subdirectory(iscore-plugin-loop)
add_subdirectory(iscore-plugin-ossia)
add_subdirectory(iscore-plugin-cohesion)
add_subdirectory(iscore-plugin-audio)

add_subdirectory(iscore-plugin-network)
add_subdirectory(iscore-plugin-js)
add_subdirectory(iscore-plugin-mapping)

add_subdirectory(iscore-plugin-temporalautomatas)

if(ISCORE_USE_DEV_PLUGINS)
add_subdirectory(iscore-plugin-csp)

add_subdirectory(iscore-plugin-space)
add_subdirectory(iscore-plugin-library)
endif()
