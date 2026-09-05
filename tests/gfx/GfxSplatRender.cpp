// =============================================================================
// A gaussian splat renders, through the path the user actually uses.
//
// Threedim/Splat/ is not that path. The legacy "Splat" process (uuid
// cdc15a16-e856-4e02-9339-7d9e48da10ce, Threedim/Splat/Metadata.hpp, renderer
// Threedim/Splat/GaussianSplatNode.{hpp,cpp}) is used by no document of the
// 263-document corpus at $SCORE_CORPUS_DIR (~/ossia/score-corpus): its uuid,
// the JSON ObjectName "Splat", and the uuid of the "Splat loader" avnd process
// that feeds it (bab30770-d6d7-4727-ad43-38eacdd910a7, Threedim/
// BufferLoader.hpp, the only caller of Threedim::GaussianSplatsFromPly) each
// occur in 0 files. "Splat" appears in 2 documents (2026/splats-room.score,
// 2026/test-3dgs-full.score), only inside user shader and process names
// ("02_DrawSplat", "DrawSplat.frag"), never as a process type. Reproduce:
//   grep -rl 'cdc15a16-e856-4e02-9339-7d9e48da10ce' ~/ossia/score-corpus
//   grep -rl 'bab30770-d6d7-4727-ad43-38eacdd910a7' ~/ossia/score-corpus
//
// The depth sort, the radix passes and the `splatCount` member all live on
// that dead path (GaussianSplatNode.cpp: depth key, radix,
// `cb.draw(6, splatCount, 0, 0)`), and none of it is reachable from the chain
// a real .score builds. This file renders the live path instead.
//
// THE PATH THIS FILE DRIVES -- read off 2026/splats-room.score, the motivating
// document; process graph transcribed from its cable list:
//
//   asset_loader#1 (uuid 2f6a8c41-7d93-4e5b-b1c8-4e3f9a7d2c5b,
//                   Threedim/AssetLoader.hpp)   [<LIBRARY>:packages/room.ply]
//     -> scene_group#24  (8a3b5e2d-..., Threedim/SceneGroup.hpp)
//     -> Scene Preprocessor#2 (a8f2c6d0-..., score::gfx::ScenePreprocessorNode)
//     -> Flattened Scene Filter#21 (7a1b3c5d-..., FlattenedSceneFilterNode)
//          Mode = 12, Match = 0, Format ID = "3dgs.classic"
//     -> CSF#36 "01_Decode" (a5bbffe0-...)   [geoIn]
//     -> Render Pipeline#53 "02_DrawSplat" (dbfc2101-...)  [Geometry In]
//
// 2026/test-3dgs-full.score is the same head with an 8-stage CSF chain
// (01_Decode / 02_TileEmit / 03_RadixHistogram / 04_RadixScan /
// 05_RadixScatter / 06_TileRanges / 07_TileRender / 08_Composite).
//
// WHAT IS REAL HERE, AND WHAT IS THE HARNESS.
//   Real, exercised verbatim:
//     * Threedim::AssetLoader::ins::asset_t::process() -- the shipped
//       extension dispatch: AssetLoader.cpp routes a splat-shaped .ply through
//       PrimitiveCloud::ply_is_splat_shaped -> parse_ply -> sceneStateFromCloud,
//       driven as tests/unit/AssetLoaderFailure.cpp and the avnd runtime drive
//       it.
//     * Threedim::PrimitiveCloud::parse_ply (PlyParser.cpp) -- autodetects
//       format_id "3dgs.classic" and struct_type_name "Splat3DGS", sets
//       primitive_count and row_stride from the column set (60 B for the
//       15-column classic schema, pinned by tests/unit/PrimitiveCloudTest.cpp).
//     * score::gfx::ScenePreprocessorNode -- the whole primitive-cloud branch,
//       rebuildPrimitiveClouds: bucketing by hash_string(format_id), the
//       raw_splats concat and upload, cloud_meta / cloud_id_lookup, the
//       indirect command (indexOrVertexCount = total_primitives, instanceCount
//       1) and the emitted bucket geometry (`g.vertices =
//       (int)b.total_primitives`, `g.instances = 1`, points topology).
//     * score::gfx::FlattenedSceneFilterNode in mode 12 with match_str
//       "3dgs.classic" (`case 12: return g.filter_tag == match_str_hash;`),
//       the configuration both corpus documents carry.
//     * The CSF node (Gfx::ProgramCache + score::gfx::ISFNode compute path)
//       and the RAW_RASTER_PIPELINE consumer and draw.
//   Harness, stated so nobody mistakes it for coverage:
//     * The Scene producer is a data-only score::gfx::ProcessNode publishing
//       the AssetLoader's `outputs.scene_out.scene` on a Types::Scene port with
//       NodeRenderer::process(port, scene_spec, key) -- the production publish
//       shape, the same call tests/gfx/SceneMergeMemo.cpp uses. It stands in
//       for oscr::GfxNode<AssetLoader> + scene_group#24, an N->1 scene merge
//       that is the identity for a single producer and is already pinned by
//       SceneMergeMemo.cpp.
//     * AssetLoader::init/update/release (the RawTransform slot claim) are not
//       called: the TRS wrap is applied on the CPU inside operator() via
//       wrapSceneWithTransform, and this fixture leaves Position, Rotation and
//       Scale at their defaults.
//     * No Camera and no Light are wired. The corpus scores have both, but
//       this file's CSF writes NDC positions directly and never reads the
//       `camera` auxiliary, so a camera would only add a failure mode. The
//       camera pack is covered by tests/gfx/GfxEnvRenderTargetSize.cpp.
//
// BUILD. `score_add_gfx_test(splat_render GfxSplatRender.cpp)` is not enough:
// the loader entry points are hidden-visibility inside
// libscore_plugin_threedim.so, so the AssetLoader / PrimitiveCloud / Ply
// translation units have to be compiled in through score_plugin_hidden_sources
// -- the test_gfx_splat_render block at the end of tests/gfx/CMakeLists.txt
// does that, reusing
// the test_unit_threedim_3rdparty static lib tests/unit defines.
// ctest name: test_gfx_splat_render  (`ctest -R gfx_splat_render`).
//
// -----------------------------------------------------------------------------
// THE FIXTURE -- synthesised in-test, nothing binary committed.
//
// A binary-little-endian .ply written byte-for-byte the way
// tests/unit/PrimitiveCloudTest.cpp (`classic_header`) and
// tests/threedim/VoxelAssets.cpp synthesise theirs. Header format taken from
// the product parser, not guessed:
//   * miniply.cpp holds the whole grammar: literal "ply" line, then
//     `format <ascii|binary_little_endian|binary_big_endian> <maj>.<min>`, the
//     element/property block, then `end_header` + optional whitespace/CR + a
//     literal '\n'. The body starts at the byte after it.
//   * `element vertex N` -- N is what miniply reports as num_rows(), which is
//     what Ply.cpp assigns to GaussianSplatData::splatCount.
//   * The 15 all-float classic-3DGS columns, in this order:
//       x y z f_dc_0 f_dc_1 f_dc_2 f_rest_0 opacity
//       scale_0 scale_1 scale_2 rot_0 rot_1 rot_2 rot_3
//     PlyParser's fingerprint (PlyParser.hpp: f_dc_0/1/2 + f_rest_* +
//     scale_0/1/2 + rot_0/1/2/3 + opacity) stamps format_id "3dgs.classic" and
//     struct_type_name "Splat3DGS"; PrimitiveCloudTest.cpp pins that this
//     header yields row_stride 60 and points topology. ply_is_splat_shaped
//     accepts it because there is no `face` element and the columns fall
//     outside the standard mesh set.
//   * Rows are tightly packed float32 LE, 15 per row, in declared order.
//
// kSplats = 240 is a number this file chose and writes into the `element
// vertex` line; it is never read back from a parse. 240 = 24 x 10 blocks of
// 2x2 pixels in a 64x64 frame, and 240 <= 255 so a per-row identity survives
// an RGBA8 byte exactly (i/255.0 -> round(255 * i/255) == i).
//
// -----------------------------------------------------------------------------
// THE ORACLES.
//
// (a) THE SPLAT COUNT REACHING THE DRAW -- two independent measurements.
//
//   M1, CPU, off the render path entirely:
//     Threedim::GaussianSplatsFromPly(path).splatCount (Ply.hpp / Ply.cpp,
//     assigned from miniply::PLYReader::num_rows()).
//   M2, GPU, counted off the framebuffer:
//     the number of 2x2 blocks lit in the readback.
//
//   They share the file on disk and nothing else:
//     * different parser TU. M1 is Ply.cpp, M2 flows through
//       PrimitiveCloud/PlyParser.cpp. Neither calls the other, and
//       AssetLoader.cpp reaches only the second: the sole caller of
//       GaussianSplatsFromPly anywhere in src/ is BufferLoader.hpp, the dead
//       "Splat loader".
//     * different field. M1 reads `num_rows()` off the vertex element header
//       line. M2's number is PlyParser's `primitive_count`, summed into
//       Bucket::total_primitives, written into `g.vertices`, substituted into
//       the CSF's "INSTANCE_COUNT": "$VERTEX_COUNT_geoIn", and issued as the
//       instance count of a real draw -- not through the indirect command,
//       which is dead weight on this chain; see negative control 4.
//     * different medium. M1 is a struct member on the CPU, M2 is lit pixels
//       in a read-back RGBA8 image. A regression that changes one leaves the
//       other alone -- see negative control 1.
//
// (b) THE FRAME IS NON-UNIFORM: both lit and unlit pixels, more than one
//   distinct colour. Guards against "any constant frame satisfies (a)".
//
// (c) "DISABLING THE SORT CHANGES THE FRAME" cannot be done on this path, so
//   this file asserts the inverse instead.
//     * The live path (AssetLoader -> ScenePreprocessorNode -> CSF) contains
//       no depth sort. rebuildPrimitiveClouds concatenates the rows in file
//       order, a straight memcpy per cloud with `dst += bytes`, and nothing
//       reorders them.
//     * The only sort in the product is on the dead path: GaussianSplatNode's
//       depth key plus 2 x 8-bit radix. Its enable flag, GaussianSplatNode.hpp
//       `bool enableSorting{true};`, is read in 6 places and written in zero,
//       and Gfx::Splat::Model::init (Threedim/Splat/Process.cpp) declares 11
//       ports, none of them a sort control. There is no user-facing sort
//       control on either path.
//     * In the real scores the sort is user shader content, not a product
//       feature: test-3dgs-full.score carries 03_RadixHistogram /
//       04_RadixScan / 05_RadixScatter as authored CSF stages, and its
//       07_TileRender output is not even cabled to the composite.
//       splats-room.score has a single-stage CSF chain, no sort of any kind,
//       and EnableBlend=false on the draw.
//   Asserting "turning off the sort changes the frame" here would mean the
//   test authoring the sort itself and then asserting its own shader ran.
//
//   (c1) CLOSED FORM, per pixel: the row that reaches instance i is row i.
//        The fragment shader emits G = the row's own payload identity
//        (f_dc_0 == i/255, carried as translation.w -> v_buf_id) and
//        B = gl_InstanceIndex/255 (v_draw_id). G == B on every lit pixel means
//        the engine handed row i to instance i: draw order is file order,
//        unreordered, end to end.
//   (c2) DIFFERENCE ORACLE, the inverse assertion: the same 240
//        splats are loaded a second time from a file whose rows are reversed.
//        Every splat keeps its own position (positions come from the row
//        payload, not from the instance index), so the set of lit blocks is
//        identical, but every block's identity byte must become 239 - itself.
//        A depth ordering between the file and the draw would present the same
//        depth set from both files and make the two frames identical; they
//        must instead differ, exactly mirrored. Because every splat owns its
//        own 2x2 block no two fragments land on the same pixel, so this oracle
//        is independent of depth test, depth write and blend state -- which is
//        why it is preferred over the obvious "stack them all on one pixel"
//        formulation, which would go spuriously red under a depth-tested
//        pipeline.
//
//   (c1)+(c2) are green today and are a pin on a gap: they encode "the live
//   splat path applies no depth ordering". If a product-side sort ever lands
//   in rebuildPrimitiveClouds or in a shipped format preset, both go red, and
//   that red is the signal to rewrite this case in sort-on/off form, not to
//   weaken the assertion.
//
// (d) NO GOLDEN. Nothing here blesses an image; every expectation is a count
//   or a closed form. Splat rasterisation is order-dependent, so a reference
//   image would be rot bait.
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROLS, on OpenGL and Vulkan.
//
//   1. COUNT (product-side, hits M2 only). In ScenePreprocessorNode.cpp's
//      emitted bucket geometry, `g.vertices = (int)b.total_primitives;` ->
//      `(int)(b.total_primitives / 2)`. What reddens: litBlocks
//      120 != 240, litPixels 480 != 960, the M1-vs-M2 agreement `240 == 120`,
//      and `missing == 0` -> 120 identities never drawn at all. M1
//      (`plySplatCount == 240`, straight out of Ply.cpp) stays green, and so
//      do both order oracles over the 120 instances that do get drawn: that is
//      what makes the two measurements independent. The two saturation
//      counters, `fwd.idMirrorMismatch == 4*kSplats` and
//      `rev.idBufMismatch == 4*kSplats`, are absolute pixel counts and redden
//      here too -- intentional, they are the "on every pixel, not merely on
//      the ones that happened to be drawn" half of the same statement.
//
//   2. "REACHES THE DRAW" (product-side). In ScenePreprocessorNode.cpp's
//      raw_splats concat, `std::memcpy(dst, cpu->data.get(), bytes)` ->
//      `std::memset(dst, 0, bytes)`. What reddens: every splat's x/y
//      and identity go to 0, so all 240 quads collapse onto one block --
//      litBlocks 4 != 240, `missing == 239`, `duplicated == 1`, and, the
//      telling one, `fwd.ids != rev.ids` fails, because with the payload gone
//      the forward and reversed frames are byte-identical. The (c2) difference
//      oracle collapses exactly when the file's bytes stop reaching the draw,
//      which is what it is for. `plySplatCount == 240` stays green.
//
//   3. ORDER / (c2) (test-side, and deliberately so). In the TEST_CASE,
//      replace write_ply(dir, "splats-rev.ply", kSplats, /*reversed=*/true)
//      with /*reversed=*/false. Only the four (c2) assertions redden, once
//      per backend: `rev.idMirrorMismatch == 0` (960 != 0),
//      `rev.idBufMismatch == 4*kSplats` (0 != 960), `fwd.ids != rev.ids`, and
//      `mirrored == compared` (0 != 960). The count, the non-uniformity and
//      the forward (c1) stay green: (c2) fails iff the two files stop
//      differing. There is no one-line product-side control for (c2), and that
//      is not an oversight -- the only product change that can redden it is
//      adding a reordering step, which is precisely the gap (c2) pins.
//      Control 2 is the product-side proof that (c2) is coupled to real data
//      flow.
//
//   4. THE INDIRECT COMMAND IS A NO-OP ON THIS CHAIN. In
//      ScenePreprocessorNode.cpp, `/*indexOrVertexCount*/
//      (uint32_t)b.total_primitives` -> `0u`: nothing reddens on either
//      backend. The preprocessor's indirect buffer
//      is only consumed by a node that draws the cloud geometry itself with
//      drawIndirect, and on the real chain nothing does -- the CSF stage
//      produces a new geometry (geoOut) whose instance count comes from the
//      resolved "$VERTEX_COUNT_geoIn" expression, i.e. from `g.vertices`, not
//      from the indirect command. Recorded because the phrase "the splat count
//      reaching the draw" invites exactly this wrong guess, and because a
//      future format preset that does consume that buffer would need its own
//      coverage; there is none today, at any level.
//
//   5. SKIPPING THE DEPTH-KEY PASS DOES NOT APPLY. The only depth-key pass in
//      the product is the dispatch in
//      Threedim/Splat/GaussianSplatNode.cpp, a TU that is not linked into
//      test_gfx_splat_render at all, so the edit turns nothing here red. Not
//      run, for that reason. On that path it is also not a usable control:
//      the sort buffers are created Immutable and never
//      uploaded, so skipping the only writer of the index buffer leaves
//      sortedIndices[] undefined while the vertex shader dereferences it
//      unconditionally. The "make both branches identical" edit there is
//      `tail.useSorting = ... ? 1u : 0u;` -> `tail.useSorting = 0u;`.
//      Recorded for whoever revives that path.
//
// -----------------------------------------------------------------------------
// GaussianSplatsFromPly is NOT on this chain: AssetLoader routes .ply to
// PrimitiveCloud::parse_ply, whose count field is `primitive_count`, not
// `splatCount`. This file keeps GaussianSplatsFromPly, but as the independent
// oracle it can honestly be.
//
// -----------------------------------------------------------------------------
// HARDWARE. Compute plus a real rasteriser. The verdict is pixels, so this
// never falls back to the Null backend: unavailable
// backends and missing compute SKIP. Run:
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_splat_render
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_splat_render
//
// Engine behaviours this file leans on, each established by running it:
//   * the CSF auxiliary alias is `geoIn_raw_splats_in` -- the emitter in
//     libisf/src/isf.cpp generates "<geo>_<aux>_in" for a read_only auxiliary,
//     as the corpus precedent `geo_stats`
//     (tests/gfx/corpus/csf-auxiliary-buffer.cs) suggested.
//   * "TYPE": "float[]" in an auxiliary layout emits a runtime-sized
//     `float rows[];` -- the array branch splits at '[' and appends the suffix
//     to the name -- and it is accepted as the last member of the SSBO on both
//     backends.
//   * a single read_only attribute ("cloud_id") is enough to make the CSF node
//     create a Geometry input port, as the engine comment in
//     ScenePreprocessorNode.cpp claims.
//   * "SEMANTIC": "custom" is accepted for that attribute.
// =============================================================================

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <Threedim/AssetLoader.hpp>
#include <Threedim/Ply.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace score::test::gfx;

