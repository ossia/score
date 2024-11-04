# Useful variables
set(3RDPARTY_FOLDER "${SCORE_SOURCE_DIR}/3rdparty/")
set(OSSIA_3RDPARTY_FOLDER "${SCORE_SOURCE_DIR}/3rdparty/libossia/3rdparty")
set(SCORE_ROOT_SOURCE_DIR "${SCORE_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SCORE_SRC "${SCORE_SOURCE_DIR}/src")
set(SCORE_AVND_SOURCE_DIR "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd")

if(WIN32)
  # On Windows there's no such thing as undefined dynamic lookup, and 
  # in developer builds we don't have an implib so we can just build a static library.
  set(BUILD_SHARED_LIBS 0)
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
    "${OSSIA_SDK}/SDL2"
    "${OSSIA_SDK}/qt6-static-debug"
    "${OSSIA_SDK}/qt6-static"
    "${OSSIA_SDK}/llvm"
    "${OSSIA_SDK}/llvm-libs"
    "${OSSIA_SDK}/fftw"
    "${OSSIA_SDK}/zlib"
    "${OSSIA_SDK}/faust"
    "${OSSIA_SDK}/openssl"
    "${OSSIA_SDK}/freetype"
    "${OSSIA_SDK}/harfbuzz"
    "${OSSIA_SDK}/ysfx"
    "${OSSIA_SDK}/pipewire"
    "${OSSIA_SDK}/portaudio"
    "${OSSIA_SDK}/portaudio/lib"
    "${OSSIA_SDK}/portaudio/lib/cmake"
    "${OSSIA_SDK}/sysroot"
    "${OSSIA_SDK}/sysroot/lib"
    "${OSSIA_SDK}/sysroot/lib/cmake"
    "${OSSIA_SDK}/sysroot/lib/cmake/freetype"
    "${OSSIA_SDK}/sysroot/lib/cmake/harfbuzz"
    "${OSSIA_SDK}/sysroot/lib/cmake/liblzma"
    "${OSSIA_SDK}/sysroot/lib/cmake/Snappy"
    "${OSSIA_SDK}/sysroot/lib/cmake/zstd"

    "${OSSIA_SDK}/SDL2/lib64"
    "${OSSIA_SDK}/qt6-static-debug/lib64"
    "${OSSIA_SDK}/qt6-static/lib64"
    "${OSSIA_SDK}/llvm-libs/lib64"
    "${OSSIA_SDK}/fftw/lib64"
    "${OSSIA_SDK}/zlib/lib64"
    "${OSSIA_SDK}/faust/lib64"
    "${OSSIA_SDK}/openssl/lib64"
    "${OSSIA_SDK}/freetype/lib64"
    "${OSSIA_SDK}/harfbuzz/lib64"
    "${OSSIA_SDK}/ysfx/lib64"
    "${OSSIA_SDK}/portaudio/lib64"
    "${OSSIA_SDK}/portaudio/lib64/cmake"
    "${CMAKE_PREFIX_PATH}")

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OSSIA_SDK}/ffmpeg/lib/pkgconfig")


find_package(Qt6 6.2 REQUIRED COMPONENTS
  Core
  Widgets
  Gui
  Network
  Qml
  StateMachine
)

find_package(Qt6 6.2 OPTIONAL_COMPONENTS ShaderTools)

find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVDEVICE AVUTIL SWRESAMPLE SWSCALE POSTPROC)

