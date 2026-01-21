project(score_plugin_media LANGUAGES CXX)

include(ExternalProject)
score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)
find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVDEVICE AVUTIL SWRESAMPLE SWSCALE POSTPROC)

if(UNIX AND NOT APPLE)
  find_package(ZLIB QUIET)
endif()

# Files & main target
set(HDRS ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioArray.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Libav.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Tempo.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioFileChooserWidget.hpp"

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

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/BW64/AdmData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/BW64/Bw64AdmReader.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/BW64/Bw64Drop.hpp"

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
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioTrimmer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/LibavMediaInfo.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/RMSData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/SndfileDecoder.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Mixer/MixerPanel.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Video/VideoInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/VideoDecoder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/CameraInput.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/WebCameraInput.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/LibavStreamInput.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/GStreamerCompatibility.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/GpuFormats.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Thumbnailer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/FrameQueue.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Rescale.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_media.hpp"
)

set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/DRWav.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/ChangeAudioFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Commands/MediaCommandFactory.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Inspector/AudioInspector.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundPresenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundView.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/WaveformComputer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/Drop/SoundDrop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/SoundComponent.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/BW64/Bw64AdmReader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Sound/BW64/Bw64Drop.cpp"

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

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioFileChooserWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.libav.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.libavstream.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.mmap.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.sndfile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/MediaFileHandle.waveform.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Media/LibavMediaInfo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/RMSData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/SndfileDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/Tempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/AudioTrimmer.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Mixer/MixerPanel.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Video/VideoDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/CameraInput.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/WebCameraInput.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/LibavStreamInput.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Thumbnailer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/FrameQueue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Rescale.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_media.cpp"
)

add_library(${PROJECT_NAME}
  ${SRCS} ${HDRS}
)

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Media/DRWav.cpp"
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        SKIP_UNITY_BUILD_INCLUSION ON
)

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
                     score_lib_base score_plugin_engine score_plugin_library score_plugin_audio score_plugin_dataflow
                     score_plugin_automation score_plugin_curve
)

# BW64/ADM support for object-based audio
if(TARGET bw64 AND TARGET adm)
  target_link_libraries(${PROJECT_NAME} PUBLIC bw64 adm)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SCORE_HAS_BW64_ADM)
endif()

### FFMPEG ###
if(EMSCRIPTEN)
  target_include_directories(${PROJECT_NAME} PUBLIC ${OSSIA_SDK}/ffmpeg/include)
  target_link_libraries(${PROJECT_NAME} PUBLIC
    -Wl,--start-group
    ${OSSIA_SDK}/ffmpeg/lib/libavcodec.a
    ${OSSIA_SDK}/ffmpeg/lib/libavdevice.a
    ${OSSIA_SDK}/ffmpeg/lib/libavfilter.a
    ${OSSIA_SDK}/ffmpeg/lib/libavformat.a
    ${OSSIA_SDK}/ffmpeg/lib/libavutil.a
    ${OSSIA_SDK}/ffmpeg/lib/libswscale.a
    ${OSSIA_SDK}/ffmpeg/lib/libswresample.a
    -Wl,--end-group
  )
