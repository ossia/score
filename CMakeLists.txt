cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
  cmake_policy(VERSION 3.18)
endif()

set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Configurations"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Configurations/travis"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia/cmake/cmake-modules")

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_MODULE_PATH}")
if(APPLE AND EXISTS /usr/local/opt/qt5)
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt5")
endif()

if(OSSIA_SDK)
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
endif()

project(score LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJCXX)
endif()
if(NOT QT_VERSION)
  set(QT_VERSION Qt5 5.9)
  set(QT_PREFIX Qt5)
else()
  set(QT_PREFIX Qt6)
endif()

set(SCORE_VERSION_MAJOR 3)
set(SCORE_VERSION_MINOR 0)
set(SCORE_VERSION_PATCH 0)
set(SCORE_VERSION_EXTRA "a24")
set(SCORE_CODENAME "Biscaresse")

set(SCORE_VERSION "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

if(NOT SCORE_DYNAMIC_PLUGINS)
  set(SCORE_STATIC_PLUGINS ON)
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  message(FATAL_ERROR "No build type defined. Please set CMAKE_BUILD_TYPE to Debug, Release, RelWithDebInfo...")
  return()
endif()

include(all-plugins)

if(SCORE_DISABLED_PLUGINS)
  list(REMOVE_ITEM SCORE_PLUGINS_TO_BUILD ${SCORE_DISABLED_PLUGINS})
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 5000)

if(MSVC)
  # TODO check /experimental:preprocessor at some point
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MP /std:c++latest /Zc:__cplusplus /permissive- /volatile:iso")
  add_definitions(/D_HAS_AUTO_PTR_ETC=1
                  /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS=1
                  /DNOMINMAX
                  /DWIN32_LEAN_AND_MEAN
                  )
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
)

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreConfiguration.cmake")

include(3rdparty/libossia.cmake)
add_subdirectory(3rdparty/QCodeEditor)

disable_qt_plugins(QCodeEditor)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(SCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SCORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")

include_directories("${SCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/variant/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/brigand")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/hopscotch-map/src")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/multi_index/include")

include_directories(SYSTEM "${3RDPARTY_FOLDER}/magicitems/include/")
if(BOOST_ROOT)
  include_directories(SYSTEM "${BOOST_ROOT}")
endif()

set(SCORE_LIBRARIES_LIST CACHE INTERNAL "List of libraries")
set(SCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins")

add_subdirectory(src)

if(SCORE_EMBEDDED)
  return()
endif()
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

# include(GenerateQMake)
# include(GenerateUnity)
include(CTest)
