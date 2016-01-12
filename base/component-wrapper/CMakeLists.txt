cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(iscore_merged_components LANGUAGES CXX)

add_library(iscore_merged_components)



set_property(TARGET iscore_merged_components APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
           set_property(TARGET iscore_merged_components APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")


foreach(Name ${ISCORE_GLOBAL_COMPONENTS_LIST})
  message("${${Name}_SOURCE_DIR}")
  get_property(CompSrcs TARGET ${Name} PROPERTY INTERFACE_SOURCES)
  get_property(CompIncl TARGET ${Name} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
  get_property(CompLibs TARGET ${Name} PROPERTY INTERFACE_LINK_LIBRARIES)
  target_sources(iscore_merged_components PUBLIC ${CompSrcs})
  target_include_directories(iscore_merged_components PUBLIC ${CompIncl})
  target_link_libraries(iscore_merged_components PUBLIC ${CompLibs})

  string(TOUPPER "${Name}" UPPERCASE_PLUGIN_NAME)
  set_property(TARGET iscore_merged_components APPEND
               PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${${Name}_SOURCE_DIR}")
  set_property(TARGET iscore_merged_components APPEND
               PROPERTY INTERFACE_COMPILE_DEFINITIONS "${UPPERCASE_PLUGIN_NAME}")

endforeach()

target_include_directories(iscore_merged_components INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
setup_iscore_plugin(iscore_merged_components)
