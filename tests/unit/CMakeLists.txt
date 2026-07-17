# Unit tests: pure logic, no application context. Fast to build and run.

score_add_test(test_unit_address
  SOURCES AddressParseTest.cpp
  PLUGINS score_lib_state)

# AssetTable is pure CPU logic; TextureLoader upload paths run on QRhi's Null
# backend (score_plugin_gfx links Qt::GuiPrivate PUBLIC, providing qrhi_p.h).
score_add_test(test_unit_gfx_assettable
  SOURCES AssetTableTest.cpp
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)

# libisf importer branches (shadertoy / shadertoy-json / glsl-sandbox /
# write_isf / float-input min-max inference). isf::parser is exported from
# score_plugin_gfx, whose PUBLIC include dirs provide <isf.hpp>.
score_add_test(test_unit_isf_importers
  SOURCES IsfImportersTest.cpp
  PLUGINS score_plugin_gfx)

score_add_test(test_unit_video_pixel_format
  SOURCES VideoPixelFormatTest.cpp
  PLUGINS score_plugin_gfx score_plugin_media
  LIBS avutil)

# --- image loaders (gfx TextureLoader decode + threedim LDR ImageLoader /
# ArrayToTexture) ----------------------------------------------------------
# All entry points are either SCORE_PLUGIN_GFX_EXPORTed (decodeImageFrom*)
# or header-inline (Threedim::ImageLoader::ins::image_t::process,
# ArrayToTexture::recreate), so no plugin .cpp needs to be recompiled here.
if(TARGET score_plugin_threedim)
  score_add_test(test_unit_image_loader
    SOURCES ImageLoaderTest.cpp
    PLUGINS score_plugin_threedim score_plugin_gfx score_plugin_avnd
    LIBS ${QT_PREFIX}::Gui)
endif()
# --- threedim asset loaders ------------------------------------------------
# The plugin is built with -fvisibility=hidden, so the loader entry points
# (GltfParser/FbxParser/AssetLoader::process, ObjFromString, ...) are not
# reachable through libscore_plugin_threedim.so. Compile the loader layer
# straight into the test binary instead; linking the plugins still provides
# the include dirs + the exported gfx/registry symbols.
if(TARGET score_plugin_threedim)
  set(_threedim_src "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")

  # C sources go in a separate static lib: mixing C into the test target
  # breaks the score_lib_pch REUSE_FROM (PCH is C++-only).
  add_library(test_unit_threedim_3rdparty STATIC
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/miniply/miniply.cpp"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/mikktspace/mikktspace.c"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/ufbx/ufbx.c")
  target_include_directories(test_unit_threedim_3rdparty SYSTEM PUBLIC
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/miniply"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/mikktspace"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/ufbx")
  set_target_properties(test_unit_threedim_3rdparty
    PROPERTIES FOLDER "Tests" POSITION_INDEPENDENT_CODE ON)

  score_add_test(test_unit_threedim_loaders
    SOURCES
      ThreedimLoaderTest.cpp
      "${_threedim_src}/AssetLoader.cpp"
      "${_threedim_src}/GltfParser.cpp"
      "${_threedim_src}/FbxParser.cpp"
      "${_threedim_src}/TinyObj.cpp"
      "${_threedim_src}/Ply.cpp"
      "${_threedim_src}/VcgImporters.cpp"
      "${_threedim_src}/SceneFromMeshes.cpp"
      "${_threedim_src}/PrimitiveCloud/PlyParser.cpp"
      "${_threedim_src}/PrimitiveCloud/SplatBinary.cpp"
      "${_threedim_src}/PrimitiveCloud/SpzCodec.cpp"
      "${_threedim_src}/PrimitiveCloud/SceneFromCloud.cpp"
      "${_threedim_src}/PrimitiveCloud/FormatOverride.cpp"
    PLUGINS score_plugin_threedim score_plugin_gfx score_plugin_avnd
    LIBS test_unit_threedim_3rdparty fastgltf spz "${QT_PREFIX}::Gui")
  target_include_directories(test_unit_threedim_loaders SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/vcglib"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/eigen")
endif()

# PrimitiveCloud parsers (PLY / .splat / .spz + scene helpers). The parser
# entry points are not exported from the dynamic score_plugin_threedim
# (hidden visibility), so compile the parser translation units directly
# into the test.
set(_pc_dir "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/PrimitiveCloud")
score_add_test(test_unit_primitivecloud
  SOURCES
    PrimitiveCloudTest.cpp
    "${_pc_dir}/PlyParser.cpp"
    "${_pc_dir}/SplatBinary.cpp"
    "${_pc_dir}/SpzCodec.cpp"
    "${_pc_dir}/SceneFromCloud.cpp"
    "${_pc_dir}/FormatOverride.cpp"
    "${3RDPARTY_FOLDER}/miniply/miniply.cpp"
  LIBS spz)
target_include_directories(test_unit_primitivecloud SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
  "${3RDPARTY_FOLDER}/miniply")
