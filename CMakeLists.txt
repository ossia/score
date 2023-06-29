cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
  cmake_policy(VERSION 3.26)
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
  set(CMAKE_PREFIX_PATH
      "${OSSIA_SDK}"
      "${OSSIA_SDK}/SDL2"
      "${OSSIA_SDK}/SDL2/cmake"
      "${OSSIA_SDK}/qt6-static-debug"
      "${OSSIA_SDK}/qt6-static"
      "${OSSIA_SDK}/llvm"
      "${OSSIA_SDK}/llvm-libs"
      "${OSSIA_SDK}/fftw"
      "${OSSIA_SDK}/zlib"
      "${OSSIA_SDK}/faust"
      "${OSSIA_SDK}/openssl"
      "${OSSIA_SDK}/freetype"
      "${OSSIA_SDK}/ysfx"
      "${OSSIA_SDK}/portaudio/lib64/cmake"
      "${OSSIA_SDK}/portaudio/lib64/cmake/portaudio"
      "${OSSIA_SDK}/portaudio/lib/cmake"
      "${OSSIA_SDK}/portaudio/lib/cmake/portaudio"
      "${CMAKE_PREFIX_PATH}")

  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OSSIA_SDK}/ffmpeg/lib/pkgconfig")

  include_directories("${OSSIA_SDK}/pipewire/include/pipewire-0.3")
  include_directories("${OSSIA_SDK}/pipewire/include/spa-0.2")
endif()

project(score LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

set(QT_VERSION Qt6 6.2)

# QT_PREFIX: Qt5 / Qt6
list(GET QT_VERSION 0 QT_PREFIX)

set(SCORE_VERSION_MAJOR 3)
set(SCORE_VERSION_MINOR 1)
set(SCORE_VERSION_PATCH 11)
set(SCORE_VERSION_EXTRA "")
set(SCORE_CODENAME "las cabritas")

set(SCORE_VERSION "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

if(NOT SCORE_DYNAMIC_PLUGINS)
  set(SCORE_STATIC_PLUGINS ON)
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  message("No build type defined. Please set CMAKE_BUILD_TYPE to Debug, Release, RelWithDebInfo...")
endif()

include(all-plugins)

if(SCORE_DISABLED_PLUGINS)
  list(REMOVE_ITEM SCORE_PLUGINS_TO_BUILD ${SCORE_DISABLED_PLUGINS})
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 5000)

if(CMAKE_VERSION VERSION_GREATER 3.19)
  set(CMAKE_PCH_INSTANTIATE_TEMPLATES 1)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)

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
               OpenGLWidgets
               Qml
               ShaderTools
)

if(EMSCRIPTEN)
  find_package(${QT_VERSION} REQUIRED COMPONENTS QmlIntegration)
endif()

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreConfiguration.cmake")

include(3rdparty/3rdparty.cmake)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(SCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SCORE_AVND_SOURCE_DIR "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd")
set(SCORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")

include_directories("${SCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")

include_directories(SYSTEM "${3RDPARTY_FOLDER}/magicitems/include/")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/avendish/include/")
if(BOOST_ROOT)
  include_directories(SYSTEM "${BOOST_ROOT}")
endif()

set(SCORE_LIBRARIES_LIST CACHE INTERNAL "List of libraries")
set(SCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins")

add_subdirectory(src)

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