namespace
{

// The committed corpus shaders (GFX_TEST_CORPUS_DIR is set by the
// test_gfx_splat_render block in tests/gfx/CMakeLists.txt). Local, like GfxPointCloudCount.cpp's, so this
// file does not have to pull in GfxProcessDoc.hpp / the scenario headers.
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

// ---------------------------------------------------------------------------
// Fixture geometry. Every constant here is chosen, never observed.
// ---------------------------------------------------------------------------

// The frame. 64x64 is the house size for the gfx L3 tests.
constexpr int kSize = 64;

// The splat count. Written into the .ply `element vertex` line.
// 24 x 10 blocks of 2x2 pixels; 240 <= 255 so the per-row identity byte is
// exact through an RGBA8 target.
constexpr int kBlockCols = 24;
constexpr int kBlockRows = 10;
constexpr int kSplats = kBlockCols * kBlockRows; // 240
static_assert(kSplats <= 255, "the identity byte must survive RGBA8 exactly");
static_assert(2 * kBlockCols <= kSize && 2 * kBlockRows <= kSize, "fits");

// The 15 all-float columns of the classic 3DGS schema, in PLY-declared order.
constexpr int kCols = 15;
constexpr const char* kColNames[kCols]
    = {"x",       "y",       "z",       "f_dc_0",  "f_dc_1",
       "f_dc_2",  "f_rest_0", "opacity", "scale_0", "scale_1",
       "scale_2", "rot_0",   "rot_1",   "rot_2",   "rot_3"};

// Column indices used by the CSF and by the fixture writer.
constexpr int kColX = 0;
constexpr int kColY = 1;
constexpr int kColFdc0 = 3; // carries the row identity i/255

// Splat i owns the 2x2-pixel block at (i % 24, i / 24).
constexpr int blockX(int i) noexcept { return i % kBlockCols; }
constexpr int blockY(int i) noexcept { return i / kBlockCols; }

// Block centre in NDC. The block spans pixels [2b, 2b+2), so its centre in
// pixel coordinates is 2b + 1 exactly, and (2*(2b+1))/64 - 1 is exact in
// binary floating point -- the quad edges land on pixel boundaries and each
// block covers exactly its own 4 pixel centres on every backend.
constexpr float blockCentreNdc(int b) noexcept
{
  return float(2 * (2 * b + 1)) / float(kSize) - 1.f;
}
// Half-extent of one block in NDC: one pixel = 2/64. MUST equal the `kHalf`
// constant baked into kDecodeCsf below -- the quad then spans exactly the
// pixel range [2b, 2b+2) in both axes, so every block covers exactly its own
// four pixel centres and nothing else, on every rasteriser.
constexpr float kBlockHalfNdc = 2.f / float(kSize);
static_assert(
    kBlockHalfNdc == 0.03125f,
    "kDecodeCsf hardcodes kHalf = 0.03125; keep the two in step");

// ---------------------------------------------------------------------------
// The .ply writer. Byte-for-byte, the VoxelAssets.cpp / PrimitiveCloudTest.cpp
// pattern: build the bytes in a std::string, write once, no library.
// ---------------------------------------------------------------------------

void append_f32_le(std::string& out, float v)
{
  static_assert(sizeof(float) == 4);
  char b[4];
  std::memcpy(b, &v, 4); // test hosts are little-endian (x86_64 / arm64 CI)
  out.append(b, 4);
}

// Row payload for splat `id` placed at block `slot`.
//   x, y      -> the block's NDC centre (drives WHERE it lands)
//   f_dc_0    -> id / 255 (drives WHICH identity byte it paints)
// The two are decoupled on purpose: `reversed` below keeps the positions in
// slot order while reversing the identities, so the lit-block SET is
// invariant and only the per-block identity moves.
void append_row(std::string& out, int slot, int id)
{
  float row[kCols] = {};
  row[kColX] = blockCentreNdc(blockX(slot));
  row[kColY] = blockCentreNdc(blockY(slot));
  row[2] = 0.f;                        // z
  row[kColFdc0] = float(id) / 255.f;   // f_dc_0 == the identity
  row[4] = 0.f;                        // f_dc_1
  row[5] = 0.f;                        // f_dc_2
  row[6] = 0.f;                        // f_rest_0
  row[7] = 1.f;                        // opacity
  row[8] = row[9] = row[10] = -5.f;    // scale_0..2 (log-space, tiny)
  row[11] = 1.f;                       // rot_0 (w)
  row[12] = row[13] = row[14] = 0.f;   // rot_1..3 (x, y, z)
  for(int c = 0; c < kCols; ++c)
    append_f32_le(out, row[c]);
}

// The complete file. `reversed` mirrors the ROW ORDER: slot s carries
// identity kSplats-1-s instead of s. Positions stay in slot order.
std::string make_ply(int n, bool reversed)
{
  std::string f;
  f += "ply\n";
  f += "format binary_little_endian 1.0\n";
  f += "comment synthesised by tests/gfx/GfxSplatRender.cpp\n";
  f += "element vertex " + std::to_string(n) + "\n";
  for(const char* c : kColNames)
  {
    f += "property float ";
    f += c;
    f += "\n";
  }
  f += "end_header\n";
  for(int s = 0; s < n; ++s)
    append_row(f, s, reversed ? (n - 1 - s) : s);
  return f;
}

std::string write_ply(const QString& dir, const char* name, int n, bool reversed)
{
  const QString path = dir + QStringLiteral("/") + QString::fromUtf8(name);
  const std::string bytes = make_ply(n, reversed);
  QFile out{path};
  if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  out.write(bytes.data(), qint64(bytes.size()));
  out.close();
  return path.toStdString();
}

// ---------------------------------------------------------------------------
// The CSF: the "01_Decode" stage of the real chain, minimised. Reads the
// cloud rows the ScenePreprocessor concatenated into the `raw_splats`
// auxiliary and emits the 6-vertex x N-instance quad topology
// ScenePreprocessorNode.cpp says the format's CSF is responsible for
// producing. Its INSTANCE_COUNT is "$VERTEX_COUNT_geoIn", i.e. the
// preprocessor's g.vertices == total_primitives, which is what makes the
// drawn instance count a function of the parsed cloud.
//
// Written to the scratch dir at run time so this case adds one file to the
// tree; the raster half reuses the committed corpus pair
// syn-instance-index-color.{vs,fs}, which already implements the
// v_buf_id / v_draw_id convention this oracle needs.
// ---------------------------------------------------------------------------
const char* const kDecodeCsf = R"CSF(/*{
  "DESCRIPTION": "minimal 3dgs.classic decode stage. Reads the ScenePreprocessor's raw_splats auxiliary (15 float32 columns per row) and emits one screen-space quad per splat: 6 vertices, INSTANCE_COUNT = $VERTEX_COUNT_geoIn. Per instance, translation.xy is the row's own x/y and translation.w is the row's f_dc_0 (its identity, i/255). Consumed by syn-instance-index-color.{vs,fs}, which paints R=1, G=translation.w, B=gl_InstanceIndex/255.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SPLAT"],
  "RESOURCES": [
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "cloud_id", "SEMANTIC": "custom", "TYPE": "uint", "ACCESS": "read_only" }
      ],
      "AUXILIARY": [
        {
          "NAME": "raw_splats",
          "ACCESS": "read_only",
          "LAYOUT": [ { "NAME": "rows", "TYPE": "float[]" } ]
        }
      ]
    },
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "6",
      "INSTANCE_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "position",    "SEMANTIC": "position",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "translation", "SEMANTIC": "translation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [6, 1, 1],  "EXECUTION_MODEL": { "TYPE": "PER_VERTEX",   "TARGET": "geoOut" } },
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE", "TARGET": "geoOut" } }
  ]
}*/

