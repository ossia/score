include("${CMAKE_CURRENT_LIST_DIR}/ScoreAddonSetup.cmake")

get_filename_component(_avnd_root "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/avendish" ABSOLUTE)
if(EXISTS "${_avnd_root}/AvendishConfig.cmake")
  list(APPEND CMAKE_PREFIX_PATH "${_avnd_root}")
  if(CMAKE_FIND_PACKAGE_REDIRECTS_DIR)
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/avendish-config.cmake"
         "include(\"${_avnd_root}/AvendishConfig.cmake\")\n")
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/avendish-config-version.cmake"
         "set(PACKAGE_VERSION \"\${PACKAGE_FIND_VERSION}\")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nset(PACKAGE_VERSION_EXACT TRUE)\n")
  endif()
endif()

function(avnd_score_plugin_init)
  cmake_parse_arguments(AVND "" "BASE_TARGET" "" ${ARGN})
  if(NOT TARGET ${AVND_BASE_TARGET})
    add_library(${AVND_BASE_TARGET})
  endif()

  set(AVND_ADDITIONAL_CLASSES "" PARENT_SCOPE)
  set(AVND_CUSTOM_FACTORIES "" PARENT_SCOPE)
endfunction()

function(avnd_score_plugin_finalize)
  cmake_parse_arguments(AVND "CUSTOM_PLUGIN;MODULE" "BASE_TARGET;PLUGIN_VERSION;PLUGIN_UUID" "" ${ARGN})

  # Each addon's generated files live in their own directory, exposed only to
  # targets that link the addon. score_static_plugins.hpp registers plug-ins
  # behind `#if __has_include(<score_addon_foo.hpp>)`, so a header sitting in
  # CMAKE_BINARY_DIR -- which is on every target's include path -- makes that
  # guard true everywhere, and any target compiling that header then references
  # a constructor it does not link. Small unit tests are the ones that break.
  set(AVND_GEN_DIR "${CMAKE_BINARY_DIR}/score_addons/${AVND_BASE_TARGET}")

  if(NOT AVND_CUSTOM_PLUGIN)
    # Generate the score_plugin_foo.{h,c}pp
    configure_file(
      "${SCORE_AVND_SOURCE_DIR}/plugin_prototype.hpp.in"
      "${AVND_GEN_DIR}/${AVND_BASE_TARGET}.hpp"
      @ONLY
      NEWLINE_STYLE LF
    )
    if(AVND_MODULE)
      configure_file(
        "${SCORE_AVND_SOURCE_DIR}/module_plugin_prototype.cpp.in"
        "${AVND_GEN_DIR}/${AVND_BASE_TARGET}.cpp"
        @ONLY
        NEWLINE_STYLE LF
      )
      target_sources(${AVND_BASE_TARGET} PRIVATE FILE_SET CXX_MODULES FILES
        "${AVND_GEN_DIR}/${AVND_BASE_TARGET}.cpp"
      )
    else()
      configure_file(
        "${SCORE_AVND_SOURCE_DIR}/plugin_prototype.cpp.in"
        "${AVND_GEN_DIR}/${AVND_BASE_TARGET}.cpp"
        @ONLY
        NEWLINE_STYLE LF
      )
      target_sources(${AVND_BASE_TARGET} PRIVATE
        "${AVND_GEN_DIR}/${AVND_BASE_TARGET}.cpp"
      )
    endif()
  else()
    file(CONFIGURE OUTPUT
         "${AVND_GEN_DIR}/include.${AVND_BASE_TARGET}.cpp"
         CONTENT "${AVND_ADDITIONAL_CLASSES}\nstatic void all_custom_factories(auto& fx, auto& ctx, auto& key) { ${AVND_CUSTOM_FACTORIES} }\n"
         NEWLINE_STYLE LF)
  endif()

  # PUBLIC: the addon's own generated .cpp includes the header, and so must the
  # app that links it -- but nothing else.
  target_include_directories(${AVND_BASE_TARGET} PUBLIC "${AVND_GEN_DIR}")

  setup_score_plugin(${AVND_BASE_TARGET})
  target_link_libraries(${AVND_BASE_TARGET} PUBLIC score_plugin_engine score_plugin_avnd)

  if(SCORE_STATIC_PLUGINS)
    return()
  endif()

  # Setup for dynamic plug-in generation
  score_write_addon_manifest(
    TARGET "${AVND_BASE_TARGET}"
    NAME "${AVND_BASE_TARGET}"
    UUID "${AVND_PLUGIN_UUID}"
    VERSION "${AVND_PLUGIN_VERSION}")

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugins/localaddon.json
          DESTINATION .)
endfunction()

