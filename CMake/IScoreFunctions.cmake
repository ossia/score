function(iscore_cotire_pre TheTarget)
    set_property(TARGET ${TheTarget} PROPERTY CXX_STANDARD 14)
if(ISCORE_COTIRE)
    set_property(TARGET ${TheTarget} PROPERTY COTIRE_ADD_UNITY_BUILD FALSE)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(${TheTarget} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()

    # FIXME on windows
    set_target_properties(${TheTarget} PROPERTIES
                          COTIRE_PREFIX_HEADER_IGNORE_PATH "${COTIRE_PREFIX_HEADER_IGNORE_PATH};/usr/include/boost/preprocessor/")

    if(NOT ${TheTarget} STREQUAL "iscore_lib_base")
        # We reuse the same prefix header

        get_target_property(ISCORE_COMMON_PREFIX_HEADER iscore_lib_base COTIRE_CXX_PREFIX_HEADER)
        set_target_properties(${TheTarget} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "${ISCORE_COMMON_PREFIX_HEADER}")
    endif()

endif()
endfunction()

function(iscore_cotire_post TheTarget)
if(ISCORE_COTIRE)
   cotire(${TheTarget})
endif()
endfunction()

### Call at the beginning of a plug-in cmakelists ###
function(iscore_common_setup)
  enable_testing()
  include_directories("${ISCORE_ROOT_SOURCE_DIR}")
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTOMOC ON)
  cmake_policy(SET CMP0020 NEW)
  cmake_policy(SET CMP0042 NEW)
endfunction()


### Initialization of most common stuff ###
function(setup_iscore_common_features TheTarget)
  iscore_cotire_pre(${TheTarget})

  if(ENABLE_LTO)
    set_property(TARGET ${TheTarget}
                 PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
  endif()

  if(ISCORE_STATIC_PLUGINS)
    target_compile_definitions(${TheTarget}
                               PUBLIC ISCORE_STATIC_PLUGINS)
  endif()

  target_include_directories(${TheTarget} INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
  
  generate_export_header(${TheTarget})
endfunction()


### Initialization of common stuff ###
function(setup_iscore_common_lib_features TheTarget)
  setup_iscore_common_features("${TheTarget}")

  string(TOUPPER "${TheTarget}" UPPERCASE_PLUGIN_NAME)
  set_property(TARGET ${TheTarget} APPEND
               PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
  set_property(TARGET ${TheTarget} APPEND
               PROPERTY INTERFACE_COMPILE_DEFINITIONS "${UPPERCASE_PLUGIN_NAME}")
endfunction()


### Call with a library target ###
function(setup_iscore_library PluginName)
  setup_iscore_common_lib_features("${PluginName}")

  set(ISCORE_LIBRARIES_LIST ${ISCORE_LIBRARIES_LIST} "${PluginName}" CACHE INTERNAL "List of libraries")

  if(ISCORE_BUILD_FOR_PACKAGE_MANAGER)
  install(TARGETS "${PluginName}"
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
          CONFIGURATIONS DynamicRelease)
  else()
  install(TARGETS "${PluginName}"
          LIBRARY DESTINATION .
          ARCHIVE DESTINATION static_lib/
          CONFIGURATIONS DynamicRelease)
  endif()
  iscore_cotire_post("${PluginName}")
endfunction()


### Call with a plug-in target ###
function(setup_iscore_plugin PluginName)
  setup_iscore_common_lib_features("${PluginName}")

  set(ISCORE_PLUGINS_LIST ${ISCORE_PLUGINS_LIST} "${PluginName}" CACHE INTERNAL "List of plugins")

  if(ISCORE_BUILD_FOR_PACKAGE_MANAGER)
  install(TARGETS "${PluginName}"
          LIBRARY DESTINATION lib/i-score
          ARCHIVE DESTINATION lib/i-score
          CONFIGURATIONS DynamicRelease)
  else()
  install(TARGETS "${PluginName}"
          LIBRARY DESTINATION plugins
          ARCHIVE DESTINATION static_plugins
          CONFIGURATIONS DynamicRelease)
  endif()
  iscore_cotire_post("${PluginName}")
endfunction()


### Generate files of commands ###
function(iscore_generate_command_list_file TheTarget Headers)
    # Initialize our lists
    set(commandNameList)
    set(commandFileList)

    # First look for the ISCORE_COMMAND_DECL(...) ones
    foreach(sourceFile ${Headers})
        file(STRINGS "${sourceFile}" fileContent REGEX "ISCORE_COMMAND_DECL\\(")

        # If there are matching strings, we add the file to our include list
        list(LENGTH fileContent matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND commandFileList "${sourceFile}")
        endif()

        foreach(fileLine ${fileContent})
            string(STRIP ${fileLine} strippedLine)
            string(REPLACE "," ";" lineAsList ${strippedLine})
            list(GET lineAsList 1 commandName)
            string(STRIP ${commandName} strippedCommandName)
            list(APPEND commandNameList "${strippedCommandName}")
        endforeach()
    endforeach()

    # Then look for the ISCORE_COMMAND_DECL_T(...) ones
    foreach(sourceFile ${Headers})
        file(STRINGS ${sourceFile} fileContent REGEX "ISCORE_COMMAND_DECL_T\\(")

        list(LENGTH fileContent matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND commandFileList "${sourceFile}")
        endif()

        foreach(fileLine ${fileContent})
            string(STRIP ${fileLine} strippedLine)
            string(REPLACE "ISCORE_COMMAND_DECL_T(" "" filtered1 ${strippedLine})
            string(REPLACE ")" "" strippedCommandName ${filtered1})
            list(APPEND commandNameList "${strippedCommandName}")
        endforeach()
    endforeach()

    # Generate a file with the list of includes
    set(finalCommandFileList)
    foreach(sourceFile ${commandFileList})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" strippedSourceFile ${sourceFile})
        set(finalCommandFileList "${finalCommandFileList}#include <${strippedSourceFile}>\n")
    endforeach()

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands_files.hpp" ${finalCommandFileList})

    # Generate a file with the list of types
    string(REPLACE ";" ", \n" commaSeparatedCommandList "${commandNameList}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands.hpp" ${commaSeparatedCommandList})
endfunction()


### Adds tests ###
function(setup_iscore_tests TestFolder)
  if(NOT DEPLOYMENT_BUILD)
    add_subdirectory(${TestFolder})
  endif()
endfunction()
