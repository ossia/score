if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
  cmake_policy(VERSION 3.18)
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

set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${SCORE_SOURCE_DIR}/cmake"
    "${SCORE_SOURCE_DIR}/cmake/modules"
    "${SCORE_SOURCE_DIR}/cmake/Configurations"
    "${SCORE_SOURCE_DIR}/cmake/Configurations/travis"
    "${SCORE_SOURCE_DIR}/3rdparty/libossia/cmake"
    "${SCORE_SOURCE_DIR}/3rdparty/libossia/cmake/cmake-modules")

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_MODULE_PATH}")

set(CMAKE_PREFIX_PATH
    "${OSSIA_SDK}"
    "${OSSIA_SDK}/SDL2/cmake"
    "${OSSIA_SDK}/qt5-static"
    "${OSSIA_SDK}/llvm-libs"
    "${OSSIA_SDK}/llvm"
    "${OSSIA_SDK}/fftw"
    "${OSSIA_SDK}/zlib"
    "${OSSIA_SDK}/faust"
    "${OSSIA_SDK}/openssl"
    "${OSSIA_SDK}/portaudio/lib/cmake"
    "${OSSIA_SDK}/portaudio/lib/cmake/portaudio"
    "${CMAKE_PREFIX_PATH}")

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OSSIA_SDK}/ffmpeg/lib/pkgconfig")

set(SCORE_VERSION_MAJOR 3)
set(SCORE_VERSION_MINOR 0)
set(SCORE_VERSION_PATCH 0)
set(SCORE_VERSION "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

set(SCORE_DYNAMIC_PLUGINS 1)
set(OSSIA_FFT "DOUBLE")

find_package(Qt5 5.9 REQUIRED COMPONENTS
  Core
  Widgets
  Gui
  Network
  Qml
)

foreach(_lib LIB_BASE LIB_DEVICE LIB_INSPECTOR LIB_LOCALTREE LIB_PROCESS LIB_STATE PLUGIN_AUDIO PLUGIN_AUTOMATION PLUGIN_CURVE PLUGIN_DATAFLOW PLUGIN_DEVICEEXPLORER PLUGIN_ENGINE PLUGIN_GFX PLUGIN_JIT PLUGIN_JS PLUGIN_LIBRARY PLUGIN_MAPPING PLUGIN_MEDIA PLUGIN_MIDI PLUGIN_RECORDING PLUGIN_REMOTECONTROL PLUGIN_SCENARIO)
  string(TOLOWER "${_lib}" _lib_lc)
  string(REPLACE "_" "-" _lib_folder "${_lib_lc}")
  set(filename score_${_lib_lc}_export.h)
  file(CONFIGURE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${filename}"
    CONTENT "#pragma once
#define SCORE_${_lib}_EXPORT
#define SCORE_${_lib}_EXPORTS 0
")
  add_library(score_${_lib_lc} INTERFACE)
  target_include_directories(score_${_lib_lc} INTERFACE "${SCORE_SOURCE_DIR}/src/plugins/score-${_lib_folder}")
  message(" - includeing  ${SCORE_SOURCE_DIR}/src/plugins/score-${_lib_folder}")
endforeach()

target_link_libraries(score_lib_base INTERFACE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Qml Qt5::WidgetsPrivate Qt5::GuiPrivate Qt5::CorePrivate)

foreach(_lib LIB_DEVICE LIB_INSPECTOR LIB_LOCALTREE LIB_PROCESS LIB_STATE PLUGIN_AUDIO PLUGIN_AUTOMATION PLUGIN_CURVE PLUGIN_DATAFLOW PLUGIN_DEVICEEXPLORER PLUGIN_ENGINE PLUGIN_GFX PLUGIN_JIT PLUGIN_JS PLUGIN_LIBRARY PLUGIN_MAPPING PLUGIN_MEDIA PLUGIN_MIDI PLUGIN_RECORDING PLUGIN_REMOTECONTROL PLUGIN_SCENARIO)
  string(TOLOWER "${_lib}" _lib_lc)
  target_link_libraries(score_${_lib_lc} INTERFACE score_lib_base)
endforeach()
target_link_libraries(score_plugin_engine INTERFACE score_lib_device score_lib_inspector score_lib_localtree score_lib_process score_lib_state score_plugin_dataflow score_plugin_library score_plugin_deviceexplorer score_plugin_scenario score_plugin_audio)

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

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag(-std=c++20 has_std_20_flag)
check_cxx_compiler_flag(-std=c++2a has_std_2a_flag)
check_cxx_compiler_flag(-std=c++17 has_std_17_flag)
check_cxx_compiler_flag(-std=c++1z has_std_1z_flag)

if (has_std_20_flag)
  set(CXX_VERSION_FLAG cxx_std_20)
elseif (has_std_2a_flag)
  set(CXX_VERSION_FLAG cxx_std_20)
elseif (has_std_17_flag)
  set(CXX_VERSION_FLAG cxx_std_17)
elseif (has_std_1z_flag)
  set(CXX_VERSION_FLAG cxx_std_17)
endif ()

target_compile_features(score_lib_base INTERFACE "${CXX_VERSION_FLAG}")

set(3RDPARTY_FOLDER "${SCORE_SOURCE_DIR}/3rdparty/")
set(OSSIA_3RDPARTY_FOLDER "${SCORE_SOURCE_DIR}/3rdparty/libossia/3rdparty")
set(SCORE_ROOT_SOURCE_DIR "${SCORE_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SCORE_SRC "${SCORE_SOURCE_DIR}/src")

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050800)
add_definitions(-DQT_NO_KEYWORDS)

include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/asio/asio/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/bitset2")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/brigand/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/chobo-shl/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/concurrentqueue")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/dr_libs")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/flat/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/fmt/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/flat_hash_map")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/Flicks")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/frozen/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/GSL/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/hopscotch-map/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/libremidi/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/multi_index/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/oscpack")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/rapidjson/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/rnd/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/spdlog/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/SmallFunction/smallfun/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/verdigris/src")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/variant/include")

include_directories(SYSTEM "${3RDPARTY_FOLDER}/magicitems/include/")

include_directories(SYSTEM "${SCORE_SOURCE_DIR}/3rdparty/libossia/src")
include_directories(SYSTEM "${SCORE_SOURCE_DIR}/src/lib")

include("${SCORE_SOURCE_DIR}/cmake/ScoreFunctions.cmake")
