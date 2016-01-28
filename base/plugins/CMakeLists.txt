cmake_minimum_required(VERSION 3.0)
enable_testing()

# ISCORE_PLUGINS_TO_BUILD is defined in the configuration.
foreach(plugin ${ISCORE_PLUGINS_TO_BUILD})
  add_subdirectory(${plugin})
endforeach()
