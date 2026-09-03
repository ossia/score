// =============================================================================
// L3 GPU render + readback — a cubemap render target gets six distinct faces
// (SPEC-SCENE-RENDER-TESTS.md P1-7, gap G8).
//
// The self-declared oracle from the user diagnostic score
// 2026/test-cubemap-output.score: a RAW_RASTER_PIPELINE with MULTIVIEW:6 +
// CUBEMAP:true (syn-cube-six-colors.{vs,fs}, lifted verbatim from the score;
// only an explicit 64x64 OUTPUT size was added) writes one solid colour per
// face; a downstream samplerCube viewer (syn-cube-six-probe.fs) samples all six
// axis directions into a 3x2 grid. Expected, exactly:
//
//     +X red    -X cyan    +Y green    -Y magenta    +Z blue    -Z yellow
//
// All six must be found, each exactly once.
//
// What this drives (verified against the current tree):
//  * The transparent CUBEMAP+MULTIVIEW shim, RenderedRawRasterPipelineNode.hpp
//    :298-312 (m_cubeCopyShadowArray / m_cubeCopyCube / m_cubeCopyOutputIdx):
//    QRhi forbids setMultiViewCount on a cube texture, so the node renders into
//    a 6-layer TextureArray and copies each layer into the matching cube face
//    at end of frame (the copy loop in runInitialPasses,
//    RenderedRawRasterPipelineNode.cpp:3215-3243).
//  * textureForOutput's cube branch, RenderedRawRasterPipelineNode.cpp:179-184:
//    the PUBLIC handle downstream consumers bind as samplerCube must be the
//    CubeMap, not the shadow array.
//  * The samplerCube image edge: raster cube output -> ISF "cubemap" INPUT,
//    the same wiring csf-cube-image-write.cs -> csf-cube-image-read.fs proves
//    for compute producers (CsfCubeArray.cpp).
//
// Backend scope (GfxMultiview.cpp precedent, followed honestly):
//  * Vulkan / D3D12 / Metal (caps.multiview true): full pixel oracle.
//  * OpenGL: the offscreen GL context on a headless box does not render a
//    procedural MULTIVIEW layered raster at all (GfxMultiview.cpp documents
//    layer 0 reading back black even on the fixed engine), so the pixel guard
//    is not expressible; the crash-free-build guard plus the structural
//    cube-handle guard below still run, then SUCCEED() with the reason.
//  * Null / no-multiview-caps fallback: the structural half of P1-7 — the
//    producer's public output handle must be a QRhiTexture::CubeMap, and when
//    multiview caps are present it must be the shim's cube
//    (RRPNode::MRT::cubeCopyCube::*), i.e. the array-then-copy path was
//    SELECTED — asserted via the public NodeRenderer::textureForOutput, no
//    pixels needed.
//
// Negative control (product-side, proposed — do not commit): in the cube-copy
// finaliser loop, RenderedRawRasterPipelineNode.cpp:3232, change
//     desc.setSourceLayer(face);
// to
//     desc.setSourceLayer(0);
// so layer 0 is blitted into every face. Five of the six probes then read +X
// red instead of their own colour and this test goes red on every
// multiview-capable backend (the +X probe alone stays green).
//
// Intended registration (tests/gfx/CMakeLists.txt):
//     score_add_gfx_test(cubemap_six_faces GfxCubemapSixFaces.cpp)
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_cubemap_six_faces
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_cubemap_six_faces
// =============================================================================
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

struct CubeFacts
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // Structural facts about the producer's public colour output handle,
  // harvested through the public NodeRenderer::textureForOutput while the
  // pipeline is alive. This is the pixel-free (Null-fallback) half of P1-7.
  bool multiview_caps = false;    // RenderList::state.caps.multiview
  bool renderer_found = false;    // the raster node had a live NodeRenderer
  bool out_tex_found = false;     // textureForOutput returned non-null
  bool out_tex_is_cube = false;   // handle carries QRhiTexture::CubeMap
  bool out_tex_is_shim_cube = false; // handle is RRPNode::MRT::cubeCopyCube::*

  ReadbackImage view; // the samplerCube viewer's 2D output (3x2 probe grid)
};

CubeFacts run_cube(score::gfx::GraphicsApi backend)
{
  CubeFacts f;
  f.backend = backend_name(backend);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    // Producer: procedural fullscreen triangle (PIPELINE_STATE.VERTEX_COUNT 3,
    // no VERTEX_INPUTS), exactly as the score runs it — no geometry node.
    const int prod = p.addRaster(
        corpus("syn-cube-six-colors.vs"), corpus("syn-cube-six-colors.fs"));
    // Viewer: samplerCube probe of all six axis directions into a 3x2 grid.
    const int view = p.addIsf(corpus("syn-cube-six-probe.fs"));
    if(prod < 0 || view < 0)
    {
      f.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }

    // This case has a STRUCTURAL half that is meaningful on the Null backend
    // (which shim the producer selected: cube vs shadow-array vs plain 2D), and
    // it is written to hold "on EVERY backend that could build the pipeline,
    // Null included" -- see the invariant below. So it opts back into Null,
    // which the fixture otherwise refuses for pixel work (spec P2-15,
    // null_backend_skip_reason()). The pixel half already excludes Null
    // explicitly at the `isNull` guard further down.
    p.allowNullBackend();

    // 96x64 sink => six 32x32 probe cells.
    const int sink = p.addSink({96, 64});
    p.wire(p.imageOut(prod, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));

    if(!p.create(backend))
    {
      f.skipped = p.skipped();
      f.skip_reason = p.skipReason();
      f.error = p.error();
      return;
    }
    if(!p.error().empty())
    {
      f.error = p.error();
      return;
    }

    p.render(4);
    f.view = p.readback(sink);

    // Structural half: what does the producer publish as its colour output?
    // With multiview caps this must be the shim's CubeMap (m_cubeCopyCube via
    // the textureForOutput branch at RenderedRawRasterPipelineNode.cpp:183);
    // without them, the direct cube-render fallback still publishes a CubeMap.
    auto* outPort = p.imageOut(prod, 0);
    auto& node = *p.isf(prod);
    for(auto& [renderList, renderer] : node.renderedNodes)
    {
      if(!renderList || !renderer)
        continue;
      f.renderer_found = true;
      f.multiview_caps = renderList->state.caps.multiview;
      if(QRhiTexture* tex = renderer->textureForOutput(*outPort))
      {
        f.out_tex_found = true;
        f.out_tex_is_cube = tex->flags().testFlag(QRhiTexture::CubeMap);
        f.out_tex_is_shim_cube = tex->name().contains("cubeCopyCube");
      }
    }
  });
  return f;
}
}