# The definitions a target compiles with that can change what the shared
# avendish PCH's headers declare: those of the avendish, halp and ossia macros.
# Definitions coming from the targets it links, transitively, are included;
# generator expressions are kept whole, unevaluated.
function(_score_avnd_pch_sensitive_definitions _out _target)
  set(_regex "(^-U|^|[^A-Za-z0-9_])(AVND|HALP|OSSIA)_[A-Za-z0-9_]")

  get_target_property(_defs ${_target} COMPILE_DEFINITIONS)
  if(NOT _defs)
    set(_defs)
  endif()
  get_target_property(_opts ${_target} COMPILE_OPTIONS)
  if(_opts)
    list(FILTER _opts INCLUDE REGEX "^-[DU]")
    list(APPEND _defs ${_opts})
  endif()

  get_target_property(_pending ${_target} LINK_LIBRARIES)
  if(NOT _pending)
    set(_pending)
  endif()
  set(_seen)
  while(_pending)
    list(POP_FRONT _pending _lib)
    string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" _lib "${_lib}")
    if(NOT TARGET "${_lib}" OR "${_lib}" IN_LIST _seen)
      continue()
    endif()
    list(APPEND _seen "${_lib}")
    get_target_property(_aliased "${_lib}" ALIASED_TARGET)
    if(_aliased)
      list(APPEND _pending "${_aliased}")
      continue()
    endif()
    get_target_property(_idefs "${_lib}" INTERFACE_COMPILE_DEFINITIONS)
    if(_idefs)
      list(APPEND _defs ${_idefs})
    endif()
    get_target_property(_ilibs "${_lib}" INTERFACE_LINK_LIBRARIES)
    if(_ilibs)
      list(APPEND _pending ${_ilibs})
    endif()
  endwhile()

  list(TRANSFORM _defs REPLACE "^-D" "")
  list(FILTER _defs INCLUDE REGEX "${_regex}")
  list(REMOVE_DUPLICATES _defs)
  set(${_out} "${_defs}" PARENT_SCOPE)
endfunction()

# Deferred to the end of the configuration. A consumer that defines one of
# those macros differently from score_plugin_avnd_pch would get the PCH's view
# of the headers, not its own: it is compiled without the PCH.
function(_score_avnd_apply_shared_pch)
  get_property(_consumers GLOBAL PROPERTY SCORE_AVND_PCH_CONSUMERS)
  if(NOT TARGET score_plugin_avnd_pch)
    return()
  endif()
  _score_avnd_pch_sensitive_definitions(_pch_defs score_plugin_avnd_pch)
  foreach(_t IN LISTS _consumers)
    _score_avnd_pch_sensitive_definitions(_defs ${_t})
    if(_pch_defs)
      list(REMOVE_ITEM _defs ${_pch_defs})
    endif()
    if(_defs)
      list(JOIN _defs " " _defs)
      message(STATUS "${_t}: not using the shared avendish PCH, it defines ${_defs}")
    else()
      target_precompile_headers(${_t} REUSE_FROM score_plugin_avnd_pch)
    endif()
  endforeach()
endfunction()

function(avnd_score_plugin_add)
  cmake_parse_arguments(AVND "DEVICE;MODULE;OPTIMIZED" "TARGET;MAIN_CLASS;NAMESPACE;BASE_TARGET" "SOURCES" ${ARGN})

  if(AVND_NAMESPACE)
    set(AVND_QUALIFIED "${AVND_NAMESPACE}::${AVND_MAIN_CLASS}")
  else()
    set(AVND_QUALIFIED "${AVND_MAIN_CLASS}")
  endif()

  list(GET AVND_SOURCES 0 AVND_MAIN_FILE)

