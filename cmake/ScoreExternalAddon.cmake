if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
  cmake_policy(VERSION 3.26)
endif()

enable_language(C)
enable_language(CXX)

if(NOT SCORE_SOURCE_DIR)
  message(FATAL_ERROR "Please set SCORE_SOURCE_DIR to score's root source folder (/home/foo/score)")
endif()

if(NOT OSSIA_SDK)
  if(WIN32)
    set(OSSIA_SDK "c:/ossia-sdk")
  elseif(APPLE)
    set(OSSIA_SDK "/opt/ossia-sdk-x86_64")
  else()
    set(OSSIA_SDK "/opt/ossia-sdk")
  endif()
endif()

if(NOT EXISTS "${OSSIA_SDK}")
  message(FATAL_ERROR "Please fetch the SDK with the score/tools/fetch-sdk.sh script")
endif()

set(SCORE_VERSION_MAJOR 3)
set(SCORE_VERSION_MINOR 0)
set(SCORE_VERSION_PATCH 0)
set(SCORE_VERSION "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

set(SCORE_DYNAMIC_PLUGINS 1)

# Official Mac / Win / Linux releases are built against KFR
set(OSSIA_ENABLE_FFT 1)
set(OSSIA_ENABLE_KFR 1)
set(OSSIA_FFT KFR_DOUBLE)
set(OSSIA_FFT_KFR 1)

set(BUILD_SHARED_LIBS ON)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY True)
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 5000)


set(3RDPARTY_FOLDER "${SCORE_SOURCE_DIR}/3rdparty/")
set(OSSIA_3RDPARTY_FOLDER "${SCORE_SOURCE_DIR}/3rdparty/libossia/3rdparty")
set(SCORE_ROOT_SOURCE_DIR "${SCORE_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SCORE_SRC "${SCORE_SOURCE_DIR}/src")


set(SCORE_PLUGINS
  LIB_BASE
  LIB_DEVICE
  LIB_INSPECTOR
  LIB_LOCALTREE
  LIB_PROCESS
  LIB_STATE
  PLUGIN_AUDIO
  PLUGIN_AUTOMATION
  PLUGIN_AVND
  PLUGIN_CONTROLSURFACE
  PLUGIN_CURVE
  PLUGIN_DATAFLOW
  PLUGIN_DEVICEEXPLORER
  PLUGIN_ENGINE
  PLUGIN_GFX
  PLUGIN_JIT
  PLUGIN_JS
  PLUGIN_LIBRARY
  PLUGIN_MEDIA
  PLUGIN_MIDI
  PLUGIN_PROTOCOLS
  PLUGIN_RECORDING
  PLUGIN_REMOTECONTROL
  PLUGIN_SCENARIO
  PLUGIN_TRANSPORT
)

if(EXISTS "${SCORE_SDK}")
  include(${CMAKE_CURRENT_LIST_DIR}/ScoreExternalAddon.sdk.cmake)
else()
  include(${CMAKE_CURRENT_LIST_DIR}/ScoreExternalAddon.developer.cmake)
endif()

foreach(_lib ${SCORE_PLUGINS})
  string(TOLOWER "${_lib}" _lib_lc)
  target_link_libraries(score_${_lib_lc} INTERFACE score_lib_base)
endforeach()
target_link_libraries(score_plugin_engine INTERFACE score_lib_device score_lib_inspector score_lib_localtree score_lib_process score_lib_state score_plugin_dataflow score_plugin_library score_plugin_deviceexplorer score_plugin_scenario score_plugin_audio)
target_link_libraries(score_plugin_media INTERFACE score_plugin_engine)
target_link_libraries(score_plugin_gfx INTERFACE score_plugin_engine)
target_link_libraries(score_plugin_avnd INTERFACE score_plugin_gfx score_plugin_engine score_plugin_media)

if(APPLE)
  target_link_libraries(score_lib_base
    INTERFACE
      -Wl,-undefined,dynamic_lookup
  )
else()
  target_link_libraries(score_lib_base
    INTERFACE
      -Wl,--allow-shlib-undefined
  )
endif()

