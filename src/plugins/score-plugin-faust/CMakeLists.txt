project(score_plugin_faust LANGUAGES CXX)

include(ExternalProject)
score_common_setup()

if(MSVC)
  return()
endif()

if(NOT TARGET score_plugin_media)
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
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

add_dependencies(${PROJECT_NAME} faustlibs)

function(disable_var VAR)
  if(${${VAR}})
    set(${VAR}_prev "${${VAR}}" CACHE "" INTERNAL)
  endif()
  set(${VAR} 0 CACHE "" INTERNAL)
endfunction()
function(restore_var VAR)
  if(${${VAR}_prev})
    set(${VAR} "${${VAR}_prev}" CACHE "" INTERNAL)
  endif()
endfunction()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# needed for llvm::sys::initializeBlabla on arm...
if(LLVM_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PRIVATE ${LLVM_INCLUDE_DIRS})
endif()

find_path(SNDFILE_INCLUDE_DIR sndfile.h HINTS ${OSSIA_SDK}/sndfile/include)
find_library(SNDFILE_LIBRARY sndfile HINTS ${OSSIA_SDK}/sndfile/lib)
if(SNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SNDFILE_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${SNDFILE_LIBRARY})
else()
  include(FetchContent)
  disable_var(BUILD_PROGRAMS)
  disable_var(BUILD_EXAMPLES)
  disable_var(ENABLE_PACKAGE_CONFIG)
  disable_var(INSTALL_PKGCONFIG_MODULE)
  disable_var(ENABLE_EXPERIMENTAL)
  disable_var(BUILD_REGTEST)
  disable_var(BUILD_TESTING)

  FetchContent_Declare(
    sndfile
    GIT_REPOSITORY https://github.com/jcelerier/libsndfile
    )
  FetchContent_MakeAvailable(sndfile)
  if(TARGET sndfile)
    set_target_properties(sndfile PROPERTIES UNITY_BUILD 0)
  endif()

  restore_var(BUILD_TESTING)

  target_link_libraries(${PROJECT_NAME} PRIVATE SndFile::sndfile)
endif()
