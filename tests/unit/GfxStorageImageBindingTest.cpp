// What the engine reads off a storage image declaration. The highest binding a
// shader declares is read from its GLSL: samplers and buffer blocks whose
// members merely look like image types do not count; the OpenGL image-unit
// warning (tests/gfx/GfxStorageImageBindings) is decided from it. A format
// qualifier maps to the QRhi format of the placeholder bound to an unwired
// image (tests/gfx/GfxRawRasterPlaceholderFormat).
#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("storage image bindings are read from the GLSL", "[gfx][binding]")
{
  using score::gfx::maxStorageImageBinding;
  CHECK(maxStorageImageBinding(u"layout(binding = 3) uniform sampler2D t;") == -1);
  CHECK(
      maxStorageImageBinding(
          u"layout(binding = 3, rgba8) uniform readonly image2D a;\n"
          u"layout(binding = 9, r32ui) restrict uniform uimage3D b;\n"
          u"layout(std430, binding = 12) buffer B { vec4 image2D_like[]; };")
      == 9);
}

TEST_CASE(
    "image format qualifiers map to their QRhi format", "[gfx][raster][placeholder]")
{
  using score::gfx::imageFormatFromQualifier;
  CHECK(imageFormatFromQualifier("rgba8") == QRhiTexture::RGBA8);
  CHECK(imageFormatFromQualifier("R32F") == QRhiTexture::R32F);
  CHECK(imageFormatFromQualifier("rgba16f") == QRhiTexture::RGBA16F);
  CHECK(imageFormatFromQualifier("r32ui") == QRhiTexture::R32UI);
  CHECK(imageFormatFromQualifier("r32i") == QRhiTexture::R32SI);
  CHECK(imageFormatFromQualifier("rgba32ui") == QRhiTexture::RGBA32UI);
}
