project(vst3puppet CXX)
find_package(${QT_VERSION} OPTIONAL_COMPONENTS WebSockets)

set(VST3_SDK_ROOT "${3RDPARTY_FOLDER}/vst3")
if(NOT TARGET ${QT_PREFIX}::WebSockets)
  message("VST loading requires QtWebSockets.")
  return()
endif()
add_executable(ossia-score-vst3puppet vst3puppet.cpp)


if(WIN32)
  target_sources(ossia-score-vst3puppet PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  )
elseif(APPLE)
  target_sources(ossia-score-vst3puppet PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
  )
else()
  target_sources(ossia-score-vst3puppet PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp"
  )
endif()

target_compile_definitions(ossia-score-vst3puppet PUBLIC HAS_VST3)
target_link_libraries(
  ossia-score-vst3puppet
  PRIVATE
    ${QT_PREFIX}::Core
    ${QT_PREFIX}::Gui
    ${QT_PREFIX}::WebSockets
    ${CMAKE_DL_LIBS}
    sdk_common sdk_hosting
    )

if(APPLE)
    find_library(Foundation_FK Foundation)
    target_link_libraries(ossia-score-vst3puppet PRIVATE
        ${Foundation_FK}
        )
endif()

target_include_directories(
  ossia-score-vst3puppet
  PRIVATE
    "${SCORE_SRC}/plugins/score-plugin-media"
)

setup_score_common_exe_features(ossia-score-vst3puppet)

set_target_properties(
    ossia-score-vst3puppet
    PROPERTIES
      DISABLE_PRECOMPILE_HEADERS TRUE
)

if(APPLE AND DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-vst3puppet
      PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        RUNTIME_OUTPUT_DIRECTORY score.app/Contents/MacOS)
  install(
    TARGETS ossia-score-vst3puppet
    BUNDLE DESTINATION score.app/Contents/MacOS
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-vst3puppet
    RUNTIME DESTINATION .
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-vst3puppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()

disable_qt_plugins(ossia-score-vst3puppet)
enable_minimal_qt_plugins(ossia-score-vst3puppet)