// Must match GfxSplatRender.cpp: 15 float32 columns per PLY row, and one
// block half-extent = one pixel of a 64-wide frame.
const uint  kStride = 15u;
const uint  kColX   = 0u;
const uint  kColY   = 1u;
const uint  kColId  = 3u;   // f_dc_0
const float kHalf   = 0.03125; // 2.0 / 64.0

void main()
{
    uint idx = gl_GlobalInvocationID.x;

    if(PASSINDEX == 0)
    {
        if(idx >= 6u)
            return;
        // Two triangles covering [-kHalf, +kHalf]^2 around the instance's
        // translation. w = 0 so the consumer's position.xy + translation.xy
        // is the full transform.
        vec2 c[6] = vec2[6](
            vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0),
            vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0));
        geoOut_position_out[idx] = vec4(c[idx] * kHalf, 0.0, 0.0);
    }
    else
    {
        uint n = uint(geoOut_translation_out.length());
        if(idx >= n)
            return;

        // Touch the read_only input attribute so the binding is live; the
        // value is the per-splat cloud index, always 0 for a single cloud.
        uint cid = geoIn_cloud_id_in[idx];

        uint base = idx * kStride;
        float x  = geoIn_raw_splats_in.rows[base + kColX];
        float y  = geoIn_raw_splats_in.rows[base + kColY];
        float id = geoIn_raw_splats_in.rows[base + kColId];

        geoOut_translation_out[idx]
            = vec4(x, y, float(cid) * 0.0, id);
    }
}
)CSF";