elseif(TARGET avcodec)
  target_link_libraries(${PROJECT_NAME}
    PUBLIC
      avcodec avformat avdevice swresample swscale avutil
  ) 
  if(TARGET postproc)
    target_link_libraries(${PROJECT_NAME}
      PUBLIC postproc
    )
  endif()

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
      )

  endif()

  if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE evr mf mfplat strmiids quartz vfw32)
    if(MSVC)
      target_link_libraries(${PROJECT_NAME} PRIVATE mfplay mfreadwrite mfuuid)
    endif()
  endif()

  # If we build against homebrew's ffmpeg we do not need those
  if(OSSIA_SDK)
    find_package(ZLIB)
    if(TARGET ZLIB::ZLIB)
      target_link_libraries(${PROJECT_NAME} PRIVATE
        ZLIB::ZLIB
      )
    endif()

    find_package(BZip2)
    if(TARGET BZip2::BZip2)
      target_link_libraries(${PROJECT_NAME} PRIVATE
        BZip2::BZip2
      )
    endif()

    find_package(LibLZMA)
    if(TARGET LibLZMA::LibLZMA)
      target_link_libraries(${PROJECT_NAME} PRIVATE
        LibLZMA::LibLZMA
      )
    endif()

    if(UNIX AND NOT APPLE AND NOT WIN32)
      find_library(UDEV_LIBRARY udev REQUIRED)
      if(UDEV_LIBRARY)
          target_link_libraries(${PROJECT_NAME} PRIVATE
            ${UDEV_LIBRARY}
          )
      endif()
    endif()

    # ossia SDK 38 (ffmpeg 9) builds libavcodec / libavformat against a set of
    # codec and protocol libraries that earlier SDKs did not have: dav1d, x264,
    # x265, opus, vpx, webp, SVT-JPEG-XS, mp3lame, libxml2, SRT (which itself
    # links the SDK's openssl) and freetype+harfbuzz for the drawtext filter.
    #
    # FindFFmpeg.cmake imports only the av* archives: it asks pkg-config for the
    # include and library DIRECTORIES but never for Libs.private, so none of the
    # above reaches the link line on its own and a static score dies in a wall of
    # undefined references.
    #
    # Every entry is looked up rather than required. SDK 36 has none of them, and
    # even in 37 some are target-dependent -- Windows-on-ARM has no OpenSSL mingw
    # target, so libsrt is built there without encryption. Found => linked,
    # missing => nothing changes; that is what keeps this file building against
    # both SDKs, and it is inert for a distro/homebrew ffmpeg since the whole
    # block is under if(OSSIA_SDK).
    set(SCORE_SDK_LIBDIRS
      "${OSSIA_SDK}/sysroot/lib"     # Linux + Windows: shared dep prefix
      "${OSSIA_SDK}/sysroot/lib64"   # Linux, RedHat layout
      "${OSSIA_SDK}/lib"             # macOS: media-deps installs into the prefix
      "${OSSIA_SDK}/openssl/lib"
      "${OSSIA_SDK}/openssl/lib64"
      "${OSSIA_SDK}/freetype/lib"    # macOS keeps these in their own prefixes
      "${OSSIA_SDK}/harfbuzz/lib"
    )
    # Ordered for a static link: dependants before their dependencies.
    # libwebp needs sharpyuv, libsrt needs ssl+crypto, freetype needs harfbuzz.
    #
    # jpeg: the SDK builds libjpeg-turbo (Qt is configured -system-libjpeg), and
    # on aarch64 Linux libavdevice reaches it too -- ffmpeg's --enable-libv4l2
    # resolves libv4l2.pc statically, whose Libs.private ends in "-ljpeg".
    foreach(_sdk_lib
        dav1d x264 x265 opus vpx webpmux webp sharpyuv SvtJpegxs mp3lame xml2
        srt ssl crypto freetype harfbuzz jpeg)
      # NO_DEFAULT_PATH is deliberate: these have to be the SDK's own copies.
      # Silently linking a system libssl or libfreetype here is exactly the kind
      # of leak the SDK exists to prevent, and it would only show up as a crash
      # on someone else's machine.
      find_library(SCORE_SDK_LIB_${_sdk_lib}
        NAMES ${_sdk_lib}
        PATHS ${SCORE_SDK_LIBDIRS}
        NO_DEFAULT_PATH
      )
      mark_as_advanced(SCORE_SDK_LIB_${_sdk_lib})
      if(SCORE_SDK_LIB_${_sdk_lib})
        # PUBLIC: score-plugin-gfx links avcodec/avformat directly and picks
        # these up through this target's interface, after them, which is the
        # order a static link needs.
        target_link_libraries(${PROJECT_NAME} PUBLIC "${SCORE_SDK_LIB_${_sdk_lib}}")
      endif()
    endforeach()

    # libvpx, libwebp, libsrt and x265 all use pthreads. Being raw archive paths
    # they carry no dependency information for CMake to order around, so name
    # this last rather than relying on whatever libossia's find_package(Threads)
    # happens to place earlier in the line. No-op where the compiler implies it.
    find_package(Threads)
    if(TARGET Threads::Threads)
      target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)
    endif()

    if(WIN32)
      # System import libraries the SDK's ffmpeg now pulls in, taken from the
      # Libs: lines of the .pc files it installs:
      #   libavcodec   mediafoundation -> mfuuid, ole32, strmiids
      #   libavformat  schannel        -> secur32, ncrypt, crypt32
      #                srt             -> ws2_32, wsock32, advapi32, shell32
      #   libavutil                    -> bcrypt
      #   libavdevice                  -> psapi, uuid, oleaut32, shlwapi, gdi32
      # All are OS import libraries present in both mingw and MSVC, so listing
      # them costs nothing where they were already coming in via Qt.
      target_link_libraries(${PROJECT_NAME} PRIVATE
        mfuuid ole32 oleaut32 uuid shlwapi psapi gdi32 advapi32 shell32
        secur32 ncrypt crypt32 bcrypt ws2_32 wsock32
      )
    endif()
  endif()
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()


# Rubberband and sample rate conversion
if(TARGET rubberband)
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rubberband
  )
endif()

# For ossia releases, libavcodec is built against snappy, v4l2
# and thus expects the library to be there
if(OSSIA_SDK)
  if(TARGET Snappy::snappy)
    target_link_libraries(${PROJECT_NAME} PUBLIC Snappy::snappy)
  elseif(TARGET snappy)
    target_link_libraries(${PROJECT_NAME} PUBLIC snappy)
  endif()

  if(LINUX)
    # v4lconvert alongside v4l2: on aarch64 the SDK's ffmpeg is built
    # --enable-libv4l2 and, because it resolves libv4l2.pc with --static,
    # libavdevice's EXTRALIBS carry the whole private chain
    # "-lv4l2 -lpthread -lv4lconvert -lrt -lm -ljpeg". v4l2 and v4lconvert are
    # the distro's (they are what libv4l is); jpeg comes from the SDK, above.
    # find_library rather than a bare name so an x86_64 machine without
    # libv4lconvert.so keeps building -- that leg is --disable-libv4l2 anyway.
    find_library(SCORE_V4LCONVERT_LIBRARY v4lconvert)
    mark_as_advanced(SCORE_V4LCONVERT_LIBRARY)
    if(SCORE_V4LCONVERT_LIBRARY)
      target_link_libraries(${PROJECT_NAME} PUBLIC v4l2 "${SCORE_V4LCONVERT_LIBRARY}")
    else()
      target_link_libraries(${PROJECT_NAME} PUBLIC v4l2)
    endif()
  endif()
endif()

if(TARGET SampleRate::samplerate)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      SampleRate::samplerate
    )
elseif(TARGET samplerate)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      samplerate
    )
endif()

if(TARGET SndFile::sndfile)
  target_link_libraries(${PROJECT_NAME} PRIVATE SndFile::sndfile)
elseif(TARGET sndfile)
  target_link_libraries(${PROJECT_NAME} PRIVATE sndfile)
endif()

setup_score_plugin(${PROJECT_NAME})
