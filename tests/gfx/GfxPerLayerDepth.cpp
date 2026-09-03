// =============================================================================
// L3 GPU render + readback — per-layer depth renders through the copy shim
// (SPEC-SCENE-RENDER-TESTS.md §3.3 row P2-2).
//
// Intended registration (tests/gfx/CMakeLists.txt), NOT applied by this change:
//     score_add_gfx_test(per_layer_depth GfxPerLayerDepth.cpp)
// -> ctest name test_gfx_per_layer_depth.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_per_layer_depth
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_per_layer_depth
//
// -----------------------------------------------------------------------------
// WHAT IS UNDER TEST
// -----------------------------------------------------------------------------
// A RAW_RASTER_PIPELINE whose EXECUTION_MODEL is PER_LAYER and whose TARGET is a
// multi-layer DEPTH output binds one array layer per pass, through
// QRhiTextureRenderTargetDescription::setDepthLayer (Qt >= 6.12). Each
// invocation renders straight into layer i of the OUTPUT array; nothing is
// copied.
//
// THIS CASE WAS PINNED EXPECTED-RED, and the pin was correct: the engine used
// to render every invocation into one shared scratch 2D D32F and copyTexture()
// it into layer i after each endPass, which is a NO-OP ON EVERY BACKEND --
// QRhi::copyTexture is colour-only (qrhivulkan.cpp:4782/:4792 set
// VK_IMAGE_ASPECT_COLOR_BIT unconditionally; the GL path attaches the source to
// GL_COLOR_ATTACHMENT0). The depth array came back cleared and the cascade
// rendered nothing, which is exactly what this case measured. The shim is gone
// rather than fixed: it never worked.
//
// Below Qt 6.12 setDepthLayer does not exist, the engine refuses the mode with
// a diagnostic and falls back to SINGLE, and this case SKIPs.
//
// State declaration (read, verified):
//   Gfx/Graph/RenderedRawRasterPipelineNode.hpp:260-279
//     "PerLayer state. m_perLayerOutputIndex is the raw index into
//      descriptor().outputs[], depth-inclusive ... depth: Qt RHI 6.11 exposes no
//      per-layer depth attachment, so m_perLayerScratchDepth is a single 2D D32F
//      shared across iterations (m_perLayerSharedRT/RP) and runInitialPasses
//      copies it into layer i after each endPass. m_perLayerOutputDepthArray
//      aliases the OUTPUT array as the copy destination."
//     (:260 is the comment's first line, :279 is m_perLayerOutputDepthArray —
//      the row's cited range is the DECLARATION, not the loop.)
//
// The copy loop itself (read, verified) is in runInitialPasses:
//   Gfx/Graph/RenderedRawRasterPipelineNode.cpp:3199-3218
//     :3203  if(m_executionMode == ExecutionMode::PerLayer && m_perLayerIsDepth
//     :3208    cdesc.setPixelSize(viewportSize);
//     :3209    cdesc.setSourceLayer(0);
//     :3212    cdesc.setDestinationLayer(i);          <-- the layer identity
//     :3215-16 copyBatch->copyTexture(m_perLayerOutputDepthArray,
//                                     m_perLayerScratchDepth, cdesc);
//
// Selection of that path (read, verified):
//   :639-640  EXECUTION_MODEL "PER_LAYER" -> ExecutionMode::PerLayer
//   :659-669  PER_LAYER walks the RAW outputs[] (depth entries included) and
//             sets m_perLayerIsDepth = (outputs[i].type == "depth")
//   :1183-1230 the depth branch: scratch D32F + shared RT + m_mipCount = LAYERS
//   :3108/:3123 the invocation loop stamps ProcessUBO.passIndex = i (== PASSINDEX)
//   :170-171  textureForOutput returns m_mrtRenderTarget.depthTexture for a
//             depth OUTPUT — i.e. the public downstream handle IS the array the
//             copy loop writes into.
//
// -----------------------------------------------------------------------------
// WHY A NEW SHADER PAIR (rr-perlayer-depth.{vs,fs})
// -----------------------------------------------------------------------------
// Neither corpus shader the spec row names can drive this:
//
//  * corpus/shadow_cascades.{vert,frag} IS the real PER_LAYER depth-array
//    shader, but it consumes indexed MDI geometry, a `per_draws` storage buffer,
//    an `indirect_draw_cmds` indirect buffer and a scene-published
//    `shadow_cascades` UBO (shadow_cascades.frag:28-50). tests/gfx/'s fixture has
//    no scene infrastructure; GfxExecutionModel.cpp:4-12 says exactly this and
//    is why that file stops at parse-level coverage for it. Its target is also
//    8 x 2048^2.
//  * corpus/rr-perlayer.{vs,fs} IS procedural PER_LAYER with no geometry, but
//    its OUTPUT is `"TYPE": "color"` (rr-perlayer.fs:14). That makes
//    m_perLayerIsDepth false (:666) and takes the m_mipRTs setLayer(i) colour
//    branch (:3157-3163) — the copy shim under test never runs.
//  * The spec row's `rr-perlayer.{vs,fs}` therefore does NOT cover P2-2 as
//    written; see "SPEC NOTES" below.
//
// rr-perlayer-depth.{vs,fs} is rr-perlayer's procedural triangle with a
// per-layer clip z and a depth OUTPUT: PER_LAYER + depth + LAYERS 4, no scene.
//
// The downstream probe is the EXISTING corpus/csf-array-image-read.fs — a
// sampler2DArray that reads layer 0/1/2/3 into the four quadrants
// (csf-array-image-read.fs:14-25). Its `layers` input is
// {"TYPE":"image","IS_ARRAY":true}, which ISFNode.cpp:139-145 turns into a
// Flag::GrabsFromSource | Flag::TextureArray port, and Graph.cpp:867-882 binds
// such a port straight to the upstream's textureForOutput() — the depth array
// itself. Sampling a D32F with a non-comparison sampler puts the depth in .r,
// so only the RED channel is asserted here.
//
// -----------------------------------------------------------------------------
// CLOSED FORM — derived, not observed
// -----------------------------------------------------------------------------
// rr-perlayer-depth.vs computes, for invocation i (== PASSINDEX == array layer):
//
//     z_gl(i) = 0.4 * i - 0.6              w = 1
//             = -0.6, -0.2, +0.2, +0.6     for i = 0,1,2,3
//
// and emits gl_Position = clipSpaceCorrMatrix * vec4(xy, z_gl, 1.0).
//
// Depth-range convention, from the tree rather than from a measurement:
//   * On OpenGL, QRhi::clipSpaceCorrMatrix() is the identity, so NDC z = z_gl in
//     [-1,+1], and the default glDepthRange maps that to window depth
//     0.5*z_gl + 0.5.
//   * On Vulkan / Metal / D3D the matrix "remaps the output NDC z in [-1,+1]
//     down to the backend-native [0,1] without further flipping"
//     (Gfx/Graph/CameraMath.hpp:54-56, read); with w == 1 that is again
//     0.5*z_gl + 0.5, and the native depth range is already [0,1].
// Both routes give the SAME window depth, which is the point of routing through
// clipSpaceCorrMatrix (the same call shadow_cascades.vert:26 makes):
//
//     d(i) = 0.5 * z_gl(i) + 0.5 = 0.2 * i + 0.2
//     d = 0.20, 0.40, 0.60, 0.80    for layers 0,1,2,3
//
// The probe writes d into an RGBA8 sink (Gfx.hpp:91-92), so the expected RED
// byte is round(d * 255):
//
//     layer 0 -> 51     layer 1 -> 102     layer 2 -> 153     layer 3 -> 204
//
// Nothing else can move these values: DEPTH_COMPARE is "always" with a
// depth-clear of 0.0 (PipelineStateHelpers.cpp:49-62), CULL_MODE is none, one
// primitive covers the whole viewport, and every texel of a layer is identical
// so sampler filtering cannot interpolate across a boundary.
//
// TOLERANCE: 3 LSB on the red byte.
//   * D32F holds 0.2/0.4/0.6/0.8 to ~1e-7 and 0.4*i-0.6 evaluates to within
//     ~1e-8 of the exact value, i.e. < 1e-5 LSB. Format precision is not the
//     limiting term.
//   * The limiting term is the RGBA8 readback: 1 LSB = 1/255 = 0.0039 of depth
//     range, plus the couple-of-LSB inter-backend slack the fixture documents
//     (Gfx.hpp:33-35). 3 covers rounding + that slack.
//   * The four expectations are 51 LSB apart, so a tolerance of 3 can never let
//     one layer's depth be mistaken for another's — which is what makes the
//     layer-identity oracle below meaningful.
//
// -----------------------------------------------------------------------------
// THE ORACLE IS LAYER IDENTITY, NOT "THE LAYERS DIFFER"
// -----------------------------------------------------------------------------
// Two named groups of pixel assertions:
//   [per-layer depth] layer i's probe carries d(i) and no other d.
//   [depth multiset]  each of {51,102,153,204} is found exactly once across the
//                     four probes. Orientation-free: it holds whichever way the
//                     readback's rows are ordered, so it is the assertion that
//                     survives if the Y convention below were ever wrong.
//
// Readback orientation: the probe keys its rows on isf_FragNormCoord.y < 0.5,
// and under the house ISF orientation contract uv.y < 0.5 is the BOTTOM half of
// the delivered image — measured in-tree on Vulkan and written up at
// GfxCubemapSixFaces.cpp:232-244 (reading rows top-first swapped the two rows
// while all colours were still present exactly once). Hence height-1-y here.
// (CsfCubeArray.cpp:78-83 comments the opposite mapping for this same probe
// shader, but it only asserts "all four bright", so its labels are never
// exercised — unverified, and not relied on.)
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROL (product-side, proposed — do NOT commit)
// -----------------------------------------------------------------------------
// PRIMARY, the copy shim itself:
//   Gfx/Graph/RenderedRawRasterPipelineNode.cpp:3212, in the PER_LAYER depth
//   copy loop, change
//       cdesc.setDestinationLayer(i);
//   to
//       cdesc.setDestinationLayer(0);
//   i.e. copy every layer out of the scratch without advancing i. Every
//   invocation then lands in layer 0, which ends up holding the LAST
//   invocation's depth while layers 1..3 are never written.
//     REDDENS: [per-layer depth] all four (layer 0 reads 204 not 51; layers
//              1..3 read the untouched array, not their d), and
//              [depth multiset] (204 found twice or more, 51/102/153 missing).
//     STAYS GREEN: [PER_LAYER decision logic] and [depth array published] —
//              the descriptor still says PER_LAYER/depth/LAYERS 4 and
//              textureForOutput still hands out a 4-layer D32F array. That is
//              the split the Null/no-pixel lane relies on.
//
// SECONDARY, matching the spec row's literal wording "all layers identical":
//   Gfx/Graph/RenderedRawRasterPipelineNode.cpp:3123, change
//       this->n.standardUBO.passIndex = i;
//   to
//       this->n.standardUBO.passIndex = 0;
//   Every invocation then renders layer 0's depth and the copy still advances,
//   so all four layers come out identical at 51.
//     REDDENS: [per-layer depth] for layers 1,2,3 (layer 0 stays green — this
//              is the "all but one" shape the row asks for), and
//              [depth multiset] (51 found four times).
//     STAYS GREEN: layer 0's probe, plus both structural groups.
//
// BOTH controls are currently UNOBSERVABLE through this case: with the
// copyTexture defect below in place every probe already reads zero, so neither
// edit changes the result. They become meaningful the moment the depth copy is
// fixed, and are kept for that. The control that IS live today is the
// PER_LAYER-colour case at the bottom of this file.
//
// -----------------------------------------------------------------------------
// BACKEND SCOPE
// -----------------------------------------------------------------------------
//  * Backend cannot initialise / offscreen target cannot be allocated -> SKIP
//    (GfxPipeline::create reports it; never a failure).
//  * Null -> no rasterisation, so the pixel half is not expressible: the
//    structural half is asserted and the case SUCCEEDs with the reason, the
//    shape P2-15 asks for and the shape GfxCubemapSixFaces.cpp:202-212 uses.
//  * Every other backend, OpenGL INCLUDED, runs the pixel oracle. OpenGL is
//    deliberately NOT pre-excluded the way GfxCubemapSixFaces/GfxMultiview
//    exclude it: their exclusion rests on a documented, measured GL failure for
//    procedural MULTIVIEW layered rasters, and this path is not multiview. That
//    choice was the right one: OpenGL and Vulkan fail here IDENTICALLY and for
//    the same reason (below), which is what proves it is a product defect and
//    not a GL-headless artefact.
//
// -----------------------------------------------------------------------------
// MEASURED: THIS CASE IS RED, AND WHY. QRhi's copyTexture IS COLOUR-ONLY.
// -----------------------------------------------------------------------------
// Measured 2026-09-02 on this box, OpenGL and Vulkan, real display, from
// tests/gfx/GfxPerLayerDepth.cpp as committed:
//
//     red at quadrant centres:       L0=0 L1=0 L2=0 L3=0
//     red at quadrant (0,0) corners: L0=0 L1=0 L2=0 L3=0
//     published depth output: name='RenderedRawRasterPipelineNode::MRT::depth::
//     cascade' array=1 layers=4 d32f=1 64x64
//
// Every layer of the depth array reads back as exactly zero — the depth clear
// value — on BOTH backends. Both structural groups are green: the array is
// allocated, 4 layers, D32F, 64x64, and published downstream.
//
// THE CAUSE. The Vulkan validation layer names it outright, 32 times in one
// run (the only two VUIDs the run emits, 4 invocations x 8 frames, src + dst):
//
//     VUID-vkCmdCopyImage-aspectMask-00142 / -00143
//     vkCmdCopyImage(): pRegions[0].srcSubresource.aspectMask
//       (VK_IMAGE_ASPECT_COLOR_BIT) cannot specify aspects not present in
//       source image (VK_FORMAT_D32_SFLOAT)
//       src = RRPNode::MRT::perLayerScratch::cascade
//       dst = RenderedRawRasterPipelineNode::MRT::depth::cascade
//
// Those are the two textures the copy loop at RenderedRawRasterPipelineNode.cpp
// :3199-3218 names. Confirmed in Qt's own source, for both backends:
//
//   * qrhivulkan.cpp:4782 and :4792 (Qt 6.11) build the VkImageCopy region with
//         region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     unconditionally — the format is never consulted. Every copyTexture of a
//     depth-format texture is therefore an invalid VkCmdCopyImage and is
//     dropped.
//   * qrhigles2.cpp:3069-3110 records the copy, and the CopyTex case at
//     qrhigles2.cpp:4077+ executes it by attaching the SOURCE to
//     GL_COLOR_ATTACHMENT0 of a scratch FBO and calling glCopyTexSubImage3D.
//     A D32F texture on GL_COLOR_ATTACHMENT0 leaves the FBO incomplete, and
//     glCopyTexSubImage* reads the colour read-buffer in any case. Same
//     colour-only assumption, no validation layer to announce it.
//
// So QRhiResourceUpdateBatch::copyTexture is a COLOUR-ONLY operation, and the
// whole PER_LAYER depth shim (hpp:260-279, cpp:3199-3218) is built on it. The
// shim cannot work as written on any backend. That is the defect.
//
// WHAT WAS ELIMINATED, AND HOW (all measured, no product edits):
//  1. "the array/PER_LAYER never happened" — eliminated. The two structural
//     groups pass: array=1 layers=4 d32f=1 64x64, published by textureForOutput.
//     Vulkan names RRPNode::MRT::perLayerScratch::cascade, so m_perLayerIsDepth
//     was true and the scratch branch (cpp:1183-1230) ran.
//  2. "the copy loop never runs" — eliminated. It runs: the 32 validation
//     errors ARE the copy loop, 4 per frame, naming both its textures.
//  3. "PASSINDEX/invocation loop is broken" — eliminated by the CONTROL case at
//     the bottom of this file: the same 4-layer array shape with a PER_LAYER
//     COLOUR target gives four distinct, closed-form, correctly-placed colours
//     on both backends. 4 invocations ran and PASSINDEX advanced 0..3.
//  4. "the probe mis-samples, so zero is not proof of an unwritten texture" —
//     eliminated by the same CONTROL: identical csf-array-image-read.fs probe,
//     identical GrabsFromSource wiring, identical RGBA8 sink, non-zero values
//     land in their own quadrants. It also empirically confirms probe_layer()'s
//     quadrant map and the height-1-y row order used below.
//  5. "copyTexture is simply broken here" — eliminated as too broad:
//     test_gfx_cubemap_six_faces is GREEN on this same box in the same session
//     (27 assertions), and it is driven entirely by copyTexture moving RGBA8
//     array layers into cube faces (cpp:3215-3243). copyTexture works for
//     colour and fails for depth. That is exactly the split the VUID states.
//
// NOT PROVEN, and it cannot change the verdict: whether the fragment's depth
// reaches the scratch attachment at all. The copy is invalid either way, so
// this is unobservable from downstream until the copy is fixed.
//
// A FIX would have to stop routing depth through copyTexture. Options, none
// applied here (this is a test): render each layer with a depth attachment that
// selects the layer (QRhiTextureRenderTargetDescription::setDepthLayer, which
// qrhivulkan.cpp:8649 does honour — the "Qt RHI 6.11 has no per-layer depth
// attachment" premise at hpp:269-270 looks outdated); or move the scratch
// through a depth-sampling blit shader instead of a transfer copy.
//
// SECOND, INDEPENDENT DEFECT ON THE SAME PATH (masked by the one above, so this
// case cannot currently observe it; recorded because it is real and verified in
// source): RenderedRawRasterPipelineNode.cpp:1206-1207 allocates the scratch
// render target's dummy colour attachment at QSize(1, 1), its comment at
// :1200-1205 claiming to "Mirror createDepthOnlyRenderTarget's attachment
// shape". Both overloads of that helper say the exact opposite, in comments
// written as a fix — Utils.cpp:1585-1588 and :1785-1792: "The dummy MUST match
// the depth extent, not be 1x1: the Vulkan backend derives the framebuffer and
// renderArea from the FIRST colour attachment". Verified in Qt: colour
// attachment 0 sets d.pixelSize (qrhivulkan.cpp:8619-8620), the depth texture
// is consulted only when colorAttCount == 0 (:8661-8662), and d.pixelSize is
// the VkFramebuffer extent (:8782-8783). So the scratch pass renders into a 1x1
// framebuffer and only texel (0,0) of the 64x64 scratch is rasterised. Vulkan
// does not warn (a framebuffer smaller than its attachments is legal), and the
// run above emitted no VUID other than the two copy ones. Fix: allocate
// m_perLayerDummyColor at `sz`, as both helper overloads do.
//
// -----------------------------------------------------------------------------
// SPEC NOTES (SPEC-SCENE-RENDER-TESTS.md §3.3, P2-2)
// -----------------------------------------------------------------------------
//  1. The row offers "`rr-perlayer.{vs,fs}` / `shadow_cascades.frag`". Both
//     exist, and NEITHER can drive the copy shim: rr-perlayer's target is a
//     COLOR array (rr-perlayer.fs:14), shadow_cascades needs the scene stack.
//     Hence the new pair.
//  2. The row's cite `RenderedRawRasterPipelineNode.hpp:260-279` is accurate but
//     points at the STATE DECLARATION; the copyTexture loop it describes lives
//     at RenderedRawRasterPipelineNode.cpp:3199-3218.
//  3. "copy every layer from the scratch without advancing i -> all layers
//     identical" conflates two edits. Freezing the copy's destination layer
//     (:3212) leaves layers 1..3 UNWRITTEN, not identical; the "identical"
//     outcome is the passIndex freeze (:3123). Both are given above.
//  4. The row assumes the copy shim works. Measured, it does not, on any
//     backend: QRhi's copyTexture is colour-only (see the MEASURED section).
//     P2-2 is therefore a `[!shouldfail]` RED pin on a real defect, not a
//     passing case. The spec should record that, and the pin should stay until
//     the PER_LAYER depth path stops routing depth through a transfer copy —
//     at which point it flips to an unexpected-pass and someone removes the tag.
// =============================================================================
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <QtGlobal>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