QString write_text(const QString& dir, const char* name, const char* text)
{
  const QString path = dir + QStringLiteral("/") + QString::fromUtf8(name);
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  f.close();
  return path;
}

// ---------------------------------------------------------------------------
// The Scene producer harness. Publishes a scene_spec on a Types::Scene output
// with the production call NodeRenderer::process(port, scene_spec, key).
// Stands in for oscr::GfxNode<AssetLoader> + scene_group; see the harness note
// in the header.
// ---------------------------------------------------------------------------
struct AssetSceneNode final : score::gfx::ProcessNode
{
  ossia::scene_spec spec;
  char key_tag{}; // stable per-producer source_key address

  explicit AssetSceneNode(ossia::scene_spec s)
      : spec{std::move(s)}
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~AssetSceneNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

// The render-thread half. The publish is the production call verbatim, the
// same one tests/gfx/SceneMergeMemo.cpp uses.
struct AssetSceneRenderer final : score::gfx::NodeRenderer
{
  AssetSceneNode& self;

  explicit AssetSceneRenderer(const AssetSceneNode& n)
      : NodeRenderer{n}
      , self{const_cast<AssetSceneNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
  }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!self.spec.state)
      return;
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, self.spec, (const void*)&self.key_tag);
  }
};

score::gfx::NodeRenderer*
AssetSceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new AssetSceneRenderer{*this};
}

