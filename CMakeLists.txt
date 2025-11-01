cmake_minimum_required(VERSION 3.25...3.31 FATAL_ERROR)
if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Configurations"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia/cmake/cmake-modules")

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_MODULE_PATH}")
if(OSSIA_SDK)
  if(UNIX AND NOT APPLE)
    set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)
  endif()

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
      "${OSSIA_SDK}/SDL2/lib64/cmake/SDL2"
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

  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OSSIA_SDK}/sysroot/lib/pkgconfig")

  if(EXISTS "${OSSIA_SDK}/ffmpeg/lib64/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OSSIA_SDK}/ffmpeg/lib64/pkgconfig")
  else()
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OSSIA_SDK}/ffmpeg/lib/pkgconfig")
  endif()

  if(EXISTS "${OSSIA_SDK}/pipewire/include/pipewire-0.3")
    include_directories("${OSSIA_SDK}/pipewire/include/pipewire-0.3")
    include_directories("${OSSIA_SDK}/pipewire/include/spa-0.2")
  endif()
endif()

project(score LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()
set(QT_VERSION Qt6 6.2)

if(SCORE_MACOS_ONLY_SYSTEM_LIBARIES)
  set(CMAKE_IGNORE_PATH "/opt/homebrew;/usr/local;/usr/X11R6;/usr/pkg;/sw;/opt/local")
  set(CMAKE_SYSTEM_IGNORE_PATH "/opt/homebrew;/usr/local;/usr/X11R6;/usr/pkg;/sw;/opt/local")
  set(CMAKE_IGNORE_PREFIX_PATH "/opt/homebrew;/usr/local;/usr/X11R6;/usr/pkg;/sw;/opt/local")
  set(CMAKE_SYSTEM_IGNORE_PREFIX_PATH "/opt/homebrew;/usr/local;/usr/X11R6;/usr/pkg;/sw;/opt/local")
  foreach(xpath "/opt/homebrew;/usr/local;/usr/X11R6;/usr/pkg;/opt;/sw;/opt/local;/")
    list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH ${xpath})
    list(REMOVE_ITEM CMAKE_PREFIX_PATH ${xpath})
  endforeach()
  foreach(xpath ${CMAKE_SYSTEM_PREFIX_PATH})
    if("${xpath}" MATCHES ".*homebrew.*")
      list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH ${xpath})
      list(REMOVE_ITEM CMAKE_PREFIX_PATH ${xpath})
    endif()
  endforeach()
endif()

# QT_PREFIX: Qt5 / Qt6
list(GET QT_VERSION 0 QT_PREFIX)

set(SCORE_VERSION_MAJOR 3)
set(SCORE_VERSION_MINOR 7)
set(SCORE_VERSION_PATCH 1)
set(SCORE_VERSION_EXTRA "")
set(SCORE_CODENAME "Beirut")

set(SCORE_VERSION "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

if(NOT SCORE_DYNAMIC_PLUGINS)
  set(SCORE_STATIC_PLUGINS ON)
endif()

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT "${is_multi_config}")
  if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    message("No build type defined. Please set CMAKE_BUILD_TYPE to Debug, Release, RelWithDebInfo...")
  endif()
endif()

include(all-plugins)

if(SCORE_DISABLE_PLUGINS)
  set(SCORE_DISABLED_PLUGINS "${SCORE_DISABLE_PLUGINS}")
endif()

if(SCORE_DISABLED_PLUGINS)
  # handle complicated CI situations with unclear amount of quoting: we pass , instead of ;
  string(REPLACE "," ";" SCORE_DISABLE_PLUGINS "${SCORE_DISABLE_PLUGINS}")

  list(REMOVE_ITEM SCORE_PLUGINS_TO_BUILD ${SCORE_DISABLED_PLUGINS})
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 5000)

if(CMAKE_VERSION VERSION_GREATER 3.19)
  set(CMAKE_PCH_INSTANTIATE_TEMPLATES 1)
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
  if("${CMAKE_CXX_COMPILER}" MATCHES ".*ccache.*")
    set(CMAKE_CXX_SCAN_FOR_MODULES 0)
  endif()
  if(WIN32)
    # https://gitlab.kitware.com/cmake/cmake/-/issues/26662
    set(CMAKE_CXX_SCAN_FOR_MODULES 0)
  endif()

  if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    set(CMAKE_CXX_SCAN_FOR_MODULES 0)
  endif()
  if(NOT CMAKE_CXX_COMPILER_VERSION)
    set(CMAKE_CXX_SCAN_FOR_MODULES 0)
  endif()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 21.0)
    set(CMAKE_CXX_SCAN_FOR_MODULES 0)
  endif()
