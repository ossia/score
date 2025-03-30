project(score_plugin_faust LANGUAGES CXX)
include(ExternalProject)
score_common_setup()

if(MSVC)
  return()
endif()

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

if(NOT TARGET score_plugin_media)
  return()
endif()

if(SCORE_HAS_SANITIZERS)
  message("Not building Faust plug-in as ASAN is not supported")
  return()
endif()

# Packages
find_package(Faust)
if(NOT FAUST_FOUND)
  return()
endif()

if(UNIX AND NOT APPLE)
  find_package(ZLIB QUIET)
endif()

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Faust/DSPWrapper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Faust/Utils.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Faust/EffectModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Faust/Library.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Faust/Commands.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_faust.hpp"
)
set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Faust/EffectModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_faust.cpp"
)

add_library(${PROJECT_NAME}
  ${SRCS} ${HDRS}
)

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

if(NOT "${FAUST_LIBRARIES}" MATCHES "/usr/l.*")
  target_compile_definitions(${PROJECT_NAME}
    PUBLIC
      FAUSTFLOAT=double
  )
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
                     score_plugin_media
                     ${FAUST_LIBRARIES}
                     samplerate
)
target_include_directories(${PROJECT_NAME} PRIVATE ${FAUST_INCLUDE_DIR})

setup_score_plugin(${PROJECT_NAME})

message("Downloading Faust libs...")

ExternalProject_Add(
    faustlibs
    PREFIX ""
    GIT_REPOSITORY https://github.com/grame-cncm/faustlibraries
    GIT_TAG 5ee53574d0b910695b9ff3c534b9aa60f7a26d3d
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

add_dependencies(${PROJECT_NAME} faustlibs)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# needed for llvm::sys::initializeBlabla on arm...
if(LLVM_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PRIVATE ${LLVM_INCLUDE_DIRS})
endif()

if(TARGET SndFile::sndfile)
  target_link_libraries(${PROJECT_NAME} PRIVATE SndFile::sndfile)
endif()

if(SCORE_FAUST_TESTER)
  add_executable(FaustTester Tests/FaustTester.cpp)
  target_compile_features(FaustTester PRIVATE cxx_std_20)
  target_include_directories(FaustTester PRIVATE  ${FAUST_INCLUDE_DIR})
  target_link_libraries(FaustTester PRIVATE score_plugin_faust)
endif()
