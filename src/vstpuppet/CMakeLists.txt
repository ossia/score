project(vstpuppet CXX)
find_package(${QT_VERSION} OPTIONAL_COMPONENTS WebSockets)

if(NOT TARGET ${QT_PREFIX}::WebSockets)
  message("VST loading requires QtWebSockets.")
  return()
endif()
add_executable(ossia-score-vstpuppet vstpuppet.cpp "${SCORE_SRC}/plugins/score-plugin-media/Media/Effect/VST/VSTLoader.cpp")
target_compile_definitions(ossia-score-vstpuppet PUBLIC HAS_VST2)
target_link_libraries(
  ossia-score-vstpuppet
  PRIVATE
    ${QT_PREFIX}::Core
    ${QT_PREFIX}::Gui
    ${QT_PREFIX}::WebSockets
    ${CMAKE_DL_LIBS})

if(APPLE)
    find_library(Foundation_FK Foundation)
    target_link_libraries(ossia-score-vstpuppet PRIVATE
        ${Foundation_FK}
        )
endif()

target_include_directories(
  ossia-score-vstpuppet
  PRIVATE
    "${SCORE_SRC}/plugins/score-plugin-media"
)

setup_score_common_exe_features(ossia-score-vstpuppet)

set_target_properties(
    ossia-score-vstpuppet
    PROPERTIES
      DISABLE_PRECOMPILE_HEADERS TRUE
)

if(APPLE AND DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-vstpuppet
      PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        RUNTIME_OUTPUT_DIRECTORY score.app/Contents/MacOS)
  install(
    TARGETS ossia-score-vstpuppet
    BUNDLE DESTINATION score.app/Contents/MacOS
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION .
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()

disable_qt_plugins(ossia-score-vstpuppet)
enable_minimal_qt_plugins(ossia-score-vstpuppet)
