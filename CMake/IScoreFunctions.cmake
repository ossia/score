include(Sanitize)
include(UseGold)
include(LinkerWarnings)
include(DebugMode)

function(iscore_cotire_pre TheTarget)
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

function(iscore_set_msvc_compile_options theTarget)
    target_compile_options(${theTarget} PUBLIC
    "/Za"
    "/wd4180"
    "/wd4224"
    )
endfunction()

function(iscore_set_apple_compile_options theTarget)
    target_link_libraries(${theTarget} PRIVATE "-Wl,-fatal_warnings")
endfunction()

function(iscore_set_gcc_compile_options theTarget)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-error=shadow -Wno-error=switch -Wno-error=switch-enum -Wno-error=empty-body -Wno-error=overloaded-virtual -Wno-error=suggest-final-methods -Wno-error=suggest-final-types -Wno-error=suggest-override -Wno-error=maybe-uninitialized")

    if (GCC_VERSION VERSION_GREATER 5.2 OR GCC_VERSION VERSION_EQUAL 5.2)
        target_compile_options(${theTarget} PUBLIC
          -Wno-div-by-zero
          -Wsuggest-final-types
          -Wsuggest-final-methods
          -Wsuggest-override
          -Wpointer-arith
          -Wsuggest-attribute=noreturn
          -Wno-missing-braces
          -Wformat=2
          -Wno-format-nonliteral
          -Wpedantic
          )

      # -Wcast-qual is nice but requires more work...
      # -Wzero-as-null-pointer-constant  is garbage
      # Too much clutter :set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch-enum -Wshadow  -Wsuggest-attribute=const  -Wsuggest-attribute=pure ")
    endif()
endfunction()

function(iscore_set_clang_compile_options theTarget)
    target_compile_options(${theTarget} PUBLIC
        -Wno-gnu-string-literal-operator-template
        )
    #if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    #	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-exit-time-destructors -Wno-padded")
    #endif()
endfunction()

function(iscore_set_linux_compile_options theTarget)
  use_gold(${theTarget})
endfunction()

function(iscore_set_unix_compile_options theTarget)
    # General options

    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wabi -Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder -Wstrict-null-sentinel -Wno-non-template-friend -Woverloaded-virtual -Wno-pmf-conversions -Wsign-promo -Wextra -Wall -Waddress -Waggregate-return -Warray-bounds -Wno-attributes -Wno-builtin-macro-redefined")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wc++0x-compat -Wcast-align -Wcast-qual -Wchar-subscripts -Wclobbered -Wcomment -Wconversion -Wcoverage-mismatch -Wno-deprecated -Wno-deprecated-declarations -Wdisabled-optimization -Wno-div-by-zero -Wempty-body -Wenum-compare")

    target_compile_options(${theTarget} PUBLIC
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unknown-pragmas
    -Wnon-virtual-dtor
    -pedantic
    -Woverloaded-virtual
    -pipe

    # Debug options
    "$<$<CONFIG:Debug>:-gsplit-dwarf>"
    "$<$<CONFIG:Debug>:-O0>"
    "$<$<CONFIG:Debug>:-g>"

    # Release options
    "$<$<CONFIG:Release>:-Ofast>"
    "$<$<AND:$<CONFIG:Release>,$<BOOL:${ISCORE_ENABLE_OPTIMIZE_CUSTOM}>>:-march=native>"
    )

endfunction()

function(iscore_set_compile_options theTarget)
  set_target_properties(${TheTarget} PROPERTIES CXX_STANDARD 14)

  target_compile_definitions(${TheTarget} PUBLIC
      $<$<CONFIG:Debug>:ISCORE_DEBUG>
      $<$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING>
      $<$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_SAFE_MODE>

# various options

      $<$<BOOL:${ISCORE_IEEE}>:ISCORE_IEEE_SKIN>
      $<$<BOOL:${ISCORE_WEBSOCKETS}>:ISCORE_WEBSOCKETS>
      $<$<BOOL:${ISCORE_OPENGL}>:ISCORE_OPENGL>
      $<$<BOOL:${DEPLOYMENT_BUILD}>:ISCORE_DEPLOYMENT_BUILD>
      $<$<BOOL:${ISCORE_STATIC_PLUGINS}>:ISCORE_STATIC_PLUGINS>
      $<$<BOOL:${ISCORE_STATIC_PLUGINS}>:QT_STATICPLUGIN>
      )

  if(ISCORE_SANITIZE)
      sanitize_build(${theTarget})
      debugmode_build(${theTarget})
  endif()

  if (CXX_IS_CLANG)
      iscore_set_clang_compile_options(${theTarget})
  endif()

  if (CXX_IS_MSVC)
      iscore_set_msvc_compile_options(${theTarget})
  endif()

  if(CXX_IS_GCC)
      iscore_set_gcc_compile_options(${theTarget})
  endif()

  if (CXX_IS_GCC OR CXX_IS_CLANG)
    iscore_set_unix_compile_options(${theTarget})
  endif()

  # OS X
  if(APPLE)
      iscore_set_apple_compile_options(${theTarget})
  endif()

  # Linux
  if(NOT APPLE AND NOT WIN32)
      iscore_set_linux_compile_options(${theTarget})
  endif()

  use_gold(${theTarget})
  add_linker_warnings(${theTarget})
