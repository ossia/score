project(vst3puppet CXX)
set(VST3_SDK_ROOT "${3RDPARTY_FOLDER}/vst3")
add_executable(ossia-score-vst3puppet vst3puppet.cpp)

if(WIN32)
  target_sources(ossia-score-vst3puppet PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  )
elseif(APPLE)
  target_sources(ossia-score-vst3puppet PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
  )
  set_source_files_properties(
      "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
    PROPERTIES
      SKIP_UNITY_BUILD_INCLUSION 1
      COMPILE_OPTIONS -fobjc-arc
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
    $<COMPILE_ONLY:ossia>
    $<BUILD_INTERFACE:fmt::fmt>
    ${CMAKE_DL_LIBS}
    sdk_common sdk_hosting
    )

if(APPLE)
    find_library(Foundation_FK Foundation)
    find_library(Cocoa_FK Cocoa)
    target_link_libraries(ossia-score-vst3puppet PRIVATE
        ${Foundation_FK}
        ${Cocoa_FK}
        )
elseif(WIN32)
  target_link_libraries(ossia-score-vst3puppet PRIVATE user32 gdi32 ws2_32)
else()
  target_include_directories(ossia-score-vstpuppet PRIVATE "${X11_X11_INCLUDE_PATH}")
endif()

target_include_directories(
  ossia-score-vst3puppet
  PRIVATE
    "${SCORE_SRC}/lib"
    "${SCORE_SRC}/plugins/score-plugin-media"
)

setup_score_common_exe_features(ossia-score-vst3puppet)

set_target_properties(
    ossia-score-vst3puppet
    PROPERTIES
      DISABLE_PRECOMPILE_HEADERS TRUE
)

if(APPLE AND SCORE_DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-vst3puppet
      PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        RUNTIME_OUTPUT_DIRECTORY "ossia score.app/Contents/MacOS")
  install(
    TARGETS ossia-score-vst3puppet
    BUNDLE DESTINATION "ossia score.app/Contents/MacOS"
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-vst3puppet
    RUNTIME DESTINATION "${SCORE_BIN_INSTALL_DIR}"
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-vst3puppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()