#   if(TARGET avnd_source_parser)
#     set(AVND_REFLECTION_HELPERS_PRE "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.refl.pre.hpp")
#     set(AVND_REFLECTION_HELPERS "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.refl.hpp")
#     add_custom_command(
#       OUTPUT "${AVND_REFLECTION_HELPERS}"
#       COMMAND avnd_source_parser "${AVND_QUALIFIED}" "${AVND_MAIN_FILE}" "${AVND_REFLECTION_HELPERS}"
#       DEPENDS avnd_source_parser
#     )
#   endif()

  if(AVND_DEVICE)
    set(source_proto "${SCORE_AVND_SOURCE_DIR}/device-prototype.cpp.in")
    set(_gen_suffix "cpp")
  elseif(AVND_MODULE)
    set(source_proto "${SCORE_AVND_SOURCE_DIR}/module_prototype.cppm.in")
    set(_gen_suffix "cppm")
  else()
    set(source_proto "${SCORE_AVND_SOURCE_DIR}/prototype.cpp.in")
    set(_gen_suffix "cpp")
  endif()
  configure_file(
    "${source_proto}"
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.${_gen_suffix}"
    @ONLY
    NEWLINE_STYLE LF
  )

  # score_plugin_avnd_pch exists under SCORE_PCH or SCORE_AVND_PCH. In a unity
  # build it goes to the unity TUs too, as it would to each of their sources.
  # Whether the target can use it is only known once all its definitions are.
  if(TARGET score_plugin_avnd_pch)
    get_target_property(_has_custom_pch ${AVND_BASE_TARGET} SCORE_CUSTOM_PCH)
    if(NOT _has_custom_pch)
      set_target_properties(${AVND_BASE_TARGET} PROPERTIES SCORE_CUSTOM_PCH 1)
      set_property(GLOBAL APPEND PROPERTY SCORE_AVND_PCH_CONSUMERS ${AVND_BASE_TARGET})
      get_property(_deferred GLOBAL PROPERTY SCORE_AVND_PCH_DEFERRED)
      if(NOT _deferred)
        set_property(GLOBAL PROPERTY SCORE_AVND_PCH_DEFERRED 1)
        cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
          CALL _score_avnd_apply_shared_pch)
      endif()
    endif()
  endif()
  if(AVND_MODULE)
    target_sources(${AVND_BASE_TARGET}
      PRIVATE FILE_SET CXX_MODULES
      BASE_DIRS
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
      FILES
          ${AVND_SOURCES}
          "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cppm"
    )
  else()
    target_sources(${AVND_BASE_TARGET}
      PRIVATE
        ${AVND_SOURCES}
        "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
    )
  endif()

  set(AVND_STRUCT_TYPE struct)
  if(MSVC)
    foreach(AVND_FILE ${AVND_SOURCES})
      # MSVC ABI requires parsing to detect struct vs class
      file(READ "${AVND_FILE}" AVND_MAIN_HEADER_SOURCE)
      string(REGEX MATCH "class[ \t\r\n]+${AVND_MAIN_CLASS}[{ \t\r\n]+" AVND_TYPE_IS_CLASS_ABI "${AVND_MAIN_HEADER_SOURCE}")
      string(REGEX MATCH "struct[ \t\r\n]+${AVND_MAIN_CLASS}[{ \t\r\n]+" AVND_TYPE_IS_STRUCT_ABI "${AVND_MAIN_HEADER_SOURCE}")
      if(NOT AVND_TYPE_IS_CLASS_ABI AND NOT AVND_TYPE_IS_STRUCT_ABI)
        continue()
      elseif(AVND_TYPE_IS_CLASS_ABI)
        set(AVND_STRUCT_TYPE class)
        break()
      else()
        set(AVND_STRUCT_TYPE struct)
        break()
      endif()
    endforeach()
  endif()

  if(NOT MSVC AND NOT SCORE_DEPLOYMENT_BUILD)
    if(AVND_OPTIMIZED)
      set_source_files_properties(
          ${AVND_SOURCES}
          "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
        PROPERTIES
          COMPILE_OPTIONS "${SCORE_AVND_OPT_FLAGS}"
          SKIP_PRECOMPILE_HEADERS ON
          SKIP_UNITY_BUILD_INCLUSION ON
      )
    endif()
  endif()


  if(TARGET avnd_source_parser)
    target_sources(${AVND_BASE_TARGET} PRIVATE
      "${AVND_REFLECTION_HELPERS}"
    )
    target_compile_definitions(${AVND_BASE_TARGET} PRIVATE AVND_USE_TUPLET_TUPLE=1)
  endif()

  if(AVND_MODULE)
    set(txt "import ${AVND_TARGET}_factories;\n")
    set(txtf "::oscr::custom_factories_${AVND_TARGET}(fx, ctx, key); \n")
  else()
    if(AVND_NAMESPACE)
      set(txt "namespace ${AVND_NAMESPACE} { ${AVND_STRUCT_TYPE} ${AVND_MAIN_CLASS}; } \n")
      set(txtf "::oscr::custom_factories<${AVND_NAMESPACE}::${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
    else()
      set(txt "struct ${AVND_MAIN_CLASS}; \n")
      set(txtf "::oscr::custom_factories<${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
    endif()
  endif()

  set(AVND_ADDITIONAL_CLASSES "${AVND_ADDITIONAL_CLASSES}\n${txt}\n" PARENT_SCOPE)
  set(AVND_CUSTOM_FACTORIES "${AVND_CUSTOM_FACTORIES}\n${txtf}\n" PARENT_SCOPE)
endfunction()
