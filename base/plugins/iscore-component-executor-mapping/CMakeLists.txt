cmake_minimum_required(VERSION 3.1)
project(iscore_component_executor_mapping LANGUAGES CXX)

iscore_common_setup()
set(DEPS
      iscore_lib_base
      iscore_lib_process
      iscore_lib_device
      iscore_plugin_ossia
      iscore_plugin_curve
      iscore_plugin_deviceexplorer
      iscore_plugin_mapping)

set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Mapping/Component.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Executor/Mapping/Component.cpp"
)

iscore_add_component("${PROJECT_NAME}" "${SRCS}" "${HDRS}" "${DEPS}")
