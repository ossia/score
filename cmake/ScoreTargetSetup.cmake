include(Sanitize)
include(UseGold)
include(LinkerWarnings)
include(DebugMode)

function(score_pch TheTarget)
  disable_qt_plugins("${TheTarget}")

  if(NOT SCORE_PCH)
    return()
  endif()
  if(NOT TARGET score_lib_pch)
    return()
  endif()
  if("${TheTarget}" STREQUAL "score_lib_base")
      return()
  endif()
  if("${TheTarget}" STREQUAL "score_lib_pch")
      return()
  endif()
  if("${TheTarget}" STREQUAL "score_plugin_jit") # due to a bug with llvm 10 in c++20
    return()
  endif()

  target_precompile_headers("${TheTarget}" REUSE_FROM score_lib_pch)
  target_compile_definitions("${TheTarget}" PRIVATE SCORE_LIB_PCH_EXPORTS)
endfunction()

### Call at the beginning of a plug-in cmakelists ###
macro(score_common_setup)
  enable_testing()
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_AUTOUIC OFF)
  set(CMAKE_AUTOMOC OFF)
endmacro()

### Initialization of most common stuff ###
function(score_set_compile_options theTarget)
  # CXX_VERSION_FLAG: see ScoreConfiguration.cmake
  if(CMAKE_VERSION VERSION_GREATER 3.16)
    target_compile_features(${theTarget} PRIVATE ${CXX_VERSION_FLAG})
  else()
    target_compile_features(${theTarget} PRIVATE cxx_std_17)
  endif()

  target_compile_definitions(${theTarget} PUBLIC
      $<$<CONFIG:Debug>:SCORE_DEBUG>

      QT_NO_LINKED_LIST
      QT_NO_JAVA_STYLE_ITERATORS
      QT_NO_USING_NAMESPACE
      QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
      QT_USE_QSTRINGBUILDER
# various options

      $<$<BOOL:${SCORE_IEEE}>:SCORE_IEEE_SKIN>
      $<$<BOOL:${SCORE_WEBSOCKETS}>:SCORE_WEBSOCKETS>
      $<$<BOOL:${SCORE_OPENGL}>:SCORE_OPENGL>
      $<$<BOOL:${DEPLOYMENT_BUILD}>:SCORE_DEPLOYMENT_BUILD>
      $<$<BOOL:${SCORE_STATIC_PLUGINS}>:SCORE_STATIC_PLUGINS>
      )
  get_target_property(theType ${theTarget} TYPE)

  if(${theType} MATCHES STATIC_LIBRARY)
    target_compile_definitions(${theTarget} PRIVATE
      $<$<BOOL:${SCORE_STATIC_PLUGINS}>:QT_STATICPLUGIN>
    )
  endif()
endfunction()

function(setup_score_common_features TheTarget)
  score_set_compile_options(${TheTarget})
  score_pch(${TheTarget})

  if(SCORE_STATIC_PLUGINS)
    target_compile_definitions(${TheTarget}
                               PUBLIC SCORE_STATIC_PLUGINS)
  endif()

  target_include_directories(${TheTarget} INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()


### Initialization of common stuff ###
function(setup_score_common_exe_features TheTarget)
  setup_score_common_features(${TheTarget})
endfunction()

function(setup_score_common_test_features TheTarget)
  setup_score_common_features(${TheTarget})
endfunction()

function(setup_score_common_lib_features TheTarget)
  setup_score_common_features(${TheTarget})

  string(TOUPPER ${TheTarget} Target_upper)
  set_target_properties(${TheTarget} PROPERTIES
    DEFINE_SYMBOL "${Target_upper}_EXPORTS"
    )

  if(SCORE_STATIC_PLUGINS)
    target_compile_definitions(${TheTarget} PRIVATE "${Target_upper}_EXPORTS")
  endif()
  set_target_properties(${TheTarget} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1
  )


  if(OSSIA_STATIC_EXPORT)
    generate_export_header(${TheTarget} ALWAYS_EXPORT)
    target_compile_definitions(${TheTarget} PRIVATE "${Target_upper}_EXPORTS=1")
  else()
    generate_export_header(${TheTarget})
  endif()

  get_target_property(_srcDir ${TheTarget} SOURCE_DIR)
  get_target_property(_binDir ${TheTarget} BINARY_DIR)

  if(SCORE_INSTALL_HEADERS)
    install(DIRECTORY "${_srcDir}/"
            DESTINATION include/score
            COMPONENT Devel
            FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN ".git" EXCLUDE
            PATTERN "tests" EXCLUDE
            PATTERN "Tests" EXCLUDE
            PATTERN "SDK" EXCLUDE
    )
    install(FILES
          ${_binDir}/${TheTarget}_export.h
          ${_binDir}/${TheTarget}_commands.hpp
          ${_binDir}/${TheTarget}_commands_files.hpp
          DESTINATION include/score
          COMPONENT Devel
          OPTIONAL)
  endif()
  string(TOUPPER "${TheTarget}" UPPERCASE_PLUGIN_NAME)
  target_include_directories(${TheTarget} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions(${TheTarget} INTERFACE "${UPPERCASE_PLUGIN_NAME}")
endfunction()


### Call with a library target ###
function(setup_score_library PluginName)
  setup_score_common_lib_features("${PluginName}")

  set(SCORE_LIBRARIES_LIST ${SCORE_LIBRARIES_LIST} "${PluginName}" CACHE INTERNAL "List of libraries")
  set_target_properties(${PluginName} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/")

  if(NOT SCORE_STATIC_PLUGINS)
    if(SCORE_BUILD_FOR_PACKAGE_MANAGER)
      install(TARGETS "${PluginName}"
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
    else()
      install(TARGETS "${PluginName}"
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION static_lib
        RUNTIME DESTINATION bin
        )
    endif()
  endif()
endfunction()

### Call with a plug-in target ###
function(setup_score_plugin PluginName)
  setup_score_common_lib_features("${PluginName}")

  set(SCORE_PLUGINS_LIST ${SCORE_PLUGINS_LIST} "${PluginName}" CACHE INTERNAL "List of plugins")

  set_target_properties(${PluginName} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/")
  if(NOT SCORE_STATIC_PLUGINS)
    if(SCORE_BUILD_FOR_PACKAGE_MANAGER)
      install(TARGETS "${PluginName}"
        LIBRARY DESTINATION lib/score
        ARCHIVE DESTINATION lib/score)
    else()
      install(TARGETS "${PluginName}"
        LIBRARY DESTINATION plugins
        ARCHIVE DESTINATION static_plugins
        RUNTIME DESTINATION bin/plugins)
    endif()
  endif()
endfunction()
