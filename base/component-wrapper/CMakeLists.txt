cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(score_merged_components LANGUAGES CXX)

# First get the list of abstract factories that will be involved
set(CleanedAbstractFactoryList)

foreach(Name ${SCORE_GLOBAL_COMPONENTS_LIST})

  get_property(AbstractFactory_List TARGET ${Name} PROPERTY INTERFACE_SCORE_COMPONENT_AbstractFactory_List)
  set(CleanedAbstractFactoryList ${CleanedAbstractFactoryList} ${AbstractFactory_List})
endforeach()

# No factories -> nothing to build
list(LENGTH CleanedAbstractFactoryList NumberOfFactories)
if(${NumberOfFactories} EQUAL 0)
  return()
endif()

list(REMOVE_DUPLICATES CleanedAbstractFactoryList)

# For each abstract component instantiate
# all the concrete components found

set(FactoryFiles)
set(FactoryCode)
list(LENGTH SCORE_GLOBAL_COMPONENTS_LIST LengthComponents)
math(EXPR NumComponents ${LengthComponents}-1)

foreach(AbstractClassName ${CleanedAbstractFactoryList})
  set(CurrentCode "FW<${AbstractClassName}")

  # For each component
  foreach(component ${SCORE_GLOBAL_COMPONENTS_LIST})

    get_property(AbstractFactory_List TARGET ${component} PROPERTY INTERFACE_SCORE_COMPONENT_AbstractFactory_List)
    get_property(ConcreteFactory_List TARGET ${component} PROPERTY INTERFACE_SCORE_COMPONENT_ConcreteFactory_List)
    get_property(ConcreteFactory_Files TARGET ${component} PROPERTY INTERFACE_SCORE_COMPONENT_Factory_Files)

    ## Includes ##
    # Generate a file with the list of includes
    foreach(header ${ConcreteFactory_Files})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" strippedheader ${header})
        set(FactoryFiles "${FactoryFiles}#include <${strippedheader}>\n")
    endforeach()

    ## Factories ##
    list(LENGTH AbstractFactory_List LengthFactories)
    math(EXPR NumFactories ${LengthFactories}-1)

    # For each factory
    foreach(factory_i RANGE ${NumFactories})
      # Get the corresponding abstract factory
      list(GET AbstractFactory_List ${factory_i} CurrentAbstractFactory)

      # If it is the same that the one we are iterating unto
      if(${AbstractClassName} STREQUAL ${CurrentAbstractFactory})
        # Add the current concrete factory text to the code
        list(GET ConcreteFactory_List ${factory_i} CurrentFactory)
        set(CurrentCode "${CurrentCode},\n    ${CurrentFactory}")
      endif()
    endforeach()
  endforeach()

  set(CurrentCode "${CurrentCode}>\n")
  set(FactoryCode "${FactoryCode}${CurrentCode}")
endforeach()

# Create the plug-in file
set(PLUGIN_NAME score_merged_components)
configure_file(
    "${SCORE_ROOT_SOURCE_DIR}/CMake/Components/score_component_plugin.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/score_merged_components_plugin.hpp")
configure_file(
    "${SCORE_ROOT_SOURCE_DIR}/CMake/Components/score_component_plugin.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/score_merged_components_plugin.cpp")


# Create the library
add_library(score_merged_components
  "${CMAKE_CURRENT_BINARY_DIR}/score_merged_components_plugin.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/score_merged_components_plugin.cpp"
)

set_property(TARGET score_merged_components APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET score_merged_components APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")

# Pass to the library all the properties of all the components
foreach(Name ${SCORE_GLOBAL_COMPONENTS_LIST})
  get_property(CompSrcs TARGET ${Name} PROPERTY INTERFACE_SOURCES)
  get_property(CompIncl TARGET ${Name} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
  get_property(CompLibs TARGET ${Name} PROPERTY INTERFACE_LINK_LIBRARIES)

  if(SCORE_STATIC_PLUGINS)
    target_sources(score_merged_components INTERFACE ${CompSrcs})
  else()
    target_sources(score_merged_components PUBLIC ${CompSrcs})
  endif()
  target_include_directories(score_merged_components PUBLIC ${CompIncl})
  target_link_libraries(score_merged_components PUBLIC ${CompLibs})

  string(TOUPPER "${Name}" UPPERCASE_PLUGIN_NAME)
  set_property(TARGET score_merged_components APPEND
               PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${${Name}_SOURCE_DIR}")
  set_property(TARGET score_merged_components APPEND
               PROPERTY INTERFACE_COMPILE_DEFINITIONS "${UPPERCASE_PLUGIN_NAME}")
endforeach()

target_include_directories(score_merged_components INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
setup_score_plugin(score_merged_components)


if(SCORE_STATIC_PLUGINS)
  ## note : we have to prevent from linking score with the components in case of a static build ##
  ## Interface sources have to be defined before cotire for unity build.
  ## For normal builds we add non-interface sources.
  foreach(Name ${SCORE_GLOBAL_COMPONENTS_LIST})
    get_property(CompSrcs TARGET ${Name} PROPERTY INTERFACE_SOURCES)

    target_sources(score_merged_components PRIVATE ${CompSrcs})
  endforeach()
endif()
