cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
project(score_plugin_threedim LANGUAGES CXX)

if(NOT TARGET score_lib_base)
  include(ScoreExternalAddon)
endif()

if(NOT TARGET score_plugin_avnd)
  return()
endif()
if(NOT TARGET score_plugin_gfx)
  return()
endif()

find_package(${QT_VERSION} REQUIRED COMPONENTS Xml)

# libssynth
add_library(
  ssynth STATIC
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Parser/EisenParser.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Parser/Preprocessor.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Parser/Tokenizer.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/Action.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/AmbiguousRule.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/Builder.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/CustomRule.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/PrimitiveRule.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/RuleSet.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/State.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/Transformation.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/Rendering/TemplateRenderer.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Model/Rendering/ObjRenderer.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/ColorPool.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/ColorUtils.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/Logging.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/MiniParser.cpp"
  "${3RDPARTY_FOLDER}/libssynth/src/ssynth/RandomStreams.cpp")
target_include_directories(ssynth SYSTEM
                           PRIVATE "${3RDPARTY_FOLDER}/libssynth/src")

target_link_libraries(ssynth PRIVATE "${QT_PREFIX}::Core" "${QT_PREFIX}::Gui"
                                     "${QT_PREFIX}::Xml" score_lib_base)

if(NOT MSVC)
  target_compile_options(ssynth PRIVATE -w)
endif()

# threedim addon
add_library(
  score_plugin_threedim
  Threedim/ModelDisplay/ModelDisplayNode.hpp
  Threedim/ModelDisplay/ModelDisplayNode.cpp
  
  Threedim/RenderPipeline/RenderPipelineNode.hpp
  Threedim/RenderPipeline/RenderPipelineNode.cpp
  
  Threedim/ArrayToBuffer.hpp
  Threedim/ArrayToBuffer.cpp
  Threedim/ArrayToGeometry.hpp
  Threedim/ArrayToGeometry.cpp
  Threedim/BufferToGeometry.hpp
  Threedim/BufferToGeometry.cpp
  Threedim/GeometryInfo.hpp
  Threedim/GeometryInfo.cpp
  Threedim/GeometryPacker.hpp
  Threedim/GeometryPacker.cpp
  Threedim/GeometryToBuffer.hpp
  Threedim/GeometryToBuffer.cpp
  Threedim/GeometryToBufferStrategies.hpp
  Threedim/GeometryToBufferStrategies.cpp
  Threedim/Noise.hpp
  Threedim/Noise.cpp
  Threedim/ObjLoader.hpp
  Threedim/ObjLoader.cpp
  Threedim/PCLToGeometry.hpp
  Threedim/PCLToGeometry.cpp
  Threedim/Ply.hpp
  Threedim/Ply.cpp
  Threedim/Primitive.hpp
  Threedim/Primitive.cpp
  Threedim/StructureSynth.hpp
  Threedim/StructureSynth.cpp
  Threedim/TinyObj.hpp
  Threedim/TinyObj.cpp
  Threedim/ModelDisplay/Executor.hpp
  Threedim/ModelDisplay/Executor.cpp
  Threedim/ModelDisplay/Metadata.hpp
  Threedim/ModelDisplay/Process.hpp
  Threedim/ModelDisplay/Process.cpp
  Threedim/ModelDisplay/Layer.hpp
  
  Threedim/RenderPipeline/Executor.hpp
  Threedim/RenderPipeline/Executor.cpp
  Threedim/RenderPipeline/Metadata.hpp
  Threedim/RenderPipeline/Process.hpp
  Threedim/RenderPipeline/Process.cpp
  Threedim/RenderPipeline/Layer.hpp
  
  "${3RDPARTY_FOLDER}/miniply/miniply.cpp"
  score_plugin_threedim.hpp
  score_plugin_threedim.cpp)

set_property(TARGET score_plugin_threedim PROPERTY SCORE_CUSTOM_PCH 1)
setup_score_plugin(score_plugin_threedim)

score_generate_command_list_file(score_plugin_threedim "Threedim/RenderPipeline/Process.hpp") 

target_include_directories(
  score_plugin_threedim SYSTEM
  PRIVATE
    "${3RDPARTY_FOLDER}/libssynth/src"
    "${3RDPARTY_FOLDER}/vcglib"
    "${3RDPARTY_FOLDER}/miniply")

target_link_libraries(
  score_plugin_threedim PRIVATE score_plugin_engine score_plugin_avnd
                                score_plugin_gfx fmt::fmt ssynth Eigen3::Eigen)
