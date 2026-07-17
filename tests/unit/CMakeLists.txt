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
