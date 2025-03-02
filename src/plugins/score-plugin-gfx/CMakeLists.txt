project(score_plugin_gfx LANGUAGES C CXX)

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

find_package(${QT_VERSION} OPTIONAL_COMPONENTS ShaderTools)
if(NOT TARGET "${QT_PREFIX}::ShaderTools")
  message("Video plug-in requires Qt Shader Tools (5.14+)")
  return()
endif()

# General initialization
score_common_setup()

find_package(${QT_VERSION} REQUIRED Gui)

if(NOT TARGET avformat AND NOT EMSCRIPTEN)
  find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE)
endif()

if(WIN32 AND "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86_64|AMD64)")
  set(SCORE_HAS_SPOUT 1)
endif()

if(SCORE_HAS_SPOUT)
set(SPOUT_SRCS
    Gfx/Spout/SpoutInput.hpp
    Gfx/Spout/SpoutInput.cpp
    Gfx/Spout/SpoutOutput.hpp
    Gfx/Spout/SpoutOutput.cpp
)
endif()

if(APPLE)
enable_language(OBJC)
enable_language(OBJCXX)
set(SYPHON_SRCS
    Gfx/Syphon/SyphonInput.hpp
    Gfx/Syphon/SyphonInput.mm
    Gfx/Syphon/SyphonOutput.hpp
    Gfx/Syphon/SyphonOutput.mm
)
endif()

find_package(freenect2)
if(NOT freenect2_LIBRARIES)
  set(freenect2_FOUND FALSE)
endif()
if(freenect2_FOUND)
  set(KINECT2_SRCS Gfx/Kinect2Device.hpp Gfx/Kinect2Device.cpp)
endif()

set(LIBAV_SRCS
  Gfx/Libav/LibavEncoder.hpp
  Gfx/Libav/LibavEncoderNode.hpp
  Gfx/Libav/LibavOutputDevice.hpp
  Gfx/Libav/LibavOutputSettings.hpp
  Gfx/Libav/LibavOutputStream.hpp

  Gfx/Libav/LibavOutputDevice.cpp
  Gfx/Libav/LibavEncoder.cpp
  Gfx/Libav/LibavEncoderNode.cpp
)

# Source files
set(HDRS
    Gfx/CommandFactory.hpp

    Gfx/Filter/Executor.hpp
    Gfx/Filter/Metadata.hpp
    Gfx/Filter/Process.hpp
    Gfx/Filter/Layer.hpp
    Gfx/Filter/Library.hpp
    Gfx/Filter/PreviewWidget.hpp

    Gfx/GeometryFilter/Executor.hpp
    Gfx/GeometryFilter/Metadata.hpp
    Gfx/GeometryFilter/Process.hpp
    Gfx/GeometryFilter/Layer.hpp

    Gfx/Video/Executor.hpp
    Gfx/Video/Inspector.hpp
    Gfx/Video/Metadata.hpp
    Gfx/Video/Presenter.hpp
    Gfx/Video/Process.hpp
    Gfx/Video/View.hpp
    Gfx/Video/Layer.hpp

    Gfx/Images/Executor.hpp
    Gfx/Images/Metadata.hpp
    Gfx/Images/Process.hpp
    Gfx/Images/ImageListChooser.hpp

    Gfx/Text/Executor.hpp
    Gfx/Text/Metadata.hpp
    Gfx/Text/Process.hpp
    Gfx/Text/Layer.hpp

    Gfx/Graph/CommonUBOs.hpp
    Gfx/Graph/CustomMesh.hpp
    Gfx/Graph/DepthNode.hpp
    Gfx/Graph/GeometryFilterNode.hpp
    Gfx/Graph/GeometryFilterNodeRenderer.hpp
    Gfx/Graph/OutputNode.hpp
    Gfx/Graph/Scale.hpp
    Gfx/Graph/Window.hpp
    Gfx/Graph/RenderState.hpp
    Gfx/Graph/Node.hpp
    Gfx/Graph/ISFNode.hpp
    Gfx/Graph/RenderedISFNode.hpp
    Gfx/Graph/Graph.hpp
    Gfx/Graph/Uniforms.hpp
    Gfx/Graph/Mesh.hpp
    Gfx/Graph/RenderList.hpp
    Gfx/Graph/NodeRenderer.hpp
    Gfx/Graph/VideoNode.hpp
    Gfx/Graph/VideoNodeRenderer.hpp
    Gfx/Graph/PhongNode.hpp
    Gfx/Graph/ImageNode.hpp
    Gfx/Graph/TexgenNode.hpp
    Gfx/Graph/ScreenNode.hpp
    Gfx/Graph/TextNode.hpp
    Gfx/Graph/ShaderCache.hpp
    Gfx/Graph/Utils.hpp
    Gfx/Graph/decoders/GPUVideoDecoder.hpp
    Gfx/Graph/decoders/HAP.hpp
    Gfx/Graph/decoders/NV12.hpp
    Gfx/Graph/decoders/RGBA.hpp
    Gfx/Graph/decoders/YUV420.hpp
    Gfx/Graph/decoders/YUV420P10.hpp
    Gfx/Graph/decoders/YUV420P12.hpp
    Gfx/Graph/decoders/YUV422.hpp
    Gfx/Graph/decoders/YUV422P10.hpp
    Gfx/Graph/decoders/YUV422P12.hpp
    Gfx/Graph/decoders/YUYV422.hpp

    Gfx/Settings/Model.hpp
    Gfx/Settings/Presenter.hpp
    Gfx/Settings/View.hpp
    Gfx/Settings/Factory.hpp

    Gfx/GfxApplicationPlugin.hpp
    Gfx/GfxContext.hpp
    Gfx/GfxExecNode.hpp
    Gfx/GfxExecContext.hpp
    Gfx/GfxParameter.hpp
    Gfx/GfxDevice.hpp
    Gfx/GfxInputDevice.hpp
    Gfx/InvertYRenderer.hpp
    Gfx/TexturePort.hpp
    Gfx/ShaderProgram.hpp
    Gfx/SharedInputSettings.hpp
    Gfx/SharedOutputSettings.hpp

    Gfx/CameraSettings.hpp
    Gfx/CameraDevice.hpp
    Gfx/CameraDeviceEnumerator.hpp
    Gfx/WindowDevice.hpp
    # Gfx/graph/hap/source/hap.h

    score_plugin_gfx.hpp
)

