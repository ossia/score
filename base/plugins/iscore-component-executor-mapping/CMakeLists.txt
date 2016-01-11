cmake_minimum_required(VERSION 3.1)
project(iscore_component_executor_mapping LANGUAGES CXX)

iscore_common_setup()
set(Dependencies
      iscore_lib_base
      iscore_lib_process
      iscore_lib_device
      iscore_plugin_ossia
      iscore_plugin_curve
      iscore_plugin_deviceexplorer
      iscore_plugin_mapping)
foreach(target ${Dependencies})
  if(NOT TARGET ${target})
    return()
  endif()
endforeach()


set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Mapping/Component.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Mapping/Component.cpp"
)

iscore_generate_plugin_file(${PROJECT_NAME} ${HDRS})
add_library(${PROJECT_NAME}
    ${SRCS} ${HDRS}
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_plugin.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_plugin.hpp"
    )

target_link_libraries(${PROJECT_NAME} PRIVATE ${Dependencies})

setup_iscore_plugin(${PROJECT_NAME})