constexpr int kLayers = 4;

// Closed-form window depth for layer i, derived in the header:
//   z_gl(i) = 0.4*i - 0.6, w = 1  ->  d(i) = 0.5*z_gl(i) + 0.5 = 0.2*i + 0.2.
constexpr double expected_depth(int layer) noexcept
{
  return 0.2 * layer + 0.2;
}

// ... as the RGBA8 red byte the sink delivers.
constexpr int expected_red(int layer) noexcept
{
  // 0.2 -> 51, 0.4 -> 102, 0.6 -> 153, 0.8 -> 204. Written as integer
  // arithmetic so the constant is not a transcribed magic number.
  return (51 * (layer + 1));
}

struct LayerFacts
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // ---- Pixel-free half A: the shader's own decision logic, straight off the
  // parsed descriptor the engine dispatches on (:631-669).
  bool desc_seen = false;
  std::string exec_type;
  std::string exec_target;
  std::string out_type;
  int desc_layers = 0;
  bool depth_compare_always = false;

  // ---- Pixel-free half B: what the producer actually published as the
  // downstream handle for its depth OUTPUT, harvested through the public
  // NodeRenderer::textureForOutput while the pipeline is alive.
  bool renderer_found = false;
  bool out_tex_found = false;
  bool out_is_array = false;
  int out_layers = 0;
  bool out_is_d32f = false;
  int out_w = 0;
  int out_h = 0;
  std::string out_name;

  // ---- Pixel half: the sampler2DArray probe's 2x2 quadrant grid.
  ReadbackImage view;
};