# ossia-config.hpp
file(CONFIGURE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ossia-config.hpp"
  CONTENT "#pragma once
  // ABI-breaking language features
  #define OSSIA_SHARED_MUTEX_AVAILABLE

  // Protocols supported by the build
  #define OSSIA_PROTOCOL_AUDIO
  #define OSSIA_PROTOCOL_MIDI
  #define OSSIA_PROTOCOL_OSC
  #define OSSIA_PROTOCOL_MINUIT
  #define OSSIA_PROTOCOL_OSCQUERY
  #define OSSIA_PROTOCOL_HTTP
  #define OSSIA_PROTOCOL_WEBSOCKETS
  #define OSSIA_PROTOCOL_SERIAL
  /* #undef OSSIA_PROTOCOL_PHIDGETS */
  /* #undef OSSIA_PROTOCOL_LEAPMOTION */
  #define OSSIA_PROTOCOL_JOYSTICK
  #define OSSIA_PROTOCOL_WIIMOTE
  #define OSSIA_PROTOCOL_ARTNET

  // Additional features
  #define OSSIA_DNSSD
  #define OSSIA_QT
  #define OSSIA_QML
  #define OSSIA_DATAFLOW
  /* #undef OSSIA_C */
  /* #undef OSSIA_QML_DEVICE */
  /* #undef OSSIA_QML_SCORE */
  #define OSSIA_EDITOR
  #define OSSIA_PARALLEL
  #define OSSIA_SCENARIO_DATAFLOW

  // FFT support
  #define OSSIA_ENABLE_FFT
  #define OSSIA_ENABLE_KFR

  #define OSSIA_FFT_KFR

  #define OSSIA_HAS_FMT
  #define OSSIA_HAS_RE2
  #define OSSIA_HAS_CTRE
  #define OSSIA_HAS_RAPIDFUZZ

  #define OSSIA_CALLBACK_CONTAINER_MUTEX std::mutex
")

foreach(_lib ${SCORE_PLUGINS})
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
  message(" - including  ${SCORE_SOURCE_DIR}/src/plugins/score-${_lib_folder}")
endforeach()

# Additional headers generated by score's build system..
file(GENERATE
  OUTPUT
    zipdownloader_export.h
  CONTENT
    "#pragma once\n#include <score_lib_base_export.h>\n#define ZIPDOWNLOADER_EXPORT SCORE_LIB_BASE_EXPORT\n"
)

file(GENERATE
  OUTPUT
    rnd_export.h
  CONTENT
    "#pragma once\n#include <score_lib_base_export.h>\n#define RND_EXPORT SCORE_LIB_BASE_EXPORT\n"
)

target_link_libraries(score_lib_base
  INTERFACE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Qml Qt6::WidgetsPrivate Qt6::GuiPrivate Qt6::CorePrivate
)

if(TARGET Qt6::ShaderTools)
  target_link_libraries(score_plugin_gfx INTERFACE Qt6::ShaderTools Qt6::ShaderToolsPrivate)
endif()


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

target_compile_definitions(score_lib_base INTERFACE
  BOOST_NO_RTTI=1
  BOOST_MATH_DISABLE_FLOAT128=1
  BOOST_ASIO_DISABLE_CONCEPTS=1
  BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
  BOOST_MULTI_INDEX_ENABLE_SAFE_MODE

  RAPIDJSON_HAS_STDSTRING=1
  # SCORE_DEBUG
  TINYSPLINE_DOUBLE_PRECISION
  FFTW_DOUBLE_ONLY

  FMT_HEADER_ONLY=1

  FMT_USE_LONG_DOUBLE=0
  FMT_USE_INT128=0
  FMT_USE_FLOAT128=0
  FMT_STATIC_THOUSANDS_SEPARATOR=1

  SPDLOG_FMT_EXTERNAL=1

  SPDLOG_NO_DATETIME=1
  SPDLOG_NO_THREAD_ID=1
  SPDLOG_NO_NAME=1

  SPDLOG_DEBUG_ON=1
  SPDLOG_TRACE_ON=1
)

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050800)
add_definitions(-DQT_NO_KEYWORDS)

include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/brigand/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/concurrentqueue")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/dr_libs")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/fmt/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/Flicks")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/libremidi/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/mdspan/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/oscpack")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/rapidjson/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/rnd/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/spdlog/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/SmallFunction/smallfun/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/span/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/tuplet/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/unordered_dense/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/verdigris/src")

include_directories(SYSTEM "${OSSIA_SDK}/boost/include")

include_directories(SYSTEM "${3RDPARTY_FOLDER}/magicitems/include/")

include_directories(SYSTEM "${SCORE_SOURCE_DIR}/3rdparty/libossia/src")
include_directories(SYSTEM "${SCORE_SOURCE_DIR}/3rdparty/avendish/include")
include_directories(SYSTEM "${SCORE_SOURCE_DIR}/src/lib")

function(ossia_set_visibility TheTarget)
  set_target_properties(${TheTarget} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1
  )
endfunction()

include("${SCORE_SOURCE_DIR}/cmake/ScoreFunctions.cmake")

function(setup_score_addon)
    cmake_parse_arguments(SETUP_ADDON "" "TARGET;NAME;METADATA" "" ${ARGN})

    setup_score_common_lib_features("${SETUP_ADDON_TARGET}")

    set(ADDON_FOLDER "${CMAKE_BINARY_DIR}/addons/${SETUP_ADDON_NAME}/")
    set(ADDON_PLATFORM "${SCORE_PLUGIN_PLATFORM}")
    set(ADDON_FILENAME "${SETUP_ADDON_NAME}-${SCORE_PLUGIN_SUFFIX}")

    set_target_properties(${AddonTarget} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${ADDON_FOLDER}/"
        PREFIX ""
        SUFFIX ""
        OUTPUT_NAME "${ADDON_FILENAME}")
    configure_file("${SETUP_ADDON_METADATA}" "${ADDON_FOLDER}/localaddon.json")

endfunction()
