project(score_app LANGUAGES CXX)

score_common_setup()
score_write_static_plugins_header()
enable_testing()
set(APPNAME "score")
set(CMAKE_POSITION_INDEPENDENT_CODE 1)

#add_executable(score-main WIN32 AppLauncher.cpp)
add_executable(${APPNAME} WIN32
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"

  "${3RDPARTY_FOLDER}/phantomstyle/src/phantom/phantomcolor.cpp"
  "${3RDPARTY_FOLDER}/phantomstyle/src/phantom/phantomstyle.cpp"

)
#target_link_libraries(score-main PRIVATE ${APPNAME})


set_target_properties(${APPNAME}
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1)

target_include_directories(${APPNAME} PRIVATE "${3RDPARTY_FOLDER}/phantomstyle/src")

target_link_libraries(${APPNAME} PUBLIC score_lib_base)
if(SCORE_STATIC_PLUGINS)
  target_link_libraries(${APPNAME} PUBLIC ${SCORE_PLUGINS_LIST})
endif()

target_compile_definitions(${APPNAME} PUBLIC
    $<$<CONFIG:Debug>:SCORE_SOURCE_DIR="${SCORE_ROOT_SOURCE_DIR}">
)
if(TARGET score_addon_jit)
  set_target_properties(${APPNAME} PROPERTIES ENABLE_EXPORTS 1)
  if(MINGW OR MSYS)
    target_link_libraries(${APPNAME} PUBLIC -Wl,--stack,16777216)
    # target_link_libraries(score-main PUBLIC -Wl,--stack,16777216)
  endif()
endif()

if(MSVC)
  # Necessary for JACK on windows.
  # http://jack-audio.10948.n7.nabble.com/Cannot-run-64bit-windows-client-td18952.html
  set_property(
    TARGET ${APPNAME}
    APPEND PROPERTY
      LINK_FLAGS "/DYNAMICBASE:NO")
endif()

if(WIN32)
  target_sources(${APPNAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/score.rc")
endif()

if(UNIX AND NOT APPLE)
  # Linux package is called ossia-score
  set_target_properties(${APPNAME}
    PROPERTIES
      OUTPUT_NAME ossia-score)

  target_link_libraries(${APPNAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

if(APPLE)
    find_library(CFLIB CoreFoundation)
    find_library(FLIB Foundation)
    target_link_libraries(${APPNAME} PRIVATE ${CFLIB} ${FLIB})

    if(DEPLOYMENT_BUILD)
      set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/score.icns")
      set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

      target_sources(${APPNAME} PRIVATE ${ICON_SRC})
      set_target_properties(${APPNAME} PROPERTIES MACOSX_BUNDLE TRUE)
    endif()
endif()

if(IOS)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(CORETEXT_LIBRARY CoreText)
  find_library(FOUNDATION_LIBRARY Foundation)
  find_library(ACCELERATE_LIBRARY Accelerate)
  find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
  find_library(SECURITY_LIBRARY Security)
  find_library(MobileCoreServices_LIBRARY MobileCoreServices)
  find_library(SystemConfiguration_LIBRARY SystemConfiguration)
  find_library(CoreGraphics_LIBRARY CoreGraphics)
  find_library(UIKit_LIBRARY UIKit)

  get_target_property(QtCore_LIB Qt5::Core LOCATION)
  get_filename_component(QT_LIB_FOLDER ${QtCore_LIB} DIRECTORY)
  find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS ${QT_LIB_FOLDER})
  find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS ${QT_LIB_FOLDER})
  find_library(qtpcre_LIBRARY NAMES qtpcre libqtpcre HINTS ${QT_LIB_FOLDER})
  target_link_libraries(${APPNAME} PUBLIC ${qtharfbuzzng_LIBRARY} ${qtfreetype_LIBRARY} ${qtpcre_LIBRARY}
    ${CoreGraphics_LIBRARY} ${CORETEXT_LIBRARY} ${COREFOUNDATION_LIBRARY}
    ${COREAUDIO_LIBRARY} ${SECURITY_LIBRARY} ${FOUNDATION_LIBRARY}
    ${ACCELERATE_LIBRARY} ${AUDIOTOOLBOX_LIBRARY}
    ${MobileCoreServices_LIBRARY} ${SystemConfiguration_LIBRARY} ${UIKit_LIBRARY}
    )
endif()

setup_score_common_exe_features(${APPNAME})

if(WIN32)
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/qt.conf"
        CONTENT  "[Platforms]
WindowsArguments = fontengine=freetype
"
         )
endif()

if(UNIX AND NOT APPLE)
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/run.sh")
      get_target_property(QtCore_LIB Qt5::Core LOCATION)
      get_filename_component(QT_ROOT_FOLDER "${QtCore_LIB}" DIRECTORY)
      get_filename_component(QT_ROOT_FOLDER "${QT_ROOT_FOLDER}" ABSOLUTE)
      file(
        WRITE "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
        "#!/bin/sh\nLD_LIBRARY_PATH=${QT_ROOT_FOLDER} ./ossia-score\n"
      )

      file(
        COPY "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
        DESTINATION "${CMAKE_BINARY_DIR}"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
      )
  endif()
endif()

if(WIN32)
  install(
    TARGETS ${APPNAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ${APPNAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()

include(UnityApp.cmake)
include(ScoreDeployment)

disable_qt_plugins(${APPNAME})
enable_minimal_qt_plugins(${APPNAME})
