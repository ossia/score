project(score_plugin_media LANGUAGES CXX)

include(ExternalProject)
score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)
find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVDEVICE AVUTIL SWRESAMPLE SWSCALE POSTPROC)
if(NOT FFmpeg_FOUND)
    return()
endif()

if(UNIX AND NOT APPLE)
  find_package(ZLIB QUIET)
endif()

# Files & main target
set(HDRS ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioArray.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Tempo.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/Factory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/WaveformComputer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/QImagePool.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundLibraryHandler.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/View.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/Factory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Commands.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Metadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Executor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Factory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Inspector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/View.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Commands.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Metadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Executor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Factory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Inspector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Model.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroCommands.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroExecutor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroView.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/ApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/RMSData.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Mixer/MixerPanel.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Video/VideoInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/VideoDecoder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/CameraInput.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Thumbnailer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/FrameQueue.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_media.hpp"
)

set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/WaveformComputer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundComponent.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/Model.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/Presenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Settings/View.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Executor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Model.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Factory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/View.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Executor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Merger/Model.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroExecutor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Metro/MetroModel.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/RMSData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Tempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Mixer/MixerPanel.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Video/VideoDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/CameraInput.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Thumbnailer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/FrameQueue.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_media.cpp"
)

add_library(${PROJECT_NAME}
  ${SRCS} ${HDRS}
)

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
                     score_lib_base score_plugin_engine score_plugin_library score_plugin_audio score_plugin_dataflow
)

### FFMPEG ###
target_link_libraries(${PROJECT_NAME} PRIVATE avcodec avformat avdevice swresample swscale postproc avutil)

if(APPLE)
    find_library(QuartzCore_FK QuartzCore)
    find_library(AppKit_FK AppKit)
    find_library(OpenGL_FK OpenGL)
    find_library(Foundation_FK Foundation)
    find_library(AVFoundation_FK AVFoundation)
    find_library(CoreVideo_FK CoreVideo)
    find_library(CoreMedia_FK CoreMedia)
    find_library(VideoDecodeAcceleration_FK VideoDecodeAcceleration)
    find_library(CoreFoundation_FK CoreFoundation)
    find_library(AudioToolbox_FK AudioToolbox)
    find_library(VideoToolbox_FK VideoToolbox)
    find_package(BZip2 REQUIRED)
    find_package(ZLIB REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${QuartzCore_FK}
        ${AppKit_FK}
        ${OpenGL_FK}
        ${Foundation_FK}
        ${AVFoundation_FK}
        ${CoreVideo_FK}
        ${CoreMedia_FK}
        ${VideoDecodeAcceleration_FK}
        ${CoreFoundation_FK}
        ${AudioToolbox_FK}
        ${VideoToolbox_FK}
        BZip2::BZip2
        ZLIB::ZLIB
        )
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE evr mf mfplat strmiids quartz vfw32)
    if(MSVC)
      target_link_libraries(${PROJECT_NAME} PRIVATE mfplay mfreadwrite mfuuid)
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})

# Rubberband and sample rate conversion
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rubberband
    samplerate
)

setup_score_plugin(${PROJECT_NAME})
