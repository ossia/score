cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_audio LANGUAGES CXX)

find_package(Sndfile)
find_package(RtAudio)
find_library(FAUST_LIBRARY faust)
message("${FAUST_LIBRARY}")
message("${SNDFILE_LIBRARY}")
message("${RtAudio_LIBRARY}")
if(NOT FAUST_LIBRARY OR NOT SNDFILE_LIBRARY OR NOT RtAudio_LIBRARY)
message("audio plugin requires faust, libsndfile, portaudio")
return()
endif()

set(CMAKE_AUTOMOC OFF)
include_directories(3rdparty/libaudiotool/src/)

set(CMAKE_AUTOMOC ON)
iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Multimedia)


file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libaudiotool/src/libwatermark/*.h")
# Files & main target
set(HDRS ${HDRS}
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioProcess.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioProcessMetadata.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioProcessModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioEngine.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioBlock.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioDocumentPlugin.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_audio.hpp"
)

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioProcessMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioEngine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioBlock.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_audio.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE
                     Qt5::Core Qt5::Widgets Qt5::Multimedia
                     iscore_lib_base iscore_plugin_ossia iscore_lib_dummyprocess
                     ${FAUST_LIBRARY} ${RtAudio_LIBRARY} ${SNDFILE_LIBRARIES})

setup_iscore_plugin(${PROJECT_NAME})
