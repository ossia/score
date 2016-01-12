cmake_minimum_required(VERSION 3.0)
enable_testing()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)

message("${ISCORE_PLUGINS_TO_BUILD}")
# ISCORE_PLUGINS_TO_BUILD is defined in the configuration.
foreach(plugin ${ISCORE_PLUGINS_TO_BUILD})
  add_subdirectory(${plugin})
endforeach()
