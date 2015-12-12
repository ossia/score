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
    target_link_libraries(${theTarget} "-Wl,-fatal_warnings")
endfunction()

function(iscore_set_gcc_compile_options theTarget)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-error=shadow -Wno-error=switch -Wno-error=switch-enum -Wno-error=empty-body -Wno-error=overloaded-virtual -Wno-error=suggest-final-methods -Wno-error=suggest-final-types -Wno-error=suggest-override -Wno-error=maybe-uninitialized")

    if(ISCORE_SANITIZE)
      target_compile_definitions(${TheTarget} PUBLIC
        $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>
        $<$<CONFIG:Debug>:_GLIBCXX_DEBUG_PEDANTIC>
        )

      if(NOT ISCORE_COTIRE) ## Sanitizer won't work with PCHs
        target_compile_options(${theTarget} PUBLIC
          $<$<CONFIG:Debug>:-fsanitize=undefined>
        )
        target_link_libraries(${theTarget}
          $<$<CONFIG:Debug>:-fsanitize=undefined> )
      endif()
    endif()

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
    #if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    #	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-exit-time-destructors -Wno-padded")
    #endif()

    if(ISCORE_SANITIZE)
      if(NOT APPLE AND NOT WIN32) # Only for linux.
        # TODO : http://stackoverflow.com/a/30176092/1495627
        # Need to check for the libstdc++ abi.
        # target_compile_definitions(${TheTarget} PUBLIC
        #  $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>
        #  $<$<CONFIG:Debug>:_GLIBCXX_DEBUG_PEDANTIC>
        #  )
      endif()

      target_compile_options(${theTarget} PUBLIC
        $<$<CONFIG:Debug>:-fsanitize=undefined>
        )
      target_link_libraries(${theTarget}
        $<$<CONFIG:Debug>:-lubsan> )
    endif()
endfunction()

function(iscore_set_linux_compile_options theTarget)
    target_link_libraries(${theTarget} "-fuse-ld=gold" "-Wl,-z,defs")
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
    "$<$<AND:$<CONFIG:Release>,$<BOOL:ISCORE_ENABLE_OPTIMIZE_CUSTOM>>:-march=native>"
    )

endfunction()

function(iscore_set_compile_options theTarget)
  set_target_properties(${TheTarget} PROPERTIES CXX_STANDARD 14)

  target_compile_definitions(${TheTarget} PUBLIC
      $<$<CONFIG:Debug>:ISCORE_DEBUG>
      $<$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING>
      $<$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_SAFE_MODE>
      )

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
  set_target_properties(${TheTarget} PROPERTIES CXX_VISIBILITY_PRESET hidden)
  set_target_properties(${TheTarget} PROPERTIES VISIBILITY_INLINES_HIDDEN 1)

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


### Adds tests ###
function(setup_iscore_tests TestFolder)
  if(NOT DEPLOYMENT_BUILD)
    add_subdirectory(${TestFolder})
  endif()
endfunction()
