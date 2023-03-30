function(avnd_score_plugin_init)
  cmake_parse_arguments(AVND "" "BASE_TARGET" "" ${ARGN})

  if(NOT TARGET ${AVND_BASE_TARGET})
    add_library(
      ${AVND_BASE_TARGET}
    )
  endif()

  set(AVND_ADDITIONAL_CLASSES "" PARENT_SCOPE)
  set(AVND_CUSTOM_FACTORIES "" PARENT_SCOPE)
endfunction()

function(avnd_score_plugin_finalize)
  cmake_parse_arguments(AVND "" "BASE_TARGET;PLUGIN_VERSION;PLUGIN_UUID" "" ${ARGN})

  # Generate the score_plugin_foo.cpp
  configure_file(
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd/plugin_prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(${AVND_BASE_TARGET} PRIVATE
    "${CMAKE_BINARY_DIR}/${AVND_BASE_TARGET}.cpp"
  )

  setup_score_plugin(${AVND_BASE_TARGET})
  target_link_libraries(${AVND_BASE_TARGET} PUBLIC score_plugin_engine score_plugin_avnd)

endfunction()

function(avnd_score_plugin_add)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_CLASS;NAMESPACE;BASE_TARGET" "SOURCES" ${ARGN})

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

  configure_file(
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(${AVND_BASE_TARGET} PRIVATE
    ${AVND_SOURCES}
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
  )

  if(TARGET avnd_source_parser)
    target_sources(${AVND_BASE_TARGET} PRIVATE
      "${AVND_REFLECTION_HELPERS}"
    )
    target_compile_definitions(${AVND_BASE_TARGET} PRIVATE AVND_USE_TUPLET_TUPLE=1)
  endif()

  if(AVND_NAMESPACE)
    set(txt "namespace ${AVND_NAMESPACE} { struct ${AVND_MAIN_CLASS}; } \n")
    set(txtf "::oscr::custom_factories<${AVND_NAMESPACE}::${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
  else()
    set(txt "struct ${AVND_MAIN_CLASS}; \n")
    set(txtf "::oscr::custom_factories<${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
  endif()

  set(AVND_ADDITIONAL_CLASSES "${AVND_ADDITIONAL_CLASSES}\n${txt}\n" PARENT_SCOPE)
  set(AVND_CUSTOM_FACTORIES "${AVND_CUSTOM_FACTORIES}\n${txtf}\n" PARENT_SCOPE)
endfunction()
