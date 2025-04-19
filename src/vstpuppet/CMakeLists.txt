project(vstpuppet CXX)
add_executable(ossia-score-vstpuppet vstpuppet.cpp "${SCORE_SRC}/plugins/score-plugin-vst/Vst/Loader.cpp")
target_compile_definitions(ossia-score-vstpuppet PRIVATE HAS_VST2)
target_link_libraries(
  ossia-score-vstpuppet
  PRIVATE
    $<COMPILE_ONLY:ossia>
    ${CMAKE_DL_LIBS})

if(APPLE)
    find_library(Foundation_FK Foundation)
    find_library(Cocoa_FK Cocoa)
    target_link_libraries(ossia-score-vstpuppet PRIVATE
        ${Foundation_FK}
        ${Cocoa_FK}
        )
elseif(WIN32)
  target_link_libraries(ossia-score-vstpuppet PRIVATE user32 gdi32)
else()
  target_include_directories(ossia-score-vstpuppet PRIVATE "${X11_X11_INCLUDE_PATH}")
endif()

target_include_directories(
  ossia-score-vstpuppet
  PRIVATE
    "${SCORE_SRC}/lib"
    "${SCORE_SRC}/plugins/score-plugin-vst"
)

setup_score_common_exe_features(ossia-score-vstpuppet)

set_target_properties(
    ossia-score-vstpuppet
    PROPERTIES
      DISABLE_PRECOMPILE_HEADERS TRUE
)

if(APPLE AND SCORE_DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-vstpuppet
      PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        RUNTIME_OUTPUT_DIRECTORY "ossia score.app/Contents/MacOS")
  install(
    TARGETS ossia-score-vstpuppet
    BUNDLE DESTINATION "ossia score.app/Contents/MacOS"
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION "${SCORE_BIN_INSTALL_DIR}"
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()
