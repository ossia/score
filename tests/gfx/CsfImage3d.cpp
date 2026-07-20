// =============================================================================
// L3 GPU render + readback — CSF 3D_IMAGE dispatch, across every RHI backend.
//
// csf-3d-image-write.cs uses EXECUTION_MODEL 3D_IMAGE to write a 64x64x64 RGBA8
// volume whose channels vary along all three axes, then an ISF sampler3D slice
// viewer (3d-slice-viewer.fs) reads a Z slice back into a 2D texture we can read.
//
// This exercises the 3D compute-image path AND the 3D-image -> ISF sampler3D
// cross-node binding, which is a distinct integration. It is ISOLATED in its
// own target so a per-backend failure cannot take down the 2D CSF image group.
//
// REGRESSION GUARD. At the default slice sliceZ=0.5 the readback is a proper
// cross-section: blue ~ 128 (uv.z = 0.5), R varies with x, G with y. GREEN on
// both Vulkan and OpenGL.
//
// History: this was GL-black (centre (0,0,0,255)) until the engine fix in
// commit "gfx: fix all-black 3D storage-image compute output on OpenGL". Qt's
// QRhi GL backend bound the 3D storage image non-layered, so the 3D_IMAGE
// compute imageStore wrote only slice 0 and the sampler3D read an empty volume
// at z=0.5; Vulkan was always correct. The assertions below encode the correct
// (Vulkan) result; both backends now pass. Do NOT weaken.
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
using score::test::gfx::near;
using score::test::gfx::ReadbackImage;

IsfResult render(score::gfx::GraphicsApi backend, std::vector<QString> chain)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_isf_chain(backend, std::move(chain));
  });
  return r;
}

bool non_degenerate(const ReadbackImage& img)
{
  const auto first = img.at(2, 2);
  for(int y = 2; y < img.height - 2; y += 2)
    for(int x = 2; x < img.width - 2; x += 2)
      if(!near(img.at(x, y), first, 6))
        return true;
  return false;
}
}

TEST_CASE("csf-3d-image-write feeds a 3D volume to an ISF slice viewer", "[gfx][l3][csf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r
      = render(backend, {corpus("csf-3d-image-write.cs"), corpus("3d-slice-viewer.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto c = img.center();
  INFO("centre=(" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ","
                  << (int)c[3] << ")");

  // At the default slice sliceZ = 0.5 the volume's B channel encodes uv.z = 0.5,
  // so blue should read back ~128 across the slice; R varies with x, G with y,
  // giving a non-constant image. A broken 3D path reads back all-black or a
  // single flat colour.
  CHECK(non_degenerate(img));
  CHECK(std::abs(int(c[2]) - 128) <= 40); // slice z=0.5 -> blue ~ mid
}