LayerFacts run_perlayer_depth(score::gfx::GraphicsApi backend)
{
  LayerFacts f;
  f.backend = backend_name(backend);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    // Producer: procedural PER_LAYER depth array (no geometry node, no SSBOs).
    const int prod = p.addRaster(
        corpus("rr-perlayer-depth.vs"), corpus("rr-perlayer-depth.fs"));
    // Probe: sampler2DArray, layer L in quadrant L (existing corpus shader).
    const int view = p.addIsf(corpus("csf-array-image-read.fs"));
    if(prod < 0 || view < 0)
    {
      f.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }

    // Descriptor facts — available before any GPU work, so they survive even a
    // backend that later fails to bring up an offscreen target.
    {
      const auto& d = p.isf(prod)->descriptor();
      f.desc_seen = true;
      f.exec_type = d.execution_model.type;
      f.exec_target = d.execution_model.target;
      if(!d.outputs.empty())
      {
        f.out_type = d.outputs[0].type;
        f.desc_layers = d.outputs[0].layers;
      }
      f.depth_compare_always = d.default_state.depth_compare.has_value()
                               && *d.default_state.depth_compare == "always";
    }

    // 64x64 sink => four 32x32 quadrant cells, matching the OUTPUT's 64x64.
    const int sink = p.addSink({64, 64});
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

    // What did the producer publish for its depth OUTPUT? For a depth output
    // textureForOutput returns m_mrtRenderTarget.depthTexture (:170-171) — the
    // very array the copy loop at :3215 writes into. If this is not a 4-layer
    // D32F array, the PER_LAYER depth path was never set up and no pixel
    // statement below would mean anything.
    auto* outPort = p.imageOut(prod, 0);
    auto& node = *p.isf(prod);
    for(auto& [renderList, renderer] : node.renderedNodes)
    {
      if(!renderList || !renderer)
        continue;
      f.renderer_found = true;
      if(QRhiTexture* tex = renderer->textureForOutput(*outPort))
      {
        f.out_tex_found = true;
        f.out_is_array = tex->flags().testFlag(QRhiTexture::TextureArray);
        f.out_layers = tex->arraySize();
        f.out_is_d32f = (tex->format() == QRhiTexture::D32F);
        f.out_w = tex->pixelSize().width();
        f.out_h = tex->pixelSize().height();
        f.out_name = tex->name().toStdString();
      }
    }
  });
  return f;
}

