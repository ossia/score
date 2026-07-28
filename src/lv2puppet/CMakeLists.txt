project(lv2puppet CXX)

find_path(LV2_PATH lv2/lv2plug.in/ns/ext/atom/atom.h
          HINTS "${OSSIA_SDK}/lv2/include")
if(NOT TARGET Lilv)
  find_package(Lilv)
endif()
if(NOT TARGET Lilv OR NOT LV2_PATH)
  message(STATUS "lv2puppet: Lilv or LV2_PATH not found, skipping")
  return()
endif()

add_executable(ossia-score-lv2puppet WIN32
  lv2puppet.cpp
  ../vstpuppet/window.cpp
)
target_compile_definitions(ossia-score-lv2puppet PRIVATE HAS_LV2)
target_link_libraries(
  ossia-score-lv2puppet
  PRIVATE
    $<COMPILE_ONLY:ossia>
    $<BUILD_INTERFACE:fmt::fmt>
    Lilv
    Threads::Threads
    ${CMAKE_DL_LIBS})

if(APPLE)
    find_library(Foundation_FK Foundation)
    find_library(Cocoa_FK Cocoa)
    target_link_libraries(ossia-score-lv2puppet PRIVATE
        ${Foundation_FK}
        ${Cocoa_FK}
        )
elseif(WIN32)
  target_link_libraries(ossia-score-lv2puppet PRIVATE user32 gdi32 ws2_32)
else()
  target_include_directories(ossia-score-lv2puppet PRIVATE "${X11_X11_INCLUDE_PATH}")
endif()

target_include_directories(
  ossia-score-lv2puppet
  PRIVATE
    "${SCORE_SRC}/lib"
    "${LV2_PATH}"
)

setup_score_common_exe_features(ossia-score-lv2puppet)

set_target_properties(
    ossia-score-lv2puppet
    PROPERTIES
      DISABLE_PRECOMPILE_HEADERS TRUE
)

if(APPLE AND SCORE_DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-lv2puppet
      PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ossia score.app/Contents/MacOS")
  install(
    TARGETS ossia-score-lv2puppet
    BUNDLE DESTINATION "ossia score.app/Contents/MacOS"
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-lv2puppet
    RUNTIME DESTINATION "${SCORE_BIN_INSTALL_DIR}"
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-lv2puppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()
