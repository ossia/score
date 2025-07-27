project(clappuppet CXX)
add_executable(ossia-score-clappuppet WIN32
  clappuppet.cpp
  ../vstpuppet/window.cpp
)
target_compile_definitions(ossia-score-clappuppet PRIVATE HAS_CLAP)
target_link_libraries(
  ossia-score-clappuppet
  PRIVATE
    $<COMPILE_ONLY:ossia>
    $<BUILD_INTERFACE:fmt::fmt>
    Threads::Threads
    ${CMAKE_DL_LIBS})

if(APPLE)
    find_library(Foundation_FK Foundation)
    find_library(Cocoa_FK Cocoa)
    target_link_libraries(ossia-score-clappuppet PRIVATE
        ${Foundation_FK}
        ${Cocoa_FK}
        )
elseif(WIN32)
  target_link_libraries(ossia-score-clappuppet PRIVATE user32 gdi32 ws2_32)
else()
  target_include_directories(ossia-score-clappuppet PRIVATE "${X11_X11_INCLUDE_PATH}")
endif()

target_include_directories(
  ossia-score-clappuppet
  PRIVATE
    "${SCORE_SRC}/lib"
    "${3RDPARTY_FOLDER}/clap/include"
)

setup_score_common_exe_features(ossia-score-clappuppet)

set_target_properties(
    ossia-score-clappuppet
    PROPERTIES
      DISABLE_PRECOMPILE_HEADERS TRUE
)

if(APPLE AND SCORE_DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-clappuppet
      PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ossia score.app/Contents/MacOS")
  install(
    TARGETS ossia-score-clappuppet
    BUNDLE DESTINATION "ossia score.app/Contents/MacOS"
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-clappuppet
    RUNTIME DESTINATION "${SCORE_BIN_INSTALL_DIR}"
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-clappuppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()
