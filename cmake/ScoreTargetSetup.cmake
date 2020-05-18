include(Sanitize)
include(UseGold)
include(LinkerWarnings)
include(DebugMode)

function(score_pch TheTarget)
  if(NOT SCORE_PCH)
    return()
  endif()
  if(NOT TARGET score_lib_base)
    return()
  endif()
  if("${TheTarget}" STREQUAL "score_lib_base")
      return()
  endif()
  if("${TheTarget}" STREQUAL "score_lib_pch")
      return()
  endif()

  target_precompile_headers("${TheTarget}" REUSE_FROM score_lib_pch)
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

function(score_set_msvc_compile_options theTarget)
    target_compile_options(${theTarget} PUBLIC
#    "/Za"
    "-wd4180"
    "-wd4224"
    "-wd4068" # pragma mark -
    "-wd4250" # inherits via dominance
    "-wd4251" # DLL stuff
    "-wd4275" # DLL stuff
    "-wd4244" # return : conversion from foo to bar, possible loss of data
    "-wd4800" # conversion from int to bool, performance warning
    "-wd4503" # decorated name length exceeded
    "-MP"
    "-std:c++latest"
    )

    target_compile_definitions(${theTarget} PUBLIC
        "NOMINMAX"
        )
endfunction()

function(score_set_apple_compile_options theTarget)
  target_compile_options(${theTarget} PUBLIC
    -Wno-auto-var-id
    -Wno-availability
    -Wno-deprecated-declarations
    -Wno-exceptions
    -Wno-auto-var-id
    -Wno-availability
    -Wno-deprecated-declarations
    -Wno-exceptions
    -Wno-extra-semi
    -Wno-gnu-folding-constant
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-inconsistent-missing-override
    -Wno-infinite-recursion
    -Wno-missing-method-return-type
    -Wno-non-virtual-dtor
    -Wno-nullability-completeness-on-arrays
    -Wno-nullability-extension
    -Wno-pedantic
    -Wno-sign-compare
    -Wno-switch
    -Wno-unguarded-availability-new
    -Wno-unknown-warning-option
    -Wno-unused-function
    -Wno-unused-local-typedef
    -Wno-unused-private-field
    -Wno-unused-variable
    -Wno-variadic-macros
    -Wno-zero-length-array
  )
endfunction()

function(score_set_gcc_compile_options theTarget)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-error=shadow -Wno-error=switch -Wno-error=switch-enum -Wno-error=empty-body -Wno-error=overloaded-virtual -Wno-error=suggest-final-methods -Wno-error=suggest-final-types -Wno-error=suggest-override -Wno-error=maybe-uninitialized")
        target_compile_options(${theTarget} PUBLIC
          -Wno-div-by-zero
          -Wsuggest-final-types
          -Wsuggest-final-methods
          -Wsuggest-override
          -Wpointer-arith
          -Wsuggest-attribute=noreturn
          -Wno-missing-braces
          -Wmissing-field-initializers
          -Wformat=2
          -Wno-format-nonliteral
          -Wpedantic
          -Werror=return-local-addr
          )

      if(NOT SCORE_SANITIZE)
      target_compile_options(${theTarget} PUBLIC
          "$<$<CONFIG:Release>:-ffunction-sections>"
          "$<$<CONFIG:Release>:-fdata-sections>"
          "$<$<CONFIG:Release>:-Wl,--gc-sections>"
          "$<$<CONFIG:Debug>:-O0>"
          "$<$<CONFIG:Debug>:-ggdb>"
      )

    if(SCORE_SPLIT_DEBUG)
      target_link_libraries(${theTarget} PUBLIC
        #          "$<$<CONFIG:Debug>:-Wa,--compress-debug-sections>"
        #          "$<$<CONFIG:Debug>:-Wl,--compress-debug-sections=zlib>"
                  "$<$<CONFIG:Debug>:-fvar-tracking-assignments>"
        )

      if(GDB_INDEX_SUPPORTED)
        if(NOT OSSIA_SANITIZE)
          target_link_libraries(${theTarget} PUBLIC
            "$<$<CONFIG:Debug>:-Wl,--gdb-index>"
          )
        endif()
      endif()
    endif()

      get_target_property(NO_LTO ${theTarget} SCORE_TARGET_NO_LTO)
      if(NOT ${NO_LTO})
          target_compile_options(${theTarget} PUBLIC
#            "$<$<BOOL:${SCORE_ENABLE_LTO}>:-s>"
#            "$<$<BOOL:${SCORE_ENABLE_LTO}>:-flto>"
#            "$<$<BOOL:${SCORE_ENABLE_LTO}>:-fuse-linker-plugin>"
#            "$<$<BOOL:${SCORE_ENABLE_LTO}>:-fno-fat-lto-objects>"
          )
      endif()
      target_link_libraries(${theTarget} PUBLIC
          "$<$<CONFIG:Release>:-ffunction-sections>"
          "$<$<CONFIG:Release>:-fdata-sections>"
          "$<$<CONFIG:Release>:-Wl,--gc-sections>"
          "$<$<CONFIG:Debug>:-fvar-tracking-assignments>"
          "$<$<CONFIG:Debug>:-O0>"
          "$<$<CONFIG:Debug>:-ggdb>"

#          "$<$<BOOL:${SCORE_ENABLE_LTO}>:-s>"
#          "$<$<BOOL:${SCORE_ENABLE_LTO}>:-flto>"
#          "$<$<BOOL:${SCORE_ENABLE_LTO}>:-fuse-linker-plugin>"
#          "$<$<BOOL:${SCORE_ENABLE_LTO}>:-fno-fat-lto-objects>"
          )

      endif()
      # -Wcast-qual is nice but requires more work...
      # -Wzero-as-null-pointer-constant  is garbage
      # Too much clutter :set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch-enum -Wshadow  -Wsuggest-attribute=const  -Wsuggest-attribute=pure ")