// Quadrant centre for layer L in the probe's own uv space, mapped into readback
// coordinates. csf-array-image-read.fs:15-22:
//     uv.x < 0.5 && uv.y < 0.5 -> layer 0      uv.x >= 0.5 && uv.y < 0.5 -> 1
//     uv.x < 0.5 && uv.y >= 0.5 -> layer 2     uv.x >= 0.5 && uv.y >= 0.5 -> 3
// x maps straight through; uv.y < 0.5 is the BOTTOM half of the readback
// (GfxCubemapSixFaces.cpp:232-244), hence height-1-y.
std::array<uint8_t, 4> probe_layer(const ReadbackImage& img, int layer)
{
  const bool right = (layer % 2) == 1;   // uv.x >= 0.5
  const bool highV = layer >= 2;         // uv.y >= 0.5
  const int x = right ? (3 * img.width / 4) : (img.width / 4);
  const int yUv = highV ? (3 * img.height / 4) : (img.height / 4);
  return img.at(x, img.height - 1 - yUv);
}

// Diagnostic only, never asserted: the texel-(0,0) corner of each quadrant.
// Under the 1x1-render-area defect described in the header, this is the ONE
// texel of the scratch that was actually drawn, so a run where the corners are
// right and the centres are wrong fingers RenderedRawRasterPipelineNode.cpp
// :1206-1207 rather than the copy loop.
std::array<uint8_t, 4> probe_layer_corner(const ReadbackImage& img, int layer)
{
  const bool right = (layer % 2) == 1;
  const bool highV = layer >= 2;
  // The cell's uv-origin corner, i.e. the texel-(0,0) end of the layer.
  const int x = right ? (img.width / 2) : 0;
  const int yUv = highV ? (img.height / 2) : 0;
  return img.at(x, img.height - 1 - yUv);
}

