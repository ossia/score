add_library(
  ssynth STATIC
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Parser/EisenParser.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Parser/Preprocessor.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Parser/Tokenizer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/Action.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/AmbiguousRule.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/Builder.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/CustomRule.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/PrimitiveRule.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/RuleSet.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/State.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/Transformation.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/Rendering/TemplateRenderer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Model/Rendering/ObjRenderer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/ColorPool.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/ColorUtils.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/Logging.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/MiniParser.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/libssynth/src/ssynth/RandomStreams.cpp")
target_include_directories(ssynth SYSTEM
                           PUBLIC "${CMAKE_CURRENT_LIST_DIR}/libssynth/src")

target_link_libraries(ssynth PRIVATE "${QT_PREFIX}::Core" "${QT_PREFIX}::Gui"
                                     "${QT_PREFIX}::Xml" score_lib_base)

if(NOT MSVC)
  target_compile_options(ssynth PRIVATE -w)
endif()
