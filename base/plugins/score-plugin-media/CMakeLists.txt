cmake_minimum_required(VERSION 3.1)
project(score_plugin_media LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Multimedia)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/ffmpeg/lib/pkgconfig" )
find_package(FFmpeg COMPONENTS  AVCODEC AVFORMAT AVUTIL SWRESAMPLE)
if(NOT FFmpeg_FOUND)
    return()
endif()
set(SCORE_AUDIO_PLUGINS 1)
if(SCORE_AUDIO_PLUGINS)
  find_package(Lilv)
#  find_package(Faust)
endif()

# Files & main target
set(HDRS ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioArray.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/CreateSoundBox.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/CreateEffectAutomation.hpp"
#    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/EditFaustEffect.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/InsertEffect.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundLayer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundComponent.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Input/InputModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Input/InputFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Input/InputMetadata.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/EffectProcessFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/EffectProcessMetadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/EffectProcessModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/EffectParameters.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/EffectModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/EffectComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/EffectFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Widgets/EffectSlider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Widgets/EffectWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Widgets/EffectListWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Missing/MissingEffectModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Inspector/EffectInspector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Generic/Effect.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Commands.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Metadata.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Executor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Factory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Inspector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/View.hpp"


    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/ApplicationPlugin.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_media.hpp"
)

set(SRCS

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModelSerialization.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.cpp"
    #    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/EditFaustEffect.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/InsertEffect.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundComponent.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Input/InputModel.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/EffectProcessModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/EffectModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/EffectFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Widgets/EffectSlider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Widgets/EffectWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Widgets/EffectListWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Effect/Missing/MissingEffectModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Inspector/EffectInspector.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Commands.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Executor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Inspector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Model.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/Presenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Step/View.cpp"



    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/ApplicationPlugin.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_media.cpp"
)


if(TARGET Faust)
    set(FAUST_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Faust/FaustEffectModel.hpp"
        )
    set(FAUST_SRCS
      "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/Faust/FaustEffectModel.cpp"
    )
endif()

if(TARGET Lilv)
    set(LV2_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/LV2/LV2EffectModel.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/LV2/lv2_atom_helpers.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/LV2/LV2Context.hpp"
        )
    set(LV2_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/LV2/LV2EffectModel.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/LV2/LV2Context.cpp"
    )
endif()

find_package(Vst2SDK)
if(TARGET Vst2SDK)
    set(VST_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/VSTLoader.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/VSTEffectModel.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/VSTNode.hpp"
        )
    set(VST_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/VSTLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/VSTEffectModel.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/VSTNode.cpp"
        )

    if(APPLE)
        set(VST_HDRS ${VST_HDRS}
            "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/AppleUI/vstwindow.h"
            )
        set(VST_SRCS ${VST_SRCS}
            "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/AppleUI/vstwindow.m"
            "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/AppleUI/vst2x.mm"
            )
    elseif(UNIX)
      set(VST_SRCS ${VST_SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/LinuxUI/vst.cpp"
        )
    elseif(MSVC)
      set(VST_SRCS ${VST_SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/Media/Effect/VST/WinUI/vst.cpp"
        )
    endif()
endif()
add_library(${PROJECT_NAME}
  ${SRCS} ${HDRS}
  ${FAUST_SRCS} ${FAUST_HDRS}
  ${LV2_SRCS} ${LV2_HDRS}
  ${VST_SRCS} ${VST_HDRS}
  ${FAUST_HDRS} ${FAUST_SRCS}
)

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     Qt5::Core Qt5::Widgets Qt5::Multimedia
                     score_lib_base score_plugin_loop score_plugin_engine
)

### FFMPEG ###
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})

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

if(TARGET Vst2SDK)
    target_link_libraries(${PROJECT_NAME} PRIVATE Vst2SDK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_VST2)
    if(UNIX AND NOT APPLE)
      target_link_libraries(${PROJECT_NAME} PRIVATE dl)
    endif()
endif()
if(TARGET Lilv)
    target_link_libraries(${PROJECT_NAME} PRIVATE Lilv)
    target_include_directories(${PROJECT_NAME} PRIVATE 3rdparty/readerwriterqueue)
endif()

if(${Faust_FOUND})
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_FAUST)
  target_link_libraries(${PROJECT_NAME}  PUBLIC ${FAUST_LIBRARIES})
  target_include_directories(${PROJECT_NAME}  PRIVATE ${FAUST_INCLUDE_DIR})
endif()

setup_score_plugin(${PROJECT_NAME})