struct ControlFacts
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  ReadbackImage view;
};

// PER_LAYER *colour* array through the identical probe + sink. See the CONTROL
// block below the main case for what a green result here proves.
ControlFacts run_perlayer_colour(score::gfx::GraphicsApi backend)
{
  ControlFacts f;
  f.backend = backend_name(backend);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod
        = p.addRaster(corpus("rr-perlayer.vs"), corpus("rr-perlayer.fs"));
    const int view = p.addIsf(corpus("csf-array-image-read.fs"));
    if(prod < 0 || view < 0)
    {
      f.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
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
  });
  return f;
}
}

// =============================================================================
// EXPECTED TO FAIL — `[!shouldfail]` pin. This case asserts the CORRECT
// per-layer depths and the product does not deliver them. It fails-as-expected
// today and turns into a loud unexpected-pass the day the defect below is
// fixed, which is the signal we want. Nothing here is weakened to get green:
// every assertion still demands 51/102/153/204.
//
// THE DEFECT: QRhi's copyTexture is COLOUR-ONLY, and the whole PER_LAYER depth
// shim (hpp:260-279, cpp:3199-3218) is built on it.
//   * qrhivulkan.cpp:4782 and :4792 build the VkImageCopy region with
//         region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     unconditionally — the texture format is never consulted.
//   * qrhigles2.cpp:4077+ executes the same op by attaching the SOURCE to
//     GL_COLOR_ATTACHMENT0 of a scratch FBO and calling glCopyTexSubImage3D,
//     which reads the colour read-buffer. Same assumption, no validation layer
//     to announce it — which is why OpenGL fails silently and identically.
//   * Measured: VUID-vkCmdCopyImage-aspectMask-00142 / -00143 fire 32 times in
//     one run (4 invocations x 8 frames, src + dst), naming
//     RRPNode::MRT::perLayerScratch::cascade and
//     RenderedRawRasterPipelineNode::MRT::depth::cascade — the copy loop's own
//     two textures. Every copy is dropped, so every layer keeps its 0.0 clear.
//
// SECOND DEFECT, on the same path, currently MASKED by the first (the array
// reads zero either way, so this case cannot observe it):
// RenderedRawRasterPipelineNode.cpp:1206-1207 allocates the scratch render
// target's dummy colour attachment at QSize(1, 1). Colour attachment 0 is what
// sets the render target's pixelSize (qrhivulkan.cpp:8619-8620; the depth
// texture is consulted only when colorAttCount == 0, :8661-8662) and that
// pixelSize is the VkFramebuffer extent (:8782-8783), so the scratch pass
// rasterises into a 1x1 framebuffer. Both createDepthOnlyRenderTarget overloads
// already allocate their dummy at the depth extent for exactly this reason
// (Utils.cpp:1589-1592, :1790-1792) — this one does not, despite its comment at
// :1200-1205 claiming to mirror them.
//
// THE SHIM'S PREMISE IS OUTDATED: hpp:269-270 states "Qt RHI 6.11 exposes no
// per-layer depth attachment", which is what motivates the scratch-and-copy
// dance in the first place. qrhivulkan.cpp:8649 honours
// QRhiTextureRenderTargetDescription::setDepthLayer for an array depth texture.
// So the real fix is probably not to repair the copy but to stop routing depth
// through a transfer copy at all — attach the layer directly, or blit through a
// depth-sampling shader. That is a DESIGN decision about the product's render
// path; it is FLAGGED HERE, NOT ATTEMPTED. This file is a test.
//
// THE 16 REDS ARE THE PRODUCT, NOT THE RIG. The proof is the sibling case
// "per-layer colour renders into every array layer (probe-path control)" at the
// bottom of this file, which is plain GREEN on both backends: same 4-layer
// array, same csf-array-image-read.fs probe, same GrabsFromSource wiring, same
// RGBA8 sink, same probe_layer() quadrant map — only the target is colour
// instead of depth. It rules out the invocation loop, PASSINDEX, the array
// allocation, the sampler2DArray binding, the quadrant map and the readback row
// order. The only link it does not share is the depth copy.
// =============================================================================
TEST_CASE(
    "per-layer depth renders into every array layer",
    "[gfx][l3][raster][execmodel][perlayer][depth]")
{
#if QT_VERSION < QT_VERSION_CHECK(6, 12, 0)
  // Per-layer depth needs QRhiTextureRenderTargetDescription::setDepthLayer,
  // which arrives in Qt 6.12. Below that the engine REFUSES the mode with a
  // qWarning and falls back to SINGLE (RenderedRawRasterPipelineNode.cpp), so
  // there is no cascade to assert -- by design, and out loud. SKIP, not fail:
  // an unavailable feature on an older Qt is not a defect in this build.
  // Releases target 6.12+.
  SKIP(
      "per-layer depth requires Qt >= 6.12 (setDepthLayer); this build is "
      QT_VERSION_STR);
#else
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const LayerFacts f = run_perlayer_depth(be);
  if(f.skipped)
    SKIP(f.backend + ": " + f.skip_reason);

  INFO("backend=" << f.backend << " error='" << f.error << "'");
  REQUIRE(f.error.empty());

  // ---- [PER_LAYER decision logic] -------------------------------------------
  // Pixel-free. The engine picks the scratch+copy path purely from these four
  // descriptor facts (RenderedRawRasterPipelineNode.cpp:639-640 for the mode,
  // :659-669 for "the TARGET output is a depth entry", :1188 for "LAYERS > 1,
  // else fall back to SINGLE"). Asserting them says the shader really does ask
  // for the path this case is about.
  REQUIRE(f.desc_seen);
  CHECK(f.exec_type == "PER_LAYER");
  CHECK(f.exec_target == "cascade");
  CHECK(f.out_type == "depth");
  CHECK(f.desc_layers == kLayers);
  // The oracle below assumes nothing rejects a fragment on depth compare.
  CHECK(f.depth_compare_always);

  // ---- [depth array published] ----------------------------------------------
  // Pixel-free. textureForOutput's depth branch (:170-171) must hand downstream
  // the multi-layer D32F array the copy loop writes into, at the declared size.
  // Holds on every backend that could build the pipeline, Null included.
  REQUIRE(f.renderer_found);
  REQUIRE(f.out_tex_found);
  INFO(
      "published depth output: name='" << f.out_name << "' array=" << f.out_is_array
                                       << " layers=" << f.out_layers
                                       << " d32f=" << f.out_is_d32f << " "
                                       << f.out_w << "x" << f.out_h);
  CHECK(f.out_is_array);
  CHECK(f.out_layers == kLayers);
  CHECK(f.out_is_d32f);
  CHECK(f.out_w == 64);
  CHECK(f.out_h == 64);

  // ---- Pixel half ------------------------------------------------------------
  // Null does not rasterise, so there is nothing to sample; the two structural
  // groups above already ran. Same shape as GfxCubemapSixFaces.cpp:202-212 and
  // what P2-15 asks of the Null lane.
  if(f.backend == "Null")
  {
    SUCCEED(
        f.backend
        + ": no rasterisation, so the per-layer depth pixel oracle is not "
          "expressible; PER_LAYER decision logic and the published depth array "
          "were asserted above");
    return;
  }

  REQUIRE(f.view.valid());
  const auto& img = f.view;
  REQUIRE(img.width >= 4);
  REQUIRE(img.height >= 4);

  std::array<std::array<uint8_t, 4>, kLayers> got{};
  for(int l = 0; l < kLayers; ++l)
    got[l] = probe_layer(img, l);

  // Diagnostic (not an assertion): centres vs texel-(0,0) corners.
  {
    std::string centres, corners;
    for(int l = 0; l < kLayers; ++l)
    {
      const auto c = probe_layer_corner(img, l);
      centres += " L" + std::to_string(l) + "=" + std::to_string(int(got[l][0]));
      corners += " L" + std::to_string(l) + "=" + std::to_string(int(c[0]));
    }
    INFO("red at quadrant centres:" << centres);
    INFO("red at quadrant (0,0) corners:" << corners);
    INFO(
        "expected red: L0=" << expected_red(0) << " L1=" << expected_red(1)
                            << " L2=" << expected_red(2) << " L3="
                            << expected_red(3));

    // ---- [per-layer depth] --------------------------------------------------
    // Layer i carries ITS OWN closed-form depth. Only the red channel is
    // asserted: a D32F sampled through a non-comparison sampler puts the depth
    // in .r and leaves g/b backend-dependent.
    constexpr int tol = 3;
    for(int l = 0; l < kLayers; ++l)
    {
      INFO(
          "layer " << l << ": expected depth " << expected_depth(l) << " -> red "
                   << expected_red(l) << ", got red " << int(got[l][0]));
      CHECK(std::abs(int(got[l][0]) - expected_red(l)) <= tol);
    }

    // ---- [depth multiset] ---------------------------------------------------
    // Every predicted depth is found exactly ONCE across the four probes. This
    // is the layer-identity statement that does not depend on the readback's
    // row order: fewer than one means a layer was dropped or overwritten (the
    // copy shim not advancing its destination), more than one means two layers
    // carry the same depth (the invocation index not advancing).
    for(int l = 0; l < kLayers; ++l)
    {
      int found = 0;
      for(int k = 0; k < kLayers; ++k)
        if(std::abs(int(got[k][0]) - expected_red(l)) <= tol)
          ++found;
      INFO(
          "depth " << expected_depth(l) << " (red " << expected_red(l)
                   << ") found " << found << " time(s) across the four probes");
      CHECK(found == 1);
    }
  }
#endif
}

