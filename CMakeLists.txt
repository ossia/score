cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
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

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_MODULE_PATH}")

set(CMAKE_PREFIX_PATH
    "${OSSIA_SDK}/SDL2/cmake"
    "${OSSIA_SDK}/portaudio/lib/cmake"
    "${OSSIA_SDK}/portaudio/lib/cmake/portaudio"
    "${CMAKE_PREFIX_PATH}")

if(NOT SCORE_CONFIGURATION)
  if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    set(SCORE_CONFIGURATION static-debug)
  elseif(${CMAKE_BUILD_TYPE} MATCHES Release)
    set(SCORE_CONFIGURATION static-release)
  else()
    set(SCORE_CONFIGURATION developer)
  endif()
endif()
include("${SCORE_CONFIGURATION}")

project(score LANGUAGES C CXX)

cmake_policy(VERSION 3.13)
cmake_policy(SET CMP0025 NEW)
cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0079 NEW)

set(SCORE_VERSION_MAJOR 3)
set(SCORE_VERSION_MINOR 0)
set(SCORE_VERSION_PATCH 0)
set(SCORE_VERSION_EXTRA "alpha0")
set(SCORE_CODENAME "La poutine aux lardons : épistémologie d'une culture culinaire")

set(SCORE_VERSION
    "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

if(MSVC)
  # TODO check /experimental:preprocessor at some point
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /std:c++latest /permissive- /volatile:iso")
  add_definitions(/D_HAS_AUTO_PTR_ETC=1
                  /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS=1
                  /DNOMINMAX
                  /DWIN32_LEAN_AND_MEAN
                  )
else()
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fno-stack-protector ")
endif()

if(MSYS OR MINGW)
  find_package(ZLIB)
endif()

find_package(Qt5 5.12 REQUIRED
             COMPONENTS
               Core
               Widgets
               Gui
               Network
               Xml
#             OPTIONAL_COMPONENTS
#               Quick
#               QuickControls2
#               SerialPort
#               Svg
#               WebSockets
#               Qml
#               Test
)

include(3rdparty/libossia.cmake)
enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreConfiguration.cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/exprtk")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/multi_index/include")

include_directories(SYSTEM "${3RDPARTY_FOLDER}/magicitems/include/")

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

include(GenerateQMake)
include(GenerateUnity)
include(CTest)
