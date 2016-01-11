cmake_minimum_required(VERSION 2.8)
project(iscore_component_executor_automation)

iscore_common_setup()
set(Dependencies iscore_plugin_ossia iscore_plugin_automation)
if(NOT TARGET iscore_plugin_ossia)
  return()
endif()
if(NOT TARGET iscore_plugin_automation)
  return()
endif()


set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Automation/Component.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Automation/Component.cpp"
)

iscore_generate_plugin_file(${PROJECT_NAME} ${HDRS})
add_library(${PROJECT_NAME}
    ${SRCS} ${HDRS}
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_plugin.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_plugin.hpp"
    )

target_link_libraries(${PROJECT_NAME} ${Dependencies})

setup_iscore_plugin(${PROJECT_NAME})