endfunction()

function(setup_iscore_common_features TheTarget)
  iscore_set_compile_options(${TheTarget})
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
endfunction()


### Initialization of common stuff ###
function(setup_iscore_common_exe_features TheTarget)
    setup_iscore_common_features("${TheTarget}")
endfunction()

function(setup_iscore_common_test_features TheTarget)
    setup_iscore_common_features("${TheTarget}")
endfunction()

function(setup_iscore_common_lib_features TheTarget)
  setup_iscore_common_features("${TheTarget}")

  generate_export_header(${TheTarget})
  if(NOT ISCORE_STATIC_PLUGINS)
    set_target_properties(${TheTarget} PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(${TheTarget} PROPERTIES VISIBILITY_INLINES_HIDDEN 1)
  endif()

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

### Component plug-ins ###

# TODO parcourir les fichiers pour trouver les FactoryFamily.
# Faire un premier parcours ou elles sont enregistrées,
# les stocker dans une map, et rajouter en parallèle les objets correspondants.
# Puis en fonction de la configuration (statique, dynamique) et du choix d'objets
# qu'on veut avoir dans le binaire, soit générer plein de sous-projets à la fin,
# soit générer un gros projet qui contient tout ? attention si des plug-ins ont
# des dépendances sur d'autres plug-ins... (générer les deps automatiquement)

function(iscore_generate_plugin_file TargetName Headers)
    # Initialize our lists
    set(ComponentAbstractFactoryList)
    set(ComponentFactoryList)
    set(ComponentFactoryFileList)

    # First look for the ISCORE_COMPONENT_FACTORY(...) ones
    foreach(header ${Headers})
        file(STRINGS "${header}" fileContent REGEX "ISCORE_COMPONENT_FACTORY\\(")

        # If there are matching strings, we add the file to our include list
        list(LENGTH fileContent matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND ComponentFactoryFileList "${header}")
        endif()

        foreach(fileLine ${fileContent})
            string(STRIP ${fileLine} strippedLine)
            string(REPLACE "ISCORE_COMPONENT_FACTORY(" "" strippedLine ${strippedLine})
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
    endforeach()

    set(FactoryCode)
    set(CleanedAbstractFactoryList ${ComponentAbstractFactoryList})
    list(LENGTH ComponentAbstractFactoryList LengthComponents)
    math(EXPR NumComponents ${LengthComponents}-1)
    list(REMOVE_DUPLICATES CleanedAbstractFactoryList)


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

    set(PLUGIN_NAME ${TargetName})
    configure_file(
        "${ISCORE_ROOT_SOURCE_DIR}/CMake/Components/iscore_component_plugin.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}_plugin.hpp")
    configure_file(
        "${ISCORE_ROOT_SOURCE_DIR}/CMake/Components/iscore_component_plugin.cpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}_plugin.cpp")
endfunction()


### Generate files of commands ###
function(iscore_write_file FileName Content)
    if(EXISTS "${FileName}")
      file(READ "${FileName}" EXISTING_CONTENT)
      if(NOT "${Content}" STREQUAL "${EXISTING_CONTENT}")
        file(WRITE "${FileName}" ${Content})
      endif()
    else()
        file(WRITE "${FileName}" ${Content})
    endif()
endfunction()

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

    iscore_write_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands_files.hpp"
        "${finalCommandFileList}"
        )

    # Generate a file with the list of types
    string(REPLACE ";" ", \n" commaSeparatedCommandList "${commandNameList}")
    iscore_write_file(
         "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands.hpp"
         "${commaSeparatedCommandList}"
        )

endfunction()

function(iscore_write_static_plugins_header)
  # TODO use iscore_write_file here
  set(ISCORE_PLUGINS_FILE "${ISCORE_ROOT_BINARY_DIR}/iscore_static_plugins.hpp")
  file(WRITE "${ISCORE_PLUGINS_FILE}" "#pragma once\n#include <QtPlugin>\n")
  foreach(plugin ${ISCORE_PLUGINS_LIST})
    message("Linking statically with i-score plugin : ${plugin}")
    file(APPEND "${ISCORE_PLUGINS_FILE}" "Q_IMPORT_PLUGIN(${plugin})\n")
  endforeach()
endfunction()

### Adds tests ###
function(setup_iscore_tests TestFolder)
  if(NOT DEPLOYMENT_BUILD)
    if(NOT ISCORE_STATIC_QT)
      add_subdirectory(${TestFolder})
    endif()
  endif()
endfunction()
