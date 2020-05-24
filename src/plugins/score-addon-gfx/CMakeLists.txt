project(score_addon_gfx LANGUAGES C CXX)

find_package(Qt5 5.15 OPTIONAL_COMPONENTS ShaderTools)
if(NOT TARGET Qt5::ShaderTools)
  return()
endif()

# General initialization
score_common_setup()


find_package(Qt5Gui 5.15 REQUIRED Private)

if(NOT TARGET avformat)
  find_package(FFmpeg COMPONENTS  AVCODEC AVFORMAT AVUTIL)
endif()

add_subdirectory(3rdparty/libisf)

# Source files
set(HDRS
    Gfx/CommandFactory.hpp

    Gfx/Filter/Executor.hpp
    Gfx/Filter/LocalTree.hpp
    Gfx/Filter/Metadata.hpp
    Gfx/Filter/Process.hpp
    Gfx/Filter/Layer.hpp

    Gfx/Mesh/Executor.hpp
    Gfx/Mesh/Inspector.hpp
    Gfx/Mesh/LocalTree.hpp
    Gfx/Mesh/Metadata.hpp
    Gfx/Mesh/Presenter.hpp
    Gfx/Mesh/Process.hpp
    Gfx/Mesh/View.hpp
    Gfx/Mesh/Layer.hpp

    Gfx/Video/Executor.hpp
    Gfx/Video/Inspector.hpp
    Gfx/Video/LocalTree.hpp
    Gfx/Video/Metadata.hpp
    Gfx/Video/Presenter.hpp
    Gfx/Video/Process.hpp
    Gfx/Video/View.hpp
    Gfx/Video/Layer.hpp

    Gfx/Images/Executor.hpp
    Gfx/Images/Inspector.hpp
    Gfx/Images/LocalTree.hpp
    Gfx/Images/Metadata.hpp
    Gfx/Images/Presenter.hpp
    Gfx/Images/Process.hpp
    Gfx/Images/View.hpp
    Gfx/Images/Layer.hpp

    Gfx/Graph/main.cpp
    Gfx/Graph/window.hpp
    Gfx/Graph/renderstate.hpp
    Gfx/Graph/scene.hpp
    Gfx/Graph/nodes.hpp
    Gfx/Graph/node.hpp
    Gfx/Graph/filternode.hpp
    Gfx/Graph/isfnode.hpp
    Gfx/Graph/graph.hpp
    Gfx/Graph/uniforms.hpp
    Gfx/Graph/mesh.hpp
    Gfx/Graph/renderer.hpp
    Gfx/Graph/videonode.hpp
    Gfx/Graph/phongnode.hpp
    Gfx/Graph/imagenode.hpp
    Gfx/Graph/shadercache.hpp

    Gfx/GfxApplicationPlugin.hpp
    Gfx/GfxContext.hpp
    Gfx/GfxExec.hpp
    Gfx/GfxDevice.hpp
    Gfx/TexturePort.hpp

    Gfx/CameraDevice.hpp

    3rdparty/icosphere/Icosphere.h
    # Gfx/graph/hap/source/hap.h

    score_addon_gfx.hpp
)

set(SRCS
    Gfx/Filter/Executor.cpp
    Gfx/Filter/LocalTree.cpp
    Gfx/Filter/Process.cpp

    Gfx/Mesh/Executor.cpp
    Gfx/Mesh/Inspector.cpp
    Gfx/Mesh/LocalTree.cpp
    Gfx/Mesh/Presenter.cpp
    Gfx/Mesh/Process.cpp
    Gfx/Mesh/View.cpp

    Gfx/Video/Executor.cpp
    Gfx/Video/Inspector.cpp
    Gfx/Video/LocalTree.cpp
    Gfx/Video/Presenter.cpp
    Gfx/Video/Process.cpp
    Gfx/Video/View.cpp

    Gfx/Images/Executor.cpp
    Gfx/Images/Inspector.cpp
    Gfx/Images/LocalTree.cpp
    Gfx/Images/Presenter.cpp
    Gfx/Images/Process.cpp
    Gfx/Images/View.cpp

    Gfx/Graph/node.cpp
    Gfx/Graph/graph.cpp
    Gfx/Graph/renderer.cpp
    Gfx/Graph/mesh.cpp
    Gfx/Graph/isfnode.cpp
    Gfx/Graph/screennode.cpp
    Gfx/Graph/phongnode.cpp

    Gfx/GfxApplicationPlugin.cpp
    Gfx/GfxDevice.cpp
    Gfx/TexturePort.cpp

    Gfx/CameraDevice.cpp
    # Gfx/graph/hap/source/hap.c

    3rdparty/icosphere/Icosphere.cpp

    score_addon_gfx.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC
  score_lib_base  score_lib_localtree score_lib_process score_plugin_dataflow score_plugin_engine
   Qt5::ShaderTools  Qt5::GuiPrivate
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  avcodec avformat swresample avutil score_plugin_media isf)

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
  add_subdirectory("${3RDPARTY_FOLDER}/Spout" "${CMAKE_CURRENT_BINARY_DIR}/Spout")
  target_link_libraries(${PROJECT_NAME} PRIVATE Spout)
  target_sources(${PROJECT_NAME}
      PRIVATE
        Gfx/SpoutDevice.hpp
        Gfx/SpoutDevice.cpp
  )
endif()

# Target-specific options
setup_score_plugin(${PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -std=c++2a)
