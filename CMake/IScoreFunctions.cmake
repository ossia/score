include(IScoreTargetSetup)

### Generate files of commands ###
function(score_write_file FileName Content)
  if(EXISTS "${FileName}")
    file(READ "${FileName}" EXISTING_CONTENT)
    string(REGEX REPLACE ";" "\\\\;" EXISTING_CONTENT "${EXISTING_CONTENT}")
    if(NOT "${Content}" STREQUAL "${EXISTING_CONTENT}")
      file(WRITE "${FileName}" ${Content})
    endif()
  else()
    file(WRITE "${FileName}" ${Content})
  endif()
endfunction()

function(score_generate_command_list_file TheTarget Headers)
    # Initialize our lists
    set(commandNameList)
    set(commandFileList)

    # First look for the SCORE_COMMAND_DECL(...) ones
    foreach(sourceFile ${Headers})
        file(READ "${sourceFile}" fileContent)
        string(REGEX MATCHALL "SCORE_COMMAND_DECL\\([A-Za-z_0-9\,\:<>\r\n\t ]*\\(\\)[A-Za-z_0-9\,\"'\:<>\r\n\t ]*\\)"
               defaultCommands "${fileContent}")

        foreach(fileLine ${defaultCommands})
            string(REPLACE "\n" "" fileLine "${fileLine}")
            string(REPLACE "\r" "" fileLine "${fileLine}")
            string(STRIP ${fileLine} strippedLine)

            string(REPLACE "," ";" lineAsList ${strippedLine})
            list(GET lineAsList 1 commandName)
            string(STRIP ${commandName} strippedCommandName)
            list(APPEND commandNameList "${strippedCommandName}")
        endforeach()

        # If there are matching strings, we add the file to our include list
        list(LENGTH defaultCommands matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND commandFileList "${sourceFile}")
        endif()


        # Then look for the SCORE_COMMAND_DECL_T(...) ones
        string(REGEX MATCHALL "SCORE_COMMAND_DECL_T\\([A-Za-z_0-9\,\:<>\r\n\t ]*\\)"
               templateCommands "${fileContent}")
        foreach(fileLine ${templateCommands})
            string(REPLACE "\n" "" fileLine "${fileLine}")
            string(REPLACE "\r" "" fileLine "${fileLine}")
            string(STRIP ${fileLine} strippedLine)

            string(REPLACE "SCORE_COMMAND_DECL_T(" "" filtered1 ${strippedLine})
            string(REPLACE ")" "" commandName ${filtered1})
            string(STRIP ${commandName} strippedCommandName)

            list(APPEND commandNameList "${strippedCommandName}")
        endforeach()

        list(LENGTH templateCommands matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND commandFileList "${sourceFile}")
        endif()

    endforeach()

    # Generate a file with the list of includes
    set(finalCommandFileList)
    foreach(sourceFile ${commandFileList})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" strippedSourceFile ${sourceFile})
        set(finalCommandFileList "${finalCommandFileList}#include <${strippedSourceFile}>\n")
    endforeach()

    score_write_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands_files.hpp"
        "${finalCommandFileList}"
        )

    # Generate a file with the list of types
    string(REPLACE ";" ", \n" commaSeparatedCommandList "${commandNameList}")
    score_write_file(
         "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands.hpp"
         "${commaSeparatedCommandList}"
        )

endfunction()

function(score_write_static_plugins_header)
  set(SCORE_PLUGINS_FILE "${SCORE_ROOT_BINARY_DIR}/score_static_plugins.hpp")
  set(SCORE_PLUGINS_FILE_DATA "#pragma once\n")

  foreach(plugin ${SCORE_PLUGINS_LIST})
    message("Linking statically with score plugin : ${plugin}")
    string(APPEND SCORE_PLUGINS_FILE_DATA "#include <${plugin}.hpp>\n")
  endforeach()

  string(APPEND SCORE_PLUGINS_FILE_DATA "#include <score/plugins/PluginInstances.hpp>\n")
  string(APPEND SCORE_PLUGINS_FILE_DATA "void score_init_static_plugins() {\n")

  foreach(plugin ${SCORE_PLUGINS_LIST})
    string(APPEND SCORE_PLUGINS_FILE_DATA "{ static ${plugin} p\; score::staticPlugins().push_back(&p)\; }\n")
  endforeach()
  string(APPEND SCORE_PLUGINS_FILE_DATA "\n }\n")

  score_write_file("${SCORE_PLUGINS_FILE}" "${SCORE_PLUGINS_FILE_DATA}")
endfunction()

### Adds tests ###
function(setup_score_tests TestFolder)
  if(NOT DEPLOYMENT_BUILD AND NOT SCORE_STATIC_QT AND NOT IOS)
    add_subdirectory(${TestFolder})
  endif()
endfunction()
