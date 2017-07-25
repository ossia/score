cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_media LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Multimedia)

if(APPLE)
find_package(Sndfile)
endif()

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libaudiotool/src/libwatermark/*.h")
# Files & main target
set(HDRS ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioArray.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/CreateSoundBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundLayer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundLayerPanelProxy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_media.hpp"
)

set(SRCS

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModelSerialization.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_media.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     Qt5::Core Qt5::Widgets Qt5::Multimedia
                     iscore_lib_base
)

if(NOT ANDROID)
if(APPLE)
target_link_libraries(${PROJECT_NAME} PUBLIC -L/usr/local/lib)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC
            iscore_plugin_midi
            ${SNDFILE_LIBRARIES}
            mediainfo)
endif()

if(${SNDFILE_FOUND})
    target_include_directories(${PROJECT_NAME} PRIVATE
                               ${SNDFILE_INCLUDE_DIR})
endif()

setup_iscore_plugin(${PROJECT_NAME})