// ---------------------------------------------------------------------------
// Frame analysis. Everything below is orientation-free: the assertions are
// over the MULTISET of block identities and over per-pixel comparisons
// between two frames rendered in the same orientation, never over an
// absolute (x, y).
// ---------------------------------------------------------------------------

struct BlockStats
{
  int litBlocks = 0;                 // blocks with >= 1 marked pixel
  int litPixels = 0;                 // marked pixels overall
  // Pixels where G (the row's own payload identity) != B (gl_InstanceIndex).
  // 0 for the forward fixture, where row i IS identity i.
  int idBufMismatch = 0;
  // Pixels where G + B != kSplats-1. 0 for the REVERSED fixture, where slot i
  // carries identity kSplats-1-i, so a draw in file order must put payload
  // identity kSplats-1-i on instance i. Both counters are closed forms of the
  // same statement -- "instance i got row i" -- read from opposite ends of the
  // pipeline (the row payload vs. the draw call's own index).
  int idMirrorMismatch = 0;
  std::vector<int> identities;       // one entry per lit block (the G byte)
  bool sawUnlit = false;
  std::array<int, 256> idHistogram{};
};

// A pixel belongs to the draw iff R is the coverage marker (syn-instance-
// index-color.fs writes R = 1.0 for every drawn fragment and nothing else
// writes to the target).
constexpr int kMarkR = 200;