else()
  set(CMAKE_CXX_SCAN_FOR_MODULES 0)
endif()

set(CMAKE_CXX_SCAN_FOR_MODULES 0)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

if(MSVC)
  if(NOT OSSIA_SDK)
    message(FATAL_ERROR "
For building on MSVC, it is required to extract:
     https://github.com/ossia/sdk/releases/download/sdk18/sdk-msvc-release.zip
in c:\\score-sdk-msvc and pass -DOSSIA_SDK=c:\\score-sdk-msvc to CMake.
For Debug builds, use score-msvc-debug.zip and extract it to c:\\score-sdk-msvc-debug.
Note that the builds made through MSYS2 using clang are better as more feature are supported
and clang optimizes more than MSVC.
")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MP /std:c++latest /Zc:__cplusplus /Zc:preprocessor /permissive- /volatile:iso")
  add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS=1
                  /DNOMINMAX
                  /DWIN32_LEAN_AND_MEAN
                  )
elseif(APPLE AND SCORE_PCH)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -faligned-allocation ")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -faligned-allocation ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -faligned-allocation ")
endif()

if(SCORE_PCH)
  if(EMSCRIPTEN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mno-sse3 ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2 -mno-sse3 ")
  elseif(NOT MSVC)
    # Needed due to kfrlib injecting these flags:
    # error: current translation unit is compiled with the target feature '+sse2' but the AST file was not
    # error: current translation unit is compiled with the target feature '-sse3' but the AST file was not
    if(CMAKE_SYSTEM_PROCESSOR MATCHES x86)
      if(APPLE)
        set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -msse4.1 ")
        set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -msse4.1 ")
      endif()
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1 ")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1 ")
    endif()
  endif()
endif()

if(MSYS OR MINGW)
  find_package(ZLIB)
endif()

find_package(${QT_VERSION} REQUIRED
  COMPONENTS
    Core
    Widgets
    Gui
    Network
    Xml
    StateMachine
    OpenGL
    OpenGLWidgets
    Qml
    ShaderTools Quick QmlModels
  OPTIONAL_COMPONENTS
    WebSockets
    Svg
    CorePrivate
    WidgetsPrivate
    GuiPrivate
    QmlPrivate
    QuickPrivate
    ShaderToolsPrivate
)

if(EMSCRIPTEN)
  find_package(${QT_VERSION} REQUIRED COMPONENTS QmlIntegration)
endif()

if(UNIX AND NOT APPLE AND NOT WIN32 AND NOT EMSCRIPTEN)
  find_package(X11 QUIET)
endif()

enable_testing()

set(SCORE_LIBRARIES_LIST CACHE INTERNAL "List of libraries")
set(SCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins")
set(SCORE_FEATURES_LIST CACHE INTERNAL "List of features")

unset(SCORE_BUNDLEUTILITIES_DIRS_LIST CACHE)
unset(SCORE_BUNDLEUTILITIES_LIB_LIST CACHE)
unset(SCORE_BUNDLEUTILITIES_PLUGIN_LIST CACHE)
set(SCORE_BUNDLEUTILITIES_DIRS_LIST CACHE INTERNAL "List of .dylibs paths to add to macOS bundle")
set(SCORE_BUNDLEUTILITIES_LIB_LIST CACHE INTERNAL "List of .dylibs to add to macOS bundle")
set(SCORE_BUNDLEUTILITIES_PLUGIN_LIST CACHE INTERNAL "List of plugins to add to macOS bundle")

include(ScoreFunctions)
include(ScoreAvndHelper)
include(ScoreConfiguration)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(OSSIA_3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia/3rdparty/")
set(SCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SCORE_AVND_SOURCE_DIR "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd")
set(SCORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")

include(3rdparty/3rdparty.cmake)

include_directories("${SCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")

include_directories(SYSTEM "${3RDPARTY_FOLDER}/magicitems/include/")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/avendish/include/")


add_subdirectory(src)

include(cmake/ScoreFeatureCheck.cmake)

if(SCORE_TESTBED)
  add_subdirectory(Tests/testbed)
endif()

if(EXISTS Documentation/Models/score.qmodel)
  add_custom_target(Docs SOURCES Documentation/Models/score.qmodel)
endif()

if(INTEGRATION_TESTING)
  file(COPY
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata"
       DESTINATION
       "${CMAKE_CURRENT_BINARY_DIR}/tests/Integration/")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/tests")
  file(GLOB_RECURSE TESTS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp")
  add_custom_target(MocksHeaders SOURCES ${TESTS_HDRS})
  setup_score_tests(tests/Integration)
endif()

include(CTest)