set(SRCS
    ${SPOUT_SRCS}
    ${SYPHON_SRCS}
    ${KINECT2_SRCS}
    ${LIBAV_SRCS}

    Gfx/Filter/Executor.cpp
    Gfx/Filter/Library.cpp
    Gfx/Filter/Process.cpp
    Gfx/Filter/PreviewWidget.cpp

    Gfx/GeometryFilter/Executor.cpp
    Gfx/GeometryFilter/Process.cpp

    Gfx/Video/Executor.cpp
    Gfx/Video/Inspector.cpp
    Gfx/Video/Presenter.cpp
    Gfx/Video/Process.cpp
    Gfx/Video/View.cpp

    Gfx/Images/Executor.cpp
    Gfx/Images/Process.cpp
    Gfx/Images/ImageListChooser.cpp

    Gfx/Text/Executor.cpp
    Gfx/Text/Process.cpp

    Gfx/Graph/decoders/GPUVideoDecoder.cpp
    Gfx/Graph/decoders/HAP.cpp
    Gfx/Graph/GeometryFilterNode.cpp
    Gfx/Graph/GeometryFilterNodeRenderer.cpp
    Gfx/Graph/Node.cpp
    Gfx/Graph/Graph.cpp
    Gfx/Graph/RenderList.cpp
    Gfx/Graph/Mesh.cpp
    Gfx/Graph/ImageNode.cpp
    Gfx/Graph/ISFNode.cpp
    Gfx/Graph/RenderedISFNode.cpp
    Gfx/Graph/VideoNode.cpp
    Gfx/Graph/VideoNodeRenderer.cpp
    Gfx/Graph/NodeRenderer.cpp
    Gfx/Graph/OutputNode.cpp
    Gfx/Graph/ScreenNode.cpp
    Gfx/Graph/ShaderCache.cpp
    Gfx/Graph/TextNode.cpp
    Gfx/Graph/PhongNode.cpp
    Gfx/Graph/Utils.cpp
    Gfx/Graph/Window.cpp

    Gfx/GfxApplicationPlugin.cpp
    Gfx/GfxExecNode.cpp
    Gfx/GfxExecutionAction.cpp
    Gfx/GfxContext.cpp
    Gfx/GfxDevice.cpp
    Gfx/TexturePort.cpp
    Gfx/ShaderProgram.cpp
    Gfx/SharedInputSettings.cpp
    Gfx/SharedOutputSettings.cpp

    Gfx/InvertYRenderer.cpp
    Gfx/CameraDevice.cpp
    Gfx/WindowDevice.cpp
    Gfx/GfxInputDevice.cpp


    Gfx/Settings/Model.cpp
    Gfx/Settings/Presenter.cpp
    Gfx/Settings/View.cpp

    score_plugin_gfx.cpp

    3rdparty/libisf/src/sajson.h
    3rdparty/libisf/src/isf.hpp
    3rdparty/libisf/src/isf.cpp

    "${3RDPARTY_FOLDER}/glsl-parser/glsl_ast.c"
    "${3RDPARTY_FOLDER}/glsl-parser/glsl.parser.c"
    "${3RDPARTY_FOLDER}/glsl-parser/glsl.lexer.c"
)
set_source_files_properties(
    "${3RDPARTY_FOLDER}/glsl-parser/glsl_ast.c"
    "${3RDPARTY_FOLDER}/glsl-parser/glsl.parser.c"
    "${3RDPARTY_FOLDER}/glsl-parser/glsl.lexer.c"
    PROPERTIES
      SKIP_PRECOMPILE_HEADERS ON
      SKIP_UNITY_BUILD_INCLUSION ON
)

# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_include_directories(${PROJECT_NAME}
  PUBLIC
    3rdparty/libisf/src
)

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC
  score_lib_base  score_lib_localtree score_lib_process score_plugin_dataflow score_plugin_engine
   ${QT_PREFIX}::ShaderTools ${QT_PREFIX}::ShaderToolsPrivate  ${QT_PREFIX}::GuiPrivate
   "$<BUILD_INTERFACE:r8brain>"
)

# for HAP
if(TARGET Snappy::snappy)
  target_link_libraries(${PROJECT_NAME} PUBLIC Snappy::snappy)
  list(APPEND SCORE_FEATURES_LIST hap)
elseif(TARGET snappy)
  target_link_libraries(${PROJECT_NAME} PUBLIC snappy)
  list(APPEND SCORE_FEATURES_LIST hap)
endif()

if(EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_media)
else()
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      avcodec avformat swresample avutil avdevice score_plugin_media
  )
endif()

if(TARGET shmdata)
  target_sources(${PROJECT_NAME}
    PRIVATE
      Gfx/Shmdata/ShmdataInputDevice.hpp
      Gfx/Shmdata/ShmdataInputDevice.cpp
      Gfx/Shmdata/ShmdataOutputDevice.hpp
      Gfx/Shmdata/ShmdataOutputDevice.cpp
  )
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      shmdata
  )
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE
      SCORE_HAS_SHMDATA
  )
  list(APPEND SCORE_FEATURES_LIST shmdata)
endif()

if(TARGET sh4lt)
  target_sources(${PROJECT_NAME}
    PRIVATE
      Gfx/Sh4lt/Sh4ltInputDevice.hpp
      Gfx/Sh4lt/Sh4ltInputDevice.cpp
      Gfx/Sh4lt/Sh4ltOutputDevice.hpp
      Gfx/Sh4lt/Sh4ltOutputDevice.cpp
  )
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      sh4lt
  )
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE
      SCORE_HAS_SH4LT
  )
  list(APPEND SCORE_FEATURES_LIST sh4lt)
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

    target_compile_options(${PROJECT_NAME}
      PRIVATE
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc-exceptions>
        $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-weak>
    )
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

    # If we build against homebrew's ffmpeg we do not need those
    if(OSSIA_SDK)
      find_package(BZip2 REQUIRED)
      find_package(ZLIB REQUIRED)
      target_link_libraries(${PROJECT_NAME} PRIVATE
        BZip2::BZip2
        ZLIB::ZLIB
      )
    endif()
endif()

if(SCORE_HAS_SPOUT)
  add_subdirectory("${3RDPARTY_FOLDER}/Spout" "${CMAKE_CURRENT_BINARY_DIR}/Spout")
  target_link_libraries(${PROJECT_NAME} PRIVATE Spout)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_SPOUT)
  list(APPEND SCORE_FEATURES_LIST spout)
endif()

if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE Gfx/CameraDevice.win32.cpp)
elseif(APPLE)
  target_sources(${PROJECT_NAME} PRIVATE Gfx/CameraDevice.avf.mm)

  set_source_files_properties(
    Gfx/CameraDevice.avf.mm
    PROPERTIES
      SKIP_UNITY_BUILD_INCLUSION 1
  )

  add_subdirectory("${3RDPARTY_FOLDER}/Syphon-Framework" "${CMAKE_CURRENT_BINARY_DIR}/Syphon")
  target_link_libraries(${PROJECT_NAME} PRIVATE Syphon)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_SYPHON)
  list(APPEND SCORE_FEATURES_LIST syphon)
elseif(EMSCRIPTEN OR BSD)
  target_sources(${PROJECT_NAME} PRIVATE Gfx/CameraDevice.libav.cpp)
else()
  target_sources(${PROJECT_NAME} PRIVATE Gfx/CameraDevice.v4l2.cpp)

  if(OSSIA_SDK)
    if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES aarch64)
      target_link_libraries(${PROJECT_NAME} PRIVATE v4l2 bz2)
    endif()
  endif()
  list(APPEND SCORE_FEATURES_LIST v4l2)
endif()

if(freenect2_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_FREENECT2)
  target_include_directories(${PROJECT_NAME} PRIVATE ${freenect2_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${freenect2_LIBRARIES})
endif()

# Target-specific options
setup_score_plugin(${PROJECT_NAME})

if(SCORE_ISF_TESTER)
add_executable(ISFTester tests/ISFTester.cpp)
target_link_libraries(ISFTester PRIVATE score_plugin_gfx)
endif()

if(SCORE_VIDEO_TESTER)
add_executable(VideoTester tests/VideoTester.cpp)
target_link_libraries(VideoTester PRIVATE score_plugin_gfx)
endif()