BlockStats analyze(const ReadbackImage& img)
{
  BlockStats s;
  s.idHistogram.fill(0);
  if(!img.valid())
    return s;

  // Walk 2x2 blocks over the whole frame. Block granularity, not pixel
  // granularity, so a half-covered quad on some rasteriser still counts once.
  for(int by = 0; by * 2 + 1 < img.height; ++by)
  {
    for(int bx = 0; bx * 2 + 1 < img.width; ++bx)
    {
      int lit = 0;
      int id = -1;
      for(int dy = 0; dy < 2; ++dy)
      {
        for(int dx = 0; dx < 2; ++dx)
        {
          const auto p = img.at(bx * 2 + dx, by * 2 + dy);
          if(int(p[0]) >= kMarkR)
          {
            ++lit;
            ++s.litPixels;
            if(int(p[1]) != int(p[2]))
              ++s.idBufMismatch;
            if(int(p[1]) + int(p[2]) != kSplats - 1)
              ++s.idMirrorMismatch;
            if(id < 0)
              id = int(p[1]);
          }
          else
          {
            s.sawUnlit = true;
          }
        }
      }
      if(lit > 0 && id >= 0)
      {
        ++s.litBlocks;
        s.identities.push_back(id);
        ++s.idHistogram[std::size_t(id)];
      }
    }
  }
  return s;
}

// Per-pixel identity map, used by the (c2) mirror oracle. -1 = not drawn.
std::vector<int> identity_map(const ReadbackImage& img)
{
  std::vector<int> m;
  if(!img.valid())
    return m;
  m.assign(std::size_t(img.width) * std::size_t(img.height), -1);
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) >= kMarkR)
        m[std::size_t(y) * std::size_t(img.width) + std::size_t(x)] = int(p[1]);
    }
  return m;
}

// ---------------------------------------------------------------------------
// One run of the whole chain for one .ply.
// ---------------------------------------------------------------------------
struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // CPU cross-checks, taken before any GPU work.
  uint32_t plySplatCount = 0;        // M1: GaussianSplatsFromPly
  std::size_t plyBufferFloats = 0;   // M1 corroboration: N * 64 floats
  bool loaderPublished = false;      // AssetLoader produced a scene at all
  std::string cloudFormatId;         // must be autodetected "3dgs.classic"
  std::string cloudStructType;       // must be "Splat3DGS"
  int64_t cloudPrimitiveCount = -1;  // PlyParser's own count (NOT the oracle)
  uint32_t cloudRowStride = 0;

  bool imgValid = false;
  BlockStats px{};
  std::vector<int> ids;              // per-pixel identity map
  int width = 0, height = 0;
};