// =============================================================================
// CONTROL — the probe path itself, proven with values known to be non-zero.
//
// Same 4-layer TextureArray shape, same csf-array-image-read.fs probe, same
// GrabsFromSource wiring, same RGBA8 sink — but a PER_LAYER *colour* target
// (corpus/rr-perlayer.{vs,fs}, already in the tree). Its fragment stage writes
//     isf_FragColor = vec4(l/3, 1 - l/3, 0.25, 1)   with l = PASSINDEX
// (rr-perlayer.fs:30-31), so the closed-form RGBA8 per layer is
//     L0 (  0,255, 64)   L1 ( 85,170, 64)   L2 (170, 85, 64)   L3 (255,  0, 64)
// (0.25*255 = 63.75 -> 64; 1/3*255 = 85; 2/3*255 = 170.)
//
// If this is GREEN and the depth case is RED, then in the depth case:
//   * the PER_LAYER invocation loop ran all 4 invocations,
//   * PASSINDEX advanced 0..3 (each layer carries a different value),
//   * the 4-layer array was allocated, bound as a sampler2DArray and sampled,
//   * the quadrant->layer mapping and the readback row order used by
//     probe_layer() are right,
// and the ONLY link left that can be broken is the depth copy itself. That is
// what makes the zero-everywhere result in the case above a product defect
// rather than a mis-sampled texture.
// =============================================================================
TEST_CASE(
    "per-layer colour renders into every array layer (probe-path control)",
    "[gfx][l3][raster][execmodel][perlayer][control]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  ControlFacts f = run_perlayer_colour(be);
  if(f.skipped)
    SKIP(f.backend + ": " + f.skip_reason);

  INFO("backend=" << f.backend << " error='" << f.error << "'");
  REQUIRE(f.error.empty());

  if(f.backend == "Null")
  {
    SUCCEED(f.backend + ": no rasterisation, control not expressible");
    return;
  }

  REQUIRE(f.view.valid());
  const auto& img = f.view;

  // Closed form from rr-perlayer.fs:31, not an observed value.
  const std::array<std::array<uint8_t, 4>, kLayers> expected{{
      {0, 255, 64, 255},
      {85, 170, 64, 255},
      {170, 85, 64, 255},
      {255, 0, 64, 255},
  }};

  std::array<std::array<uint8_t, 4>, kLayers> got{};
  for(int l = 0; l < kLayers; ++l)
    got[l] = probe_layer(img, l);

  std::string dump;
  for(int l = 0; l < kLayers; ++l)
    dump += " L" + std::to_string(l) + "=(" + std::to_string(int(got[l][0])) + ","
            + std::to_string(int(got[l][1])) + "," + std::to_string(int(got[l][2]))
            + ")";
  INFO("probe RGB per layer:" << dump);

  // [control: per-layer colour] — layer identity through the very same probe.
  for(int l = 0; l < kLayers; ++l)
  {
    INFO(
        "layer " << l << ": expected ("
                 << int(expected[l][0]) << "," << int(expected[l][1]) << ","
                 << int(expected[l][2]) << ") got (" << int(got[l][0]) << ","
                 << int(got[l][1]) << "," << int(got[l][2]) << ")");
    CHECK(near(got[l], expected[l], 3));
  }
}
