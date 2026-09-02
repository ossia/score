// =============================================================================
// P1-21 -- A 3D STORAGE IMAGE RESIZED AT RUNTIME KEEPS WRITING CORRECT VOXELS.
//
// Intended registration: score_add_csf_test(image3d_resize CsfImage3dResize.cpp)
//
// ISOLATION: this target needs the SAME treatment as test_gfx_csf_image3d --
// ISOLATED in its own target, EXPECTED RED on OpenGL: the 3D storage-image
// path is CORRECT on Vulkan but reads back all-black on OpenGL (a real
// GL-specific engine bug, documented at CsfImage3d.cpp:1-14 and its CMake
// registration comment, tests/gfx/CMakeLists.txt:109-114). Exactly like
// CsfImage3d.cpp, this file does NOT special-case GL in code: it asserts the
// correct pixels on every backend and lets the isolated registration carry
// the attributable GL RED, so the finding stays honest and visible.
//
// ADDITIONALLY EXPECTED RED TODAY ON EVERY BACKEND for the resize phases
// (32^3 and 96^3): the runtime resize this test asserts is NOT YET
// IMPLEMENTED. RenderedCSFNode.cpp:4250-4251 says it in so many words:
//     // Update output texture size if it has changed
//     // TODO: Check if texture size inputs have changed and recreate texture
// and the storage-image texture is allocated exactly once, lazily, in
// buildComputeSrbBindings (RenderedCSFNode.cpp:3170-3172,
// `if(!it->texture) it->texture = make_tex("")`) -- nothing ever compares the
// live size expression against the existing allocation. Per the house rule
// ("assert correct behaviour, not current behaviour") the phases assert the
// spec'd contract; the 64^3 phase, which matches the initial allocation, is
// GREEN on Vulkan, giving per-phase attribution. When the TODO is implemented
// the whole case flips green on Vulkan.
//
// WHAT IS DRIVEN (all verified in source, this worktree):
//  * corpus/syn-3d-image-resize.cs declares a long INPUT `edge` (DEFAULT 64)
//    and a write_only rgba8 volume with WIDTH/HEIGHT/DEPTH: "$edge".
//    csf-3d-image-write.cs (the P1-21 spec shader) hard-codes 64x64x64 with
//    no control, so this synthesized twin follows its conventions (3D_IMAGE
//    dispatch, imageSize guard, LOCAL_SIZE [4,4,4]) with the size made
//    drivable -- the spec's "$USER/long-driven size" escape hatch.
//  * WIDTH/HEIGHT strings are kept as expressions (libisf isf.cpp:1381-1406,
//    DEPTH likewise) and evaluated by computeTextureSize
//    (RenderedCSFNode.cpp:253, `$` -> `var_` at :268/:278) /
//    resolveDispatchExpression for DEPTH (:3127-3129), against `var_edge`
//    registered from the long control port's CURRENT value
//    (registerCommonExpressionVariables, RenderedCSFNode.cpp:494-509,
//    `*(int*)port->value`).
//  * The 3D texture itself is created at RenderedCSFNode.cpp:3132-3133
//    (`rhi.newTexture(format, w, h, depth, 1, ThreeDimensional |
//    UsedWithLoadStore)`), size resolved at :3105-3107 via getImageSize
//    (:701) -> computeTextureSize.
//  * The 3D_IMAGE dispatch is sized from the LIVE texture every frame
//    (RenderedCSFNode.cpp:4511-4521: pixelSize() + tex->depth()), so the
//    writer always covers exactly the actual allocation -- which is what
//    makes the R == edge fingerprint below trustworthy: R reports the real
//    allocated edge, not the requested one.
//  * setControl drives ProcessNode::process(port, value), the same public
//    entry the exec engine uses (score_test/Gfx.hpp), between render()
//    calls of ONE GfxPipeline session -- one create(), no graph rebuild,
//    same shape as GfxInstanceCountLive.cpp's grow/shrink phases.
//
// CLOSED FORMS (writer, for voxel p in [0,E)^3 at live allocated edge E):
//    R = E/255            -> readback byte R == E exactly (UNORM8-exact)
//    G = (p.y + 0.5)/E    -> sampled G(x,y) ~= 255*(y+0.5)/64 on the 64px
//                            sink, INDEPENDENT of E (linear reconstruction)
//    B = (p.z + 0.5)/E    -> at sliceZ = 0.5, B ~= 128 (viewer default)
//    A = 1                -> 255
// The oracle per phase (E in 64, 32, 96): R == E across the whole slice.
// "Stale content of the previous allocation" is exactly R == previous E:
// after 64^3, a kept texture reads R = 64 in the 32^3 phase (delta 32) and
// R = 64 in the 96^3 phase (delta 32) -- far outside the +-6 tolerance.
//
// NEGATIVE CONTROL (product side, for the orchestrator): the spec's control
// -- "keep the old QRhiTexture on a size change -> the 96^3 case samples the
// 64^3 content" -- is TODAY'S SHIPPED CODE: the allocate-once guard at
// src/plugins/score-plugin-gfx/Gfx/Graph/RenderedCSFNode.cpp:3170
// (`if(!it->texture)`) plus the unimplemented TODO at
// RenderedCSFNode.cpp:4251. Once the resize fix lands (re-running
// getImageSize/computeTextureSize against the live allocation and
// re-creating at :3132-3133 on a mismatch), reinstating the bare
// `if(!it->texture)` guard at :3170 is the one-line revert that makes this
// test's 96^3 phase read R = 64 again.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_image3d_resize
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_image3d_resize   (RED, GL finding)
// QT_QPA_PLATFORM=offscreen must NOT be used: the verdict is pixels, so
// unavailable backends SKIP instead (fixture probe).
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
using score::test::gfx::GfxPipeline;
using score::test::gfx::near;
using score::test::gfx::nth_control_input;
using score::test::gfx::ReadbackImage;
using score::test::gfx::setControl;

QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

constexpr int kSink = 64;   // sink is 64x64 regardless of the volume edge
constexpr int kFrames = 3;  // >= 2 so each setControl is picked up + rendered
constexpr int kTol = 6;     // per-channel LSB slack across backends
constexpr int kEdgeA = 64;  // matches the initial allocation (DEFAULT 64)
constexpr int kEdgeB = 32;  // shrink
constexpr int kEdgeC = 96;  // grow -- the spec's stale-allocation catch

struct ResizeResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  ReadbackImage at64, at32, at96;
};

/// One session: writer (.cs) -> slice viewer (.fs) -> sink, ONE create(),
/// then edge=64 / render / read, edge=32 / render / read, edge=96 / render /
/// read. Collect only; Catch2 macros run after run_in_gui_app returns
/// (fixture header contract).
ResizeResult run_resize(score::gfx::GraphicsApi be)
{
  ResizeResult r;
  r.backend = score::test::gfx::backend_name(be);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int writer = p.addCsf(corpus("syn-3d-image-resize.cs"));
    const int viewer = p.addIsf(corpus("3d-slice-viewer.fs"));
    if(writer < 0 || viewer < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({kSink, kSink});
    // Same wiring render_isf_chain gives {cs, fs}: the CSF's write_only
    // volume RESOURCE is its image output; the viewer's DIMENSIONS:3
    // `volume` INPUT is its image input 0 (sliceZ stays at DEFAULT 0.5).
    p.wire(p.imageOut(writer, 0), p.imageIn(viewer, 0));
    p.wire(p.imageOut(viewer, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    if(!p.error().empty())
    {
      r.error = p.error();
      return;
    }

    // The writer's only control inlet is `edge` (the write_only volume
    // creates an outlet, not an inlet) -- but locate it by control ordinal
    // rather than hard-coding raw port 0.
    const int edgePort = nth_control_input(*p.isf(writer), 0);
    if(edgePort < 0)
    {
      r.error = "syn-3d-image-resize.cs exposed no control inlet for `edge`";
      return;
    }

    // Phase A: 64^3 -- equals the initial allocation (and the DEFAULT), so
    // this phase is green wherever CsfImage3d's fixed-64^3 case is green.
    setControl(*p.isf(writer), edgePort, ossia::value{kEdgeA});
    p.render(kFrames);
    r.at64 = p.readback(sink);

    // Phase B: SHRINK to 32^3 -- same session, no rebuild, just the control.
    setControl(*p.isf(writer), edgePort, ossia::value{kEdgeB});
    p.render(kFrames);
    r.at32 = p.readback(sink);

    // Phase C: GROW to 96^3 -- the spec's negative-control catch: a kept
    // 64^3 QRhiTexture reads back R = 64 here, not 96.
    setControl(*p.isf(writer), edgePort, ossia::value{kEdgeC});
    p.render(kFrames);
    r.at96 = p.readback(sink);
  });
  return r;
}

/// True if the image is not one flat colour (same helper as CsfImage3d.cpp).
bool non_degenerate(const ReadbackImage& img)
{
  const auto first = img.at(2, 2);
  for(int y = 2; y < img.height - 2; y += 2)
    for(int x = 2; x < img.width - 2; x += 2)
      if(!near(img.at(x, y), first, 6))
        return true;
  return false;
}

/// Assert one phase's slice against the closed forms for edge E:
///  1. R == E on a coarse interior grid (the size fingerprint; stale
///     previous-allocation content carries the previous E, delta >= 32).
///  2. B(center) ~= 128 (sliceZ = 0.5 cross-section, CsfImage3d precedent).
///  3. |G(32,56) - G(32,8)| ~= 191 = round(255*48/64): the y ramp survives
///     the resize; abs() keeps it orientation-agnostic across backends.
///  4. A == 255, and the slice is non-degenerate.
void check_phase(const ReadbackImage& img, int edge, const char* phase)
{
  INFO("phase " << phase << " (edge=" << edge << ")");
  REQUIRE(img.valid());
  REQUIRE(img.width == kSink);
  REQUIRE(img.height == kSink);

  // 1. Size fingerprint: R == edge everywhere (constant channel, immune to
  // filtering). Track the worst deviation so one CHECK reports the field.
  int worstR = 0, worstX = -1, worstY = -1;
  for(int y = 2; y < img.height - 2; y += 3)
    for(int x = 2; x < img.width - 2; x += 3)
    {
      const int d = std::abs(int(img.at(x, y)[0]) - edge);
      if(d > worstR)
      {
        worstR = d;
        worstX = x;
        worstY = y;
      }
    }
  INFO(
      "worst |R - " << edge << "| = " << worstR << " at (" << worstX << ","
                    << worstY << ") -- R must equal the LIVE allocation edge; "
                    << "a stale previous allocation reads the OLD edge here");
  CHECK(worstR <= kTol);

  const auto c = img.center();
  INFO(
      "centre=(" << int(c[0]) << "," << int(c[1]) << "," << int(c[2]) << ","
                 << int(c[3]) << ")");

  // 2. B ~ 128: the viewer samples z = 0.5 of the CURRENT volume.
  CHECK(std::abs(int(c[2]) - 128) <= kTol + 2);

  // 3. The y ramp: G ~= 255*(y+0.5)/64, so |G@y=56 - G@y=8| ~= 191,
  // whichever way the backend orients the readback.
  const int g0 = img.at(kSink / 2, 8)[1];
  const int g1 = img.at(kSink / 2, 56)[1];
  INFO("G ramp: G(32,8)=" << g0 << " G(32,56)=" << g1 << " |diff| expected ~191");
  CHECK(std::abs(std::abs(g1 - g0) - 191) <= 2 * kTol);

  // 4. Alpha solid; slice not a flat field (broken 3D path reads all-black).
  CHECK(int(c[3]) == 255);
  CHECK(non_degenerate(img));
}
}

TEST_CASE(
    "a 3D storage image resized at runtime keeps writing correct voxels",
    "[gfx][l3][csf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const ResizeResult r = run_resize(backend);

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());

  // 64^3 -> 32^3 -> 96^3, one session, one allocation contract per phase.
  check_phase(r.at64, kEdgeA, "A 64^3 (initial allocation)");
  check_phase(r.at32, kEdgeB, "B 32^3 (shrink)");
  check_phase(r.at96, kEdgeC, "C 96^3 (grow past the original allocation)");
}
