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

# fastgltf — vendored glTF 2.0 parser. The library auto-downloads simdjson
# on first configure (into 3rdparty/fastgltf/deps/simdjson/) unless a
# system simdjson is found via find_package.
if(NOT TARGET fastgltf)
  set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)
  add_subdirectory("${3RDPARTY_FOLDER}/fastgltf" "${CMAKE_CURRENT_BINARY_DIR}/fastgltf" EXCLUDE_FROM_ALL)
endif()

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

  Threedim/ArrayToTexture.hpp
  Threedim/ArrayToTexture.cpp
  Threedim/CubemapComposer.hpp
  Threedim/CubemapLoader.hpp
  Threedim/CubemapLoader.cpp
  Threedim/ArrayToGeometry.hpp
  Threedim/ArrayToGeometry.cpp
  Threedim/BufferLoader.hpp
  Threedim/BufferLoader.cpp
  Threedim/BufferToGeometry.hpp
  Threedim/BufferToGeometry.cpp
  Threedim/BufferToGeometry2.hpp
  Threedim/BufferToGeometry2.cpp
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
  Threedim/GeometryLoader.hpp
  Threedim/GeometryLoader.cpp
  Threedim/AssetLoader.hpp
  Threedim/AssetLoader.cpp
  Threedim/FbxParser.hpp
  Threedim/FbxParser.cpp
  Threedim/GltfParser.hpp
  Threedim/GltfParser.cpp
  Threedim/VcgImporters.hpp
  Threedim/VcgImporters.cpp
  Threedim/Camera.hpp
  Threedim/Camera.cpp
  Threedim/CameraArray.hpp
  Threedim/CameraArray.cpp
  Threedim/CameraSwitch.hpp
  Threedim/Light.hpp
  Threedim/Light.cpp
  Threedim/Transform3D.hpp
  Threedim/Transform3D.cpp
  Threedim/TransformHelper.hpp

  Threedim/ScenePreprocessor/Executor.hpp
  Threedim/ScenePreprocessor/Executor.cpp
  Threedim/ScenePreprocessor/Metadata.hpp
  Threedim/ScenePreprocessor/Process.hpp
  Threedim/ScenePreprocessor/Process.cpp

  Threedim/SceneFilter/Executor.hpp
  Threedim/SceneFilter/Executor.cpp
  Threedim/SceneFilter/Metadata.hpp
  Threedim/SceneFilter/Process.hpp
  Threedim/SceneFilter/Process.cpp

  Threedim/FlattenedSceneFilter/Executor.hpp
  Threedim/FlattenedSceneFilter/Executor.cpp
  Threedim/FlattenedSceneFilter/Metadata.hpp
  Threedim/FlattenedSceneFilter/Process.hpp
  Threedim/FlattenedSceneFilter/Process.cpp

  Threedim/MergeGeometries/Executor.hpp
  Threedim/MergeGeometries/Executor.cpp
  Threedim/MergeGeometries/Metadata.hpp
  Threedim/MergeGeometries/Process.hpp
  Threedim/MergeGeometries/Process.cpp

  Threedim/SceneGraphFilter.hpp
  Threedim/SceneGraphFilter.cpp
  Threedim/SceneSwitch.hpp
  Threedim/SceneSelector.hpp
  Threedim/SceneSelector.cpp
  Threedim/SceneGroup.hpp
  Threedim/SceneGroup.cpp
  Threedim/SceneDuplicator.hpp
  Threedim/SceneDuplicator.cpp
  Threedim/SceneFromMeshes.hpp
  Threedim/SceneFromMeshes.cpp
  Threedim/SceneInspector.hpp
  Threedim/SceneInspector.cpp
  Threedim/CreateCollection.hpp
  Threedim/CreateCollection.cpp
  Threedim/SceneResourceRoute.hpp
  Threedim/SceneResourceRoute.cpp
  Threedim/InjectBuffer.hpp
  Threedim/InjectBuffer.cpp
  Threedim/InjectTexture.hpp
  Threedim/InjectTexture.cpp
  Threedim/BufferInfo.hpp
  Threedim/TextureInfo.hpp
  Threedim/ImageLoader.hpp
  Threedim/ImageLoader.cpp
  Threedim/TangentUtils.hpp
  Threedim/BufferToGeometryCommon.hpp
  Threedim/Debug.hpp
  Threedim/MeshHelpers.hpp
  Threedim/PCLToGeometry.hpp
  Threedim/PCLToGeometry.cpp
  Threedim/Ply.hpp
  Threedim/Ply.cpp
  Threedim/Primitive.hpp
  Threedim/Primitive.cpp
  Threedim/StructureSynth.hpp
  Threedim/StructureSynth.cpp
  Threedim/TextureToBuffer.hpp
  Threedim/TinyObj.hpp
  Threedim/TinyObj.cpp
  Threedim/Vox.hpp
  Threedim/Vox.cpp
  Threedim/VoxelLoader.hpp
  Threedim/VoxelLoader.cpp
  Threedim/ModelDisplay/Executor.hpp
  Threedim/ModelDisplay/Executor.cpp
  Threedim/ModelDisplay/Metadata.hpp
  Threedim/ModelDisplay/Process.hpp
  Threedim/ModelDisplay/Process.cpp

  Threedim/RenderPipeline/Executor.hpp
  Threedim/RenderPipeline/Executor.cpp
  Threedim/RenderPipeline/Metadata.hpp
  Threedim/RenderPipeline/Process.hpp
  Threedim/RenderPipeline/Process.cpp
  Threedim/RenderPipeline/Layer.hpp

  Threedim/Splat/Executor.hpp
  Threedim/Splat/Executor.cpp
  Threedim/Splat/Metadata.hpp
  Threedim/Splat/Process.hpp
  Threedim/Splat/Process.cpp
  Threedim/Splat/GaussianSplatNode.hpp
  Threedim/Splat/GaussianSplatNode.cpp

  "${3RDPARTY_FOLDER}/miniply/miniply.cpp"
  "${3RDPARTY_FOLDER}/mikktspace/mikktspace.c"
  "${3RDPARTY_FOLDER}/ufbx/ufbx.c"
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
    "${3RDPARTY_FOLDER}/miniply"
    "${3RDPARTY_FOLDER}/mikktspace"
    "${3RDPARTY_FOLDER}/opengametools/src"
    "${3RDPARTY_FOLDER}/ufbx")

target_link_libraries(
  score_plugin_threedim PRIVATE score_plugin_engine score_plugin_avnd
                                score_plugin_gfx fmt::fmt ssynth Eigen3::Eigen
                                fastgltf::fastgltf)
