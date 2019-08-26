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
  set(SCORE_CONFIGURATION developer)
endif()
include("${SCORE_CONFIGURATION}")

project(score LANGUAGES C CXX)

cmake_policy(VERSION 3.13)
cmake_policy(SET CMP0025 NEW)
cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0079 NEW)

set(SCORE_VERSION_MAJOR 2)
set(SCORE_VERSION_MINOR 5)
set(SCORE_VERSION_PATCH 2)
set(SCORE_VERSION_EXTRA "")
set(SCORE_CODENAME "An illustrated guide to buying Greenland")

set(SCORE_VERSION
    "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /std:c++latest")
  add_definitions(/D_HAS_AUTO_PTR_ETC=1
                  /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS=1)
else()
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fno-stack-protector")
endif()

if(MSYS OR MINGW)
  find_package(ZLIB)
endif()

include(3rdparty/libossia.cmake)
find_package(Boost REQUIRED)
enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScoreConfiguration.cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
       "${CMAKE_CURRENT_SOURCE_DIR}/Tests/testdata"
       DESTINATION
       "${CMAKE_CURRENT_BINARY_DIR}/Tests/Integration/")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Tests")
  file(GLOB_RECURSE TESTS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.hpp")
  add_custom_target(MocksHeaders SOURCES ${TESTS_HDRS})
  setup_score_tests(Tests/Integration)
endif()

include(GenerateQMake)
include(GenerateUnity)
include(CTest)