// Reach into the loaded scene for the single primitive_cloud_component, so
// the file can report (not assert against) what the parser produced. The
// count oracle deliberately does NOT come from here -- see "WHY THESE ARE
// INDEPENDENT" in the header.
void describe_cloud(const ossia::scene_spec& spec, Outcome& out)
{
  if(!spec.state)
    return;
  const auto& st = *spec.state;
  if(!st.roots)
    return;
  // Depth-first walk over scene_node::children, which is a vector of
  // scene_payload variants: either a nested scene_node_ptr or a component
  // (geometry_port.hpp). The .ply splat path produces exactly one cloud
  // payload (SceneFromCloud.hpp), wrapped by AssetLoader's TRS root.
  std::vector<const ossia::scene_node*> stack;
  for(const auto& r : *st.roots)
    if(r)
      stack.push_back(r.get());
  while(!stack.empty())
  {
    const auto* n = stack.back();
    stack.pop_back();
    if(!n->children)
      continue;
    for(const auto& payload : *n->children)
    {
      if(const auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
      {
        if(*child)
          stack.push_back(child->get());
      }
      else if(
          const auto* cl
          = ossia::get_if<ossia::primitive_cloud_component_ptr>(&payload))
      {
        if(*cl)
        {
          out.cloudFormatId = (*cl)->format_id;
          out.cloudStructType = (*cl)->struct_type_name;
          out.cloudPrimitiveCount = (int64_t)(*cl)->primitive_count;
          out.cloudRowStride = (*cl)->row_stride;
          return;
        }
      }
    }
  }
}

Outcome run_chain(
    score::gfx::GraphicsApi api, const std::string& plyPath,
    const QString& csfPath)
{
  Outcome out;

  // --- M1, CPU, off the render path: the OTHER PLY reader in this repo. ---
  // Ply.cpp does reader(filename.data()) with no NUL-termination copy, so hand
  // it a std::string whose data() is NUL-terminated.
  {
    const auto d = Threedim::GaussianSplatsFromPly(std::string_view{plyPath});
    out.plySplatCount = d.splatCount;
    out.plyBufferFloats = d.buffer.size();
  }

  // --- The real AssetLoader dispatch, driven as the avnd runtime drives it
  // (AssetLoaderFailure.cpp). For .ply the parser reads from disk and only
  // needs the name, but the bytes are handed over too, as the runtime does.
  std::unique_ptr<Threedim::AssetLoader> loader;
  {
    QFile f{QString::fromStdString(plyPath)};
    QByteArray raw;
    if(f.open(QIODevice::ReadOnly))
      raw = f.readAll();

    halp::text_file_view tv;
    tv.filename = plyPath;
    tv.bytes = std::string_view{raw.constData(), std::size_t(raw.size())};

    auto apply = Threedim::AssetLoader::ins::asset_t::process(tv);
    if(!apply)
    {
      out.error = "AssetLoader refused the synthesised .ply";
      return out;
    }
    loader = std::make_unique<Threedim::AssetLoader>();
    apply(*loader);
    // Publishes outputs.scene_out.scene / .dirty.
    (*loader)();
  }

  const ossia::scene_spec scene = loader->outputs.scene_out.scene;
  out.loaderPublished = (scene.state != nullptr);
  describe_cloud(scene, out);
  if(!out.loaderPublished)
  {
    out.error = "AssetLoader published a null scene for the synthesised .ply";
    return out;
  }

  // --- The GPU half. ---
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    const int producer = p.addNode(std::make_unique<AssetSceneNode>(scene));
    const int preproc
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());

    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    // The exact configuration both corpus documents carry: mode 12
    // (format_id == match_str), match 0, "3dgs.classic".
    // FlattenedSceneFilterNode compares g.filter_tag against
    // hash_string(match_str) truncated to 32 bits -- the same key
    // ScenePreprocessorNode stamps on the bucket.
    filterNode->m_mode = 12;
    filterNode->m_match = 0;
    filterNode->m_match_str = "3dgs.classic";
    const int filter = p.addNode(std::move(filterNode));

    const int csf = p.addCsf(csfPath);
    const int raster = p.addRaster(
        corpus("syn-instance-index-color.vs"),
        corpus("syn-instance-index-color.fs"));

    if(producer < 0 || preproc < 0 || filter < 0 || csf < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* sceneOut = p.nodeSceneOut(producer, 0);
    auto* sceneIn = p.nodeSceneIn(preproc, 0);
    auto* preprocGeo = p.nodeGeometryOut(preproc, 0);
    auto* filterGeoIn = p.nodeGeometryIn(filter, 0);
    auto* filterGeoOut = p.nodeGeometryOut(filter, 0);
    auto* csfGeoIn = p.geometryIn(csf, 0);
    auto* csfGeoOut = p.geometryOut(csf, 0);
    auto* rasterGeoIn = p.geometryIn(raster, 0);
    if(!sceneOut || !sceneIn || !preprocGeo || !filterGeoIn || !filterGeoOut
       || !csfGeoIn || !csfGeoOut || !rasterGeoIn)
    {
      out.error = "a port on the splat chain is missing";
      return;
    }

    p.wire(sceneOut, sceneIn);
    p.wire(preprocGeo, filterGeoIn);
    p.wire(filterGeoOut, csfGeoIn);
    p.wire(csfGeoOut, rasterGeoIn);

    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // Frame 1 publishes the scene; frame 2 lets the preprocessor's bucket
    // upload land and the CSF adopt it; frame 3 draws. Two extra frames of
    // margin, as in the instancer / point-cloud twins.
    p.render(5);

    const auto img = p.readback(sink);
    out.imgValid = img.valid();
    if(out.imgValid)
    {
      out.width = img.width;
      out.height = img.height;
      out.px = analyze(img);
      out.ids = identity_map(img);
    }
  });

  return out;
}

} // namespace

// =============================================================================

