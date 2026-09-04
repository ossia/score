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

include("${3RDPARTY_FOLDER}/ssynth.cmake")
include("${3RDPARTY_FOLDER}/miniply.cmake")
include("${3RDPARTY_FOLDER}/mikktspace.cmake")
include("${3RDPARTY_FOLDER}/vcglib.cmake")
include("${3RDPARTY_FOLDER}/opengametools.cmake")

# fastgltf — vendored glTF 2.0 parser. Its configure step file(LOCK)s and then
# downloads the simdjson single-header unless a simdjson::simdjson target
# already exists. Pre-seeding deps/ is not enough: the pinned fastgltf wants
# 4.6.2 and re-downloads anything older, and the lock is taken before the
# version is even checked, so a filesystem without POSIX locks fails there too.
# Providing the target from the vendored copy skips that block entirely.
# fastgltf's export set cannot reference a target outside it, which is what
# FASTGLTF_ENABLE_INSTALL is documented for.
if(NOT TARGET simdjson::simdjson)
  add_library(score_simdjson STATIC "${3RDPARTY_FOLDER}/simdjson/simdjson.cpp")
  target_include_directories(score_simdjson PUBLIC "${3RDPARTY_FOLDER}/simdjson")
  set_target_properties(score_simdjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_library(simdjson::simdjson ALIAS score_simdjson)
endif()
if(NOT TARGET fastgltf)
  set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)
  set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory("${3RDPARTY_FOLDER}/fastgltf" "${CMAKE_CURRENT_BINARY_DIR}/fastgltf" EXCLUDE_FROM_ALL)
endif()

# spz — Niantic / Adobe reference SPZ decoder for compressed 3DGS files.
# v1-3 only (v4/ZSTD stubbed; see 3rdparty/spz/CMakeLists.txt). Pulls
# in zlib via ZLIB::ZLIB.
if(NOT TARGET spz)
  set(SPZ_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(SPZ_BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
  set(SPZ_BUILD_EXTENSIONS OFF CACHE BOOL "" FORCE)

  # spz falls back to FetchContent for zlib and zstd when find_package misses,
  # and a fetched dependency's own CMakeLists is then part of our configure --
  # including its add_test() calls. zlib 1.3.2 defaults ZLIB_BUILD_TESTING to
  # ON, which put seventeen zlib_* entries into score's ctest: zlib_example,
  # zlib_install, zlib_find_package_* and zlib_add_subdirectory_*. They are
  # EXCLUDE_FROM_ALL, so their executables never build and every one of them
  # reports Not Run, permanently, in every full-suite run. They are not ours to
  # pass and they are not ours to fix; they must not be registered at all.
  set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
  # NOT ZLIB_BUILD_SHARED OFF, however tempting: spz/CMakeLists.txt:43 sets
  # zlib_linkage_target to the SHARED target name `zlib` on its FetchContent
  # path, and zlib 1.3.2 only defines that target when ZLIB_BUILD_SHARED is on.
  # Turning it off deletes the target, so the bare name survives to the link
  # line and mold reports "library not found: zlib" when relinking
  # score_plugin_threedim. Only ZLIB_BUILD_TESTING is needed to keep zlib's own
  # ctest entries out of our suite, which is the whole point here.
  # Belt and braces for any other dependency spz may fetch: the CTest module
  # gates enable_testing()/add_test() on BUILD_TESTING for projects that use it.
  set(old_BUILD_TESTING ${BUILD_TESTING})
  set(BUILD_TESTING OFF)

  add_subdirectory("${3RDPARTY_FOLDER}/spz" "${CMAKE_CURRENT_BINARY_DIR}/spz" EXCLUDE_FROM_ALL)

  set(BUILD_TESTING ${old_BUILD_TESTING})

  if(MSVC)
    # The prebuilt sysroot's zconf.h includes <unistd.h> unconditionally;
    # provide a stub so spz's zlib usage compiles with MSVC / clang-cl.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/spz-msvc-compat/unistd.h"
         "#include <io.h>\n#include <process.h>\n")
    target_include_directories(spz PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/spz-msvc-compat")
  endif()
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
  Threedim/TagAs.hpp
  Threedim/TagAs.cpp
  Threedim/PBRMesh.hpp
  Threedim/PBRMesh.cpp
  Threedim/MaterialOverride.hpp
  Threedim/MaterialOverride.cpp
  Threedim/ConfigurePrimitive.hpp
  Threedim/ConfigurePrimitive.cpp
  Threedim/Instancer.hpp
  Threedim/Instancer.cpp
  Threedim/ShadowCascadeSetup.hpp
  Threedim/ShadowCascadeSetup.cpp
  Threedim/EnvironmentLoader.hpp
  Threedim/EnvironmentLoader.cpp
  Threedim/AnimationPlayer.hpp
  Threedim/AnimationPlayer.cpp
  Threedim/HumanoidPose.hpp
  Threedim/HumanoidPresets.hpp
  Threedim/HumanoidRetarget.hpp
  Threedim/HumanoidSourceAdapters.hpp
  Threedim/HumanoidSourceMaps.hpp
  Threedim/InverseKinematics.hpp
  Threedim/TextToMesh.hpp
  Threedim/TextToMesh.cpp
  Threedim/TextToTexture.hpp
  Threedim/ExtractBuffer2.hpp
  Threedim/ExtractBuffer2.cpp
  Threedim/ExtractSceneBuffer.hpp
  Threedim/ExtractSceneBuffer.cpp
  Threedim/ExtractTexture.hpp
  Threedim/ExtractTexture.cpp
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
  Threedim/PrimitiveCloud/PlyParser.hpp
  Threedim/PrimitiveCloud/PlyParser.cpp
  Threedim/PrimitiveCloud/SplatBinary.hpp
  Threedim/PrimitiveCloud/SplatBinary.cpp
  Threedim/PrimitiveCloud/SpzCodec.hpp
  Threedim/PrimitiveCloud/SpzCodec.cpp
  Threedim/PrimitiveCloud/SceneFromCloud.hpp
  Threedim/PrimitiveCloud/SceneFromCloud.cpp
  Threedim/PrimitiveCloud/FormatOverride.hpp
  Threedim/PrimitiveCloud/FormatOverride.cpp
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

  "${3RDPARTY_FOLDER}/ufbx/ufbx.c"
  score_plugin_threedim.hpp
  score_plugin_threedim.cpp)

set_property(TARGET score_plugin_threedim PROPERTY SCORE_CUSTOM_PCH 1)
setup_score_plugin(score_plugin_threedim)

score_generate_command_list_file(score_plugin_threedim "Threedim/RenderPipeline/Process.hpp") 

# ufbx.c is compiled straight into this plug-in, so it still needs its header
# path here. libssynth / vcglib / miniply / mikktspace / opengametools now come
# in as targets from their own .cmake files and carry their own include dirs.
target_include_directories(
  score_plugin_threedim SYSTEM
  PRIVATE
    "${3RDPARTY_FOLDER}/ufbx")

target_link_libraries(
  score_plugin_threedim PRIVATE score_plugin_engine score_plugin_avnd
                                score_plugin_gfx fmt::fmt ssynth Eigen3::Eigen
                                miniply mikktspace vcglib opengametools
                                fastgltf::fastgltf spz)
