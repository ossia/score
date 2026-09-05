// =============================================================================
// L3 GPU render + readback — CSF CUBE and ARRAY storage-image compute write,
// sampled downstream, across every RHI backend.
//
// Extends the 3D-image test (CsfImage3d.cpp) to the other two LAYERED storage
// kinds. A CSF compute pass writes every face of an imageCube (csf-cube-image-
// write.cs) / every layer of an image2DArray (csf-array-image-write.cs) with a
// distinct bright tint; a downstream samplerCube / sampler2DArray reader inspects
// individual faces/layers in four quadrants.
//
// Qt's GL backend, before 6.10, binds cube maps and 2D texture arrays
// non-layered exactly as it does 3D textures
// (`layered = CubeMap || ThreeDimensional || TextureArray`), so
// dispatchComputeLayeredImages has to re-bind all three: an imageStore into an
// imageCube or image2DArray otherwise writes only face/layer 0 and every other
// face or layer reads back black. Vulkan is unaffected.
//
// The quadrant sampling a face/layer past 0 (BR = +Z face / layer 3) must be
// non-black on BOTH backends. The +X-face / layer-0 quadrant (TL) is written on
// either path and is a sanity anchor.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_cube_array
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_cube_array
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

using score::test::gfx::IsfResult;
using score::test::gfx::ReadbackImage;

IsfResult render(score::gfx::GraphicsApi backend, std::vector<QString> chain)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_isf_chain(backend, std::move(chain));
  });
  return r;
}

// Sum of RGB at a point (0..765). A written face/layer is bright (>= ~190 total);
// an unwritten one reads back (0,0,0).
int brightness(const ReadbackImage& img, int x, int y)
{
  const auto p = img.at(x, y);
  return int(p[0]) + int(p[1]) + int(p[2]);
}
}

TEST_CASE("csf imageCube compute write is non-black on every face", "[gfx][l3][csf][cube]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r
      = render(backend, {corpus("csf-cube-image-write.cs"), corpus("csf-cube-image-read.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = score::test::gfx::compute_shader_skip_reason(backend))
    SKIP(why);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // Quadrant sample points (image is 64x64): TL=+X face0, BR=+Z face4.
  const int qx0 = img.width / 4, qx1 = 3 * img.width / 4;
  const int qy0 = img.height / 4, qy1 = 3 * img.height / 4;
  const int tl = brightness(img, qx0, qy0); // +X face 0  (written even pre-fix)
  const int tr = brightness(img, qx1, qy0); // -X face 1
  const int bl = brightness(img, qx0, qy1); // +Y face 2
  const int br = brightness(img, qx1, qy1); // +Z face 4  (THE guard)
  INFO("faces  +X=" << tl << " -X=" << tr << " +Y=" << bl << " +Z=" << br);

  // Sanity: face 0 is bright on every backend / engine version.
  CHECK(tl > 120);
  // Faces past 0 must be written too.
  CHECK(br > 120); // +Z (face 4)
  CHECK(tr > 120); // -X (face 1)
  CHECK(bl > 120); // +Y (face 2)
}

TEST_CASE("csf image2DArray compute write is non-black on every layer", "[gfx][l3][csf][array]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r
      = render(backend, {corpus("csf-array-image-write.cs"), corpus("csf-array-image-read.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = score::test::gfx::compute_shader_skip_reason(backend))
    SKIP(why);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const int qx0 = img.width / 4, qx1 = 3 * img.width / 4;
  const int qy0 = img.height / 4, qy1 = 3 * img.height / 4;
  const int l0 = brightness(img, qx0, qy0); // layer 0 (written even pre-fix)
  const int l1 = brightness(img, qx1, qy0); // layer 1
  const int l2 = brightness(img, qx0, qy1); // layer 2
  const int l3 = brightness(img, qx1, qy1); // layer 3 (THE guard)
  INFO("layers 0=" << l0 << " 1=" << l1 << " 2=" << l2 << " 3=" << l3);

  CHECK(l0 > 120);
  // Layers past 0 must be written too.
  CHECK(l3 > 120);
  CHECK(l1 > 120);
  CHECK(l2 > 120);
}