TEST_CASE(
    "a gaussian splat renders through asset_loader -> Scene Preprocessor -> "
    "Flattened Scene Filter -> CSF -> raster: 240 written, 240 drawn, in file "
    "order",
    "[gfx][threedim][splat][primitivecloud][p1-17]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString dir = tmp.path();

  const std::string plyFwd = write_ply(dir, "splats.ply", kSplats, false);
  const std::string plyRev = write_ply(dir, "splats-rev.ply", kSplats, true);
  const QString csf = write_text(dir, "splat-decode-quads.cs", kDecodeCsf);
  REQUIRE_FALSE(plyFwd.empty());
  REQUIRE_FALSE(plyRev.empty());
  REQUIRE_FALSE(csf.isEmpty());

  const auto fwd = run_chain(api, plyFwd, csf);

  if(fwd.skipped)
    SKIP(fwd.backend + ": " + fwd.skip_reason);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("backend=" << fwd.backend << " error=" << fwd.error);
  REQUIRE(fwd.error.empty());

  // -------------------------------------------------------------------------
  // (a) THE COUNT -- measurement 1: the independent CPU parser.
  // Ply.cpp, from miniply's num_rows() of the `element vertex` line.
  // -------------------------------------------------------------------------
  CHECK(fwd.plySplatCount == uint32_t(kSplats));
  // 64 floats per splat -- corroborates that the count is the row count and
  // not a byte-size accident.
  CHECK(
      fwd.plyBufferFloats
      == std::size_t(kSplats) * std::size_t(Threedim::GaussianSplatData::floatsPerSplat));

  // The live path's own parse, REPORTED (not the oracle): if these are wrong
  // the failure below is attributable to the parser rather than to the graph.
  CHECK(fwd.loaderPublished);
  CHECK(fwd.cloudFormatId == "3dgs.classic");   // PlyParser.hpp fingerprint
  CHECK(fwd.cloudStructType == "Splat3DGS");
  CHECK(fwd.cloudRowStride == uint32_t(kCols * 4)); // 60 B, PrimitiveCloudTest.cpp
  CHECK(fwd.cloudPrimitiveCount == int64_t(kSplats));

  // -------------------------------------------------------------------------
  // (a) THE COUNT -- measurement 2: counted off the framebuffer.
  // One 2x2 block per splat, each block's 4 pixel centres inside its own
  // quad. litBlocks IS the number of instances the draw actually issued.
  // -------------------------------------------------------------------------
  REQUIRE(fwd.imgValid);
  INFO(
      "litBlocks=" << fwd.px.litBlocks << " litPixels=" << fwd.px.litPixels
                   << " expected " << kSplats << " blocks");
  CHECK(fwd.px.litBlocks == kSplats);
  // Every block fully covered: 4 pixels each, no partial quads.
  CHECK(fwd.px.litPixels == 4 * kSplats);

  // ...and the two measurements agree. Stated as its own assertion so a
  // failure names the disagreement rather than one side of it.
  CHECK(int(fwd.plySplatCount) == fwd.px.litBlocks);

  // Each identity appears exactly once: all 240 rows arrived, none twice,
  // none lost, and each landed at the block ITS OWN payload named.
  {
    int missing = 0, duplicated = 0, outOfRange = 0;
    for(int i = 0; i < kSplats; ++i)
    {
      if(fwd.px.idHistogram[std::size_t(i)] == 0)
        ++missing;
      else if(fwd.px.idHistogram[std::size_t(i)] > 1)
        ++duplicated;
    }
    for(int i = kSplats; i < 256; ++i)
      outOfRange += fwd.px.idHistogram[std::size_t(i)];
    INFO(
        "missing=" << missing << " duplicated=" << duplicated
                   << " outOfRange=" << outOfRange);
    CHECK(missing == 0);
    CHECK(duplicated == 0);
    CHECK(outOfRange == 0);
  }

  // -------------------------------------------------------------------------
  // (b) THE FRAME IS NON-UNIFORM.
  // -------------------------------------------------------------------------
  CHECK(fwd.px.litPixels > 0);
  CHECK(fwd.px.sawUnlit); // the 64x64 frame is far larger than 48x20 of blocks

  // -------------------------------------------------------------------------
  // (c1) NO REORDERING, closed form: the row that reaches instance i is row i.
  // G is the row's own payload byte (translation.w, written by the CSF from
  // f_dc_0); B is gl_InstanceIndex/255, written by the raster from the draw
  // call itself. They come from opposite ends of the pipeline.
  //
  // GREEN TODAY, AND A PIN: the live splat path applies no depth ordering
  // (rebuildPrimitiveClouds concatenates rows in file order and nothing
  // downstream in the product reorders them). If a product-side sort
  // ever lands, this goes red -- rewrite the case in sort-on/off form rather
  // than relaxing this.
  // -------------------------------------------------------------------------
  INFO("pixels where the payload id != the draw's instance index: "
       << fwd.px.idBufMismatch);
  CHECK(fwd.px.idBufMismatch == 0);
  // ...and the mirror counter is correspondingly saturated: G + B == 2i, which
  // equals kSplats-1 == 239 for no integer i. Asserting both directions on
  // both fixtures is what makes the pair a real control on each other.
  CHECK(fwd.px.idMirrorMismatch == 4 * kSplats);

  // -------------------------------------------------------------------------
  // (c2) DIFFERENCE ORACLE -- the honest inverse of "disabling the sort
  // changes the frame". Same 240 splats, same positions, ROWS REVERSED.
  // A depth ordering anywhere between the file and the draw would present
  // the same depth set from both files and produce IDENTICAL frames.
  // -------------------------------------------------------------------------
  const auto rev = run_chain(api, plyRev, csf);
  if(rev.skipped)
    SKIP(rev.backend + ": " + rev.skip_reason);
  INFO("reversed run: backend=" << rev.backend << " error=" << rev.error);
  REQUIRE(rev.error.empty());
  REQUIRE(rev.imgValid);

  // The reversed file is the same cloud: same count, same lit set.
  CHECK(rev.plySplatCount == uint32_t(kSplats));
  CHECK(rev.px.litBlocks == kSplats);
  CHECK(rev.px.litPixels == 4 * kSplats);

  // (c1) from the reversed side. Slot i of the reversed file carries identity
  // kSplats-1-i, so a draw that preserves file order must put payload identity
  // kSplats-1-i on instance i: G + B == kSplats-1 on every drawn pixel, and
  // G == B on none of them (kSplats-1 == 239 is odd, so G == B would need
  // i == 119.5). This is the same statement as the forward run's
  // `idBufMismatch == 0` with the fixture inverted, so a stuck or ignored
  // payload cannot satisfy both.
  CHECK(rev.px.idMirrorMismatch == 0);
  CHECK(rev.px.idBufMismatch == 4 * kSplats);
  REQUIRE(rev.width == fwd.width);
  REQUIRE(rev.height == fwd.height);
  REQUIRE(rev.ids.size() == fwd.ids.size());

  // The frames differ...
  CHECK(fwd.ids != rev.ids);

  // ...and they differ EXACTLY as file order predicts: every drawn pixel's
  // identity is mirrored. Comparing the two frames pixel-by-pixel makes this
  // independent of the backend's Y orientation -- both frames carry the same
  // one.
  {
    int compared = 0, mirrored = 0, coverageMismatch = 0;
    for(std::size_t i = 0; i < fwd.ids.size(); ++i)
    {
      const int a = fwd.ids[i];
      const int b = rev.ids[i];
      if((a < 0) != (b < 0))
      {
        ++coverageMismatch; // one frame drew here and the other did not
        continue;
      }
      if(a < 0)
        continue;
      ++compared;
      if(b == kSplats - 1 - a)
        ++mirrored;
    }
    INFO(
        "compared=" << compared << " mirrored=" << mirrored
                    << " coverageMismatch=" << coverageMismatch);
    CHECK(coverageMismatch == 0);
    CHECK(compared == 4 * kSplats);
    CHECK(mirrored == compared);
  }
}
