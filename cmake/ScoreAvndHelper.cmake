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

  if(NOT AVND_CUSTOM_PLUGIN)
    # Generate the score_plugin_foo.{h,c}pp
    configure_file(
      "${SCORE_AVND_SOURCE_DIR}/plugin_prototype.hpp.in"
      "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.hpp"
      @ONLY
      NEWLINE_STYLE LF
    )
    if(AVND_MODULE)
      configure_file(
        "${SCORE_AVND_SOURCE_DIR}/module_plugin_prototype.cpp.in"
        "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.cpp"
        @ONLY
        NEWLINE_STYLE LF
      )
      target_sources(${AVND_BASE_TARGET} PRIVATE FILE_SET CXX_MODULES FILES
        "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.cpp"
      )
    else()
      configure_file(
        "${SCORE_AVND_SOURCE_DIR}/plugin_prototype.cpp.in"
        "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.cpp"
        @ONLY
        NEWLINE_STYLE LF
      )
      target_sources(${AVND_BASE_TARGET} PRIVATE
        "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.cpp"
      )
    endif()
  else()
    file(CONFIGURE OUTPUT
         "${CMAKE_BINARY_DIR}/include.${AVND_BASE_TARGET}.cpp"
         CONTENT "${AVND_ADDITIONAL_CLASSES}\nstatic void all_custom_factories(auto& fx, auto& ctx, auto& key) { ${AVND_CUSTOM_FACTORIES} }\n"
         NEWLINE_STYLE LF)
  endif()

  setup_score_plugin(${AVND_BASE_TARGET})
  target_link_libraries(${AVND_BASE_TARGET} PUBLIC score_plugin_engine score_plugin_avnd)

  if(SCORE_STATIC_PLUGINS)
    return()
  endif()

  # Setup for dynamic plug-in generation
  if(APPLE)
    set(PLUGIN_PLATFORM "darwin-amd64")
  elseif(WIN32)
    set(PLUGIN_PLATFORM "windows-amd64")
  else()
    set(PLUGIN_PLATFORM "linux-amd64")
  endif()

  get_target_property(PLUGIN_BINARY "${AVND_BASE_TARGET}" OUTPUT_NAME)

  file(GENERATE OUTPUT plugins/localaddon.json
    CONTENT
      "{
  \"${PLUGIN_PLATFORM}\": \"$<TARGET_FILE_NAME:${AVND_BASE_TARGET}>\",
  \"name\": \"${AVND_BASE_TARGET}\",
  \"raw_name\": \"${AVND_BASE_TARGET}\",
  \"version\": \"${AVND_PLUGIN_VERSION}\",
  \"kind\": \"Sample addon\",
  \"short\": \"Sample addon\",
  \"long\": \"Sample addon\",
  \"key\": \"${AVND_PLUGIN_UUID}\"
}"
  )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugins/localaddon.json
          DESTINATION .)
endfunction()

function(avnd_score_plugin_add)
  cmake_parse_arguments(AVND "DEVICE;MODULE" "TARGET;MAIN_CLASS;NAMESPACE;BASE_TARGET" "SOURCES" ${ARGN})

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

  if(NOT CMAKE_UNITY_BUILD)
    set_target_properties(${AVND_BASE_TARGET} PROPERTIES SCORE_CUSTOM_PCH 1)
    target_precompile_headers("${AVND_BASE_TARGET}" REUSE_FROM score_plugin_avnd)
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
      set(txt "namespace ${AVND_NAMESPACE} { struct ${AVND_MAIN_CLASS}; } \n")
      set(txtf "::oscr::custom_factories<${AVND_NAMESPACE}::${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
    else()
      set(txt "struct ${AVND_MAIN_CLASS}; \n")
      set(txtf "::oscr::custom_factories<${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
    endif()
  endif()

  set(AVND_ADDITIONAL_CLASSES "${AVND_ADDITIONAL_CLASSES}\n${txt}\n" PARENT_SCOPE)
  set(AVND_CUSTOM_FACTORIES "${AVND_CUSTOM_FACTORIES}\n${txtf}\n" PARENT_SCOPE)
endfunction()
