cmake_minimum_required(VERSION 3.1)
project(iscore_component_executor_loop LANGUAGES CXX)

iscore_common_setup()
set(DEPS
      iscore_lib_base
      iscore_lib_process
      iscore_lib_device
      iscore_plugin_ossia
      iscore_plugin_deviceexplorer
      iscore_plugin_scenario
      iscore_plugin_loop
      )

set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Loop/Component.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Loop/Component.cpp"
)

iscore_add_component("${PROJECT_NAME}" "${SRCS}" "${HDRS}" "${DEPS}")