endfunction()

function(score_set_clang_compile_options theTarget)
    target_compile_options(${theTarget} PUBLIC
        -Wno-gnu-string-literal-operator-template
        -Wno-missing-braces
        -Werror=return-stack-address
        -Wmissing-field-initializers
        -Wno-gnu-statement-expression
        -Wno-four-char-constants
      #  -Wweak-vtables
        -ftemplate-backtrace-limit=0
        "$<$<CONFIG:Debug>:-O0>"
        )
    #if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    #	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-exit-time-destructors -Wno-padded")
    #endif()
endfunction()

function(score_set_linux_compile_options theTarget)
    use_gold(${theTarget})

    if(NOT SCORE_SANITIZE AND (LINKER_IS_GOLD OR LINKER_IS_LLD) AND SCORE_SPLIT_DEBUG)
        target_compile_options(${theTarget} PUBLIC
            # Debug options
          #          "$<$<CONFIG:Debug>:-Wa,--compress-debug-sections>"
          #          "$<$<CONFIG:Debug>:-Wl,--compress-debug-sections=zlib>"
          # "$<$<CONFIG:Debug>:-gsplit-dwarf>"
          "$<$<CONFIG:Debug>:-fdebug-types-section>"
          "$<$<CONFIG:Debug>:-ggnu-pubnames>"
        )

      target_link_libraries(${theTarget} PUBLIC
      -Wl,-z,defs
      -Wl,-z,now
      -Wl,--no-allow-shlib-undefined
      -Wl,--no-undefined
      -Wl,--unresolved-symbols,report-all
      )
    endif()
    target_compile_options(${theTarget} PUBLIC
        # Debug options
        "$<$<CONFIG:Debug>:-ggdb>"
        "$<$<CONFIG:Debug>:-O0>")
    target_link_libraries(${theTarget} PUBLIC
        # Debug options
        "$<$<CONFIG:Debug>:-ggdb>"
        "$<$<CONFIG:Debug>:-O0>")
endfunction()

function(score_set_unix_compile_options theTarget)
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
    -Wno-missing-declarations
    -Wredundant-decls
    -Werror=return-type
    -Werror=trigraphs
    # -std=c++17
    # Release options
    "$<$<CONFIG:Release>:-Ofast>"
    "$<$<CONFIG:Release>:-fno-finite-math-only>"
    "$<$<AND:$<CONFIG:Release>,$<BOOL:${SCORE_ENABLE_OPTIMIZE_CUSTOM}>>:-march=native>"
    )

    target_link_libraries(${theTarget} PUBLIC
        "$<$<CONFIG:Release>:-Ofast>"
        "$<$<CONFIG:Release>:-fno-finite-math-only>"
        "$<$<AND:$<CONFIG:Release>,$<BOOL:${SCORE_ENABLE_OPTIMIZE_CUSTOM}>>:-march=native>")
endfunction()

function(score_set_compile_options theTarget)
  #set_target_properties(${TheTarget} PROPERTIES CXX_STANDARD 17)
  target_compile_definitions(${theTarget} PUBLIC
      $<$<CONFIG:Debug>:SCORE_DEBUG>
      $<$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING>
      $<$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_SAFE_MODE>

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

  if(SCORE_SANITIZE)
      get_target_property(NO_SANITIZE ${theTarget} SCORE_TARGET_NO_SANITIZE)
      if(NOT "${NO_SANITIZE}")
          sanitize_build(${theTarget})
      endif()
      # debugmode_build(${theTarget})
  endif()

  if (CXX_IS_GCC OR CXX_IS_CLANG)
    score_set_unix_compile_options(${theTarget})
  endif()

  if (CXX_IS_CLANG)
      score_set_clang_compile_options(${theTarget})
  endif()

  if (CXX_IS_MSVC)
      score_set_msvc_compile_options(${theTarget})
  endif()

  if(CXX_IS_GCC)
      score_set_gcc_compile_options(${theTarget})
  endif()

  # OS X
  if(APPLE)
      score_set_apple_compile_options(${theTarget})
  endif()

  # Linux
  if(NOT APPLE AND NOT WIN32)
      score_set_linux_compile_options(${theTarget})
  endif()

  # currently breaks build : add_linker_warnings(${theTarget})
endfunction()

function(setup_score_common_features TheTarget)
  score_set_compile_options(${TheTarget})
  score_pch(${TheTarget})

  if(ENABLE_LTO)
    set_property(TARGET ${TheTarget}
                 PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
  endif()

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
