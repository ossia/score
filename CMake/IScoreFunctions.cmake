include(IScoreTargetSetup)
### Component plug-ins ###

# TODO parcourir les fichiers pour trouver les FactoryFamily.
# Faire un premier parcours ou elles sont enregistrées,
# les stocker dans une map, et rajouter en parallèle les objets correspondants.
# Puis en fonction de la configuration (statique, dynamique) et du choix d'objets
# qu'on veut avoir dans le binaire, soit générer plein de sous-projets à la fin,
# soit générer un gros projet qui contient tout ? attention si des plug-ins ont
# des dépendances sur d'autres plug-ins... (générer les deps automatiquement)

function(score_add_component Name Sources Headers Dependencies Version Uuid)
  foreach(target ${Dependencies})
    if(NOT TARGET ${target})
      return()
    endif()
  endforeach()

  set(SCORE_GLOBAL_COMPONENTS_LIST
        ${SCORE_GLOBAL_COMPONENTS_LIST} ${Name}
        CACHE INTERNAL "List of components")

  score_parse_components(${Name} ${Headers})

  if(SCORE_MERGE_COMPONENTS)
    # We create a fake library that will be instantiated in component-wrapper
    add_library(${Name} INTERFACE)
    target_include_directories(${Name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_sources(${Name} INTERFACE ${Sources} ${Headers})
    target_link_libraries(${Name} INTERFACE ${Dependencies})

    get_property(ComponentAbstractFactoryList GLOBAL
                 PROPERTY SCORE_${Name}_AbstractFactoryList)
    get_property(ComponentFactoryList GLOBAL
                 PROPERTY SCORE_${Name}_ConcreteFactoryList)
    get_property(ComponentFactoryFileList GLOBAL
                 PROPERTY SCORE_${Name}_FactoryFileList)

    set_target_properties(${Name} PROPERTIES
      INTERFACE_SCORE_COMPONENT_AbstractFactory_List ${ComponentAbstractFactoryList}
      INTERFACE_SCORE_COMPONENT_ConcreteFactory_List ${ComponentFactoryList}
      INTERFACE_SCORE_COMPONENT_Factory_Files ${ComponentFactoryFileList}
      INTERFACE_SCORE_COMPONENT_Version ${Version}
      INTERFACE_SCORE_COMPONENT_Key ${Uuid}
      )
  else()
    # concrete library
    score_generate_plugin_file(${Name} ${Headers} ${Version} ${Uuid})
    add_library(${Name}
        ${Sources} ${Headers}
        "${CMAKE_CURRENT_BINARY_DIR}/${Name}_plugin.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/${Name}_plugin.hpp"
        )

    target_link_libraries(${Name} PUBLIC ${Dependencies})

    setup_score_plugin(${Name})
  endif()
endfunction()

function(score_parse_components TargetName Headers)
  # Initialize our lists
  set(ComponentAbstractFactoryList)
  set(ComponentFactoryList)
  set(ComponentFactoryFileList)

  # First look for the SCORE_CONCRETE_COMPONENT_FACTORY(...) ones
  foreach(header ${Headers})
      file(READ "${header}" fileContent)
      string(REGEX MATCHALL "SCORE_CONCRETE_COMPONENT_FACTORY\\([A-Za-z_0-9\,\:\r\n\t ]*\\)" fileContent "${fileContent}")

      foreach(fileLine ${fileContent})
          string(REPLACE "\n" "" fileLine "${fileLine}")
          string(REPLACE "\r" "" fileLine "${fileLine}")

          string(STRIP "${fileLine}" strippedLine)

          string(REPLACE "SCORE_CONCRETE_COMPONENT_FACTORY(" "" strippedLine ${strippedLine})
          string(REPLACE ")" "" strippedLine ${strippedLine})
          string(REPLACE "," ";" lineAsList ${strippedLine})
          list(GET lineAsList 0 AbstractClassName)
          list(GET lineAsList 1 ClassName)
          string(STRIP ${AbstractClassName} strippedAbstractClassName)
          string(STRIP ${ClassName} strippedClassName)
          list(APPEND ComponentAbstractFactoryList "${strippedAbstractClassName}")
          list(APPEND ComponentFactoryList "${strippedClassName}")
          # There are two lists : the first contains the concrete class and the
          # second contains its abstract parent class. Then we iterate on the list to add
          # the childs to the parent correctly in the generated code.
      endforeach()

      # If there are matching strings, we add the file to our include list
      list(LENGTH fileContent matchingLines)
      if(${matchingLines} GREATER 0)
          list(APPEND ComponentFactoryFileList "${header}")
      endif()

  endforeach()

  set_property(GLOBAL PROPERTY SCORE_${TargetName}_AbstractFactoryList ${ComponentAbstractFactoryList})
  set_property(GLOBAL PROPERTY SCORE_${TargetName}_ConcreteFactoryList ${ComponentFactoryList})
  set_property(GLOBAL PROPERTY SCORE_${TargetName}_FactoryFileList ${ComponentFactoryFileList})
endfunction()

function(score_generate_plugin_file TargetName Headers Version Uuid)
    get_property(ComponentAbstractFactoryList GLOBAL PROPERTY SCORE_${TargetName}_AbstractFactoryList)
    get_property(ComponentFactoryList GLOBAL PROPERTY SCORE_${TargetName}_ConcreteFactoryList)
    get_property(ComponentFactoryFileList GLOBAL PROPERTY SCORE_${TargetName}_FactoryFileList)

    set(FactoryCode)
    set(CleanedAbstractFactoryList ${ComponentAbstractFactoryList})
    list(REMOVE_DUPLICATES CleanedAbstractFactoryList)

    list(LENGTH ComponentAbstractFactoryList LengthComponents)
    math(EXPR NumComponents ${LengthComponents}-1)

    foreach(AbstractClassName ${CleanedAbstractFactoryList})
        set(CurrentCode "FW<${AbstractClassName}")
        foreach(i RANGE ${NumComponents})
            list(GET ComponentAbstractFactoryList ${i} CurrentAbstractFactory)

            if(${AbstractClassName} STREQUAL ${CurrentAbstractFactory})
                list(GET ComponentFactoryList ${i} CurrentFactory)
                set(CurrentCode "${CurrentCode},\n    ${CurrentFactory}")
            endif()
        endforeach()
        set(CurrentCode "${CurrentCode}>\n")
        set(FactoryCode "${FactoryCode}${CurrentCode}")
    endforeach()

    # Generate a file with the list of includes
    set(FactoryFiles)
    foreach(header ${ComponentFactoryFileList})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" strippedheader ${header})
        set(FactoryFiles "${FactoryFiles}#include <${strippedheader}>\n")
    endforeach()

    # Set the variables that will be replaces
    set(PLUGIN_NAME ${TargetName})
    set(SCORE_COMPONENT_Version ${Version})
    set(SCORE_COMPONENT_Uuid ${Uuid})
    configure_file(
        "${SCORE_ROOT_SOURCE_DIR}/CMake/Components/score_component_plugin.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}_plugin.hpp")
    configure_file(
        "${SCORE_ROOT_SOURCE_DIR}/CMake/Components/score_component_plugin.cpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}_plugin.cpp")
endfunction()


### Generate files of commands ###
function(score_write_file FileName Content)
    if(EXISTS "${FileName}")
      file(READ "${FileName}" EXISTING_CONTENT)
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
  set(SCORE_PLUGINS_FILE_DATA)

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

  score_write_file("${SCORE_PLUGINS_FILE}" "${SCORE_PLUGINS_FILE_DATA}\n")
endfunction()
### Adds tests ###
function(setup_score_tests TestFolder)
  if(NOT DEPLOYMENT_BUILD AND NOT SCORE_STATIC_QT AND NOT IOS)
    add_subdirectory(${TestFolder})
  endif()
endfunction()