TEST_CASE(
    "a cubemap render target gets six distinct faces",
    "[gfx][l3][raster][cubemap][multiview]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const CubeFacts f = run_cube(be);
  if(f.skipped)
  {
    SKIP(f.backend + ": " + f.skip_reason);
  }

  INFO("backend=" << f.backend << " error='" << f.error << "'");
  REQUIRE(f.error.empty());
  REQUIRE(f.view.valid());

  // ---- Structural (pixel-free) half: the shim / cube handle was selected.
  // Holds on EVERY backend that could build the pipeline, Null included.
  REQUIRE(f.renderer_found);
  REQUIRE(f.out_tex_found);
  // Downstream samplerCube consumers bind textureForOutput's result directly;
  // publishing anything but a cube texture would break every consumer.
  CHECK(f.out_tex_is_cube);
  if(f.multiview_caps)
  {
    // Multiview available => the CUBEMAP+MULTIVIEW array-then-copy shim must
    // be the selected path, and the public handle its cube — not the shadow
    // TextureArray actually rendered into.
    CHECK(f.out_tex_is_shim_cube);
  }

  // ---- Pixel half: the six-face oracle.
  // HONEST BACKEND SCOPE (GfxMultiview.cpp precedent): the offscreen OpenGL
  // context on this box does not render a procedural MULTIVIEW layered raster
  // headless — layers read back black even on the fixed engine — so the pixel
  // oracle is not expressible on GL; nor on Null (no rasterization), nor
  // where multiview caps are absent (the one-draw-six-views amplification
  // cannot happen). The crash-free build + structural guards above still ran.
  const bool isGL = f.backend == "OpenGL";
  const bool isNull = f.backend == "Null";
  if(isGL || isNull || !f.multiview_caps)
  {
    SUCCEED(
        f.backend
        + ": procedural MULTIVIEW layered raster not renderable headless here "
          "(GfxMultiview.cpp precedent); crash-free build and cube-handle "
          "selection asserted, six-face pixel oracle runs on Vulkan/D3D12/Metal");
    return;
  }

  const auto& img = f.view;
  const int cw = img.width / 3;
  const int ch = img.height / 2;
  REQUIRE(cw > 0);
  REQUIRE(ch > 0);

  // The score's own oracle, QRhi/GL face order 0..5:
  //   +X red, -X cyan, +Y green, -Y magenta, +Z blue, -Z yellow.
  const std::array<std::array<uint8_t, 4>, 6> expected{{
      {255, 0, 0, 255},     // +X red
      {0, 255, 255, 255},   // -X cyan
      {0, 255, 0, 255},     // +Y green
      {255, 0, 255, 255},   // -Y magenta
      {0, 0, 255, 255},     // +Z blue
      {255, 255, 0, 255},   // -Z yellow
  }};
  static const char* const face_names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};

  // One probe per grid cell centre (viewer cell face = row*3+col). The
  // shader keys its rows on isf_FragNormCoord.y < 0.5, and under the house
  // ISF orientation contract (GfxOrientation / GfxMrtPattern: uv.y == 1 is
  // the TOP row of the Y-corrected delivered image) that puts shader row 0
  // (faces 0..2) in the BOTTOM half of the readback -- measured on Vulkan:
  // reading rows top-first swapped faces 0<->3, 1<->4, 2<->5 while all six
  // colours were present exactly once. Hence height - 1 - ... here.
  std::array<std::array<uint8_t, 4>, 6> got{};
  for(int face = 0; face < 6; ++face)
  {
    const int col = face % 3;
    const int row = face / 3; // shader row: 0 = uv.y < 0.5 = bottom half
    got[face] = img.at(col * cw + cw / 2, img.height - 1 - (row * ch + ch / 2));
  }

  // Each face shows exactly its own colour (solid fills: tol 2).
  for(int face = 0; face < 6; ++face)
  {
    INFO(
        "face " << face_names[face] << " (" << face << "): got (" << int(got[face][0])
                << "," << int(got[face][1]) << "," << int(got[face][2]) << ","
                << int(got[face][3]) << ") expected (" << int(expected[face][0]) << ","
                << int(expected[face][1]) << "," << int(expected[face][2]) << ","
                << int(expected[face][3]) << ")");
    CHECK(near(got[face], expected[face], 2));
  }

  // ... and every one of the six oracle colours is found exactly once across
  // the six probes — fewer means a face was dropped or duplicated (the shim
  // blitting one layer everywhere), more means colours aliased.
  for(int c = 0; c < 6; ++c)
  {
    int found = 0;
    for(int face = 0; face < 6; ++face)
      if(near(got[face], expected[c], 2))
        ++found;
    INFO(
        "colour " << face_names[c] << " found " << found
                  << " time(s) across the six probes");
    CHECK(found == 1);
  }
}
