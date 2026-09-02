// =============================================================================
// P1-20 -- A MATERIAL TEXTURE SWAPPED LIVE CHANGES THE FRAME WITHOUT A GRAPH
// REBUILD: the RENDER half.
//
// (The CPU contract -- a wired texture becomes a source-less DYNAMIC ref on
// the material clone, detected by native-handle swap with no port event,
// stable_id preserved so downstream fingerprints see the same logical
// material -- is already pinned by tests/threedim/MaterialOverrideTest.cpp.
// This file adds the render half: the REAL score::gfx::ScenePreprocessorNode
// fed a scene with one textured material, consumed by a REAL
// RAW_RASTER_PIPELINE draw, on every backend the box offers, with the frame
// read back as pixels. Threedim::MaterialOverride itself is NOT compiled in;
// the harness publishes exactly the scene shape that node emits.)
//
// THE TWO LEGITIMATE PATHS -- the distinction this test exists to assert.
// A live texture change reaches the GPU through one of two engine paths, and
// WHICH one fires per change kind is the whole point (spec P1-20):
//
//  1. PLAIN HANDLE SWAP (dynamic ref: native_handle set, source null; same
//     size/format, same material stable_id). rebuildDynamicSlots
//     (ScenePreprocessorNode.cpp:3044) re-resolves the handle into the
//     registry's per-channel dynamic slot map EVERY frame, explicitly
//     "regardless of sameMaterialsContent" (:3093-3095), while
//     rebuildChannel's fast path (:3102-3105) returns false: NO
//     QRhiTextureArray is reallocated, no channelReallocated (:3939), no
//     auxBuffersChanged (:4094). Registry observable: every BaseColor bucket's
//     `array` pointer and layer count are IDENTICAL across the swap, and the
//     new QRhiTexture lands in a dynamic slot
//     (GpuResourceRegistry.cpp:450 resolveDynamicSlot; a swapped-in handle is
//     a globalResourceId miss, so it appends/reuses a slot: old texture at
//     slot 0, new one deterministically at slot 1, :481-499).
//
//  2. ARRAY-REALLOCATING CHANGE (static refs: `source` set; the materials
//     fingerprint changes and the channel's layer count grows).
//     rebuildChannel (:3081) walks the sources, and the per-bucket
//     allocate/reallocate loop (:3241-3260) does
//     `array->deleteLater(); array = rhi.newTextureArray(...)` when
//     `layers != wantLayers` -- anyReallocated -> arrayReallocated
//     (:3377/:3402) -> channelReallocated (:3939) -> auxBuffersChanged
//     (:4094) -> meshesUnchanged is false (:4497-4515) -> rebuildMDI (:4565)
//     republishes a FRESH meshes vector whose auxiliary_textures
//     (appendTextureAuxes :3522) carry the NEW array; downstream,
//     RenderedRawRasterPipelineNode sees geometryChanged
//     (NodeRenderer.cpp:546-549, shared_ptr identity), re-resolves the aux
//     (RenderedRawRasterPipelineNode.cpp:2513) and recreates its passes
//     (mustRecreatePasses, :2638). Registry observable: bucket 0's `array`
//     POINTER CHANGES and its layer count grows 1 -> 2; pixel observable:
//     the frame now samples the NEW array's layer 0.
//
// mustRecreatePasses itself is private consumer state; the assertable proxy
// for "a rebuild happened" is the QRhiTextureArray reallocation the spec
// names (pointer + layer-count change in the registry), and the assertable
// proxy for "the rebuild took effect" is the pixel: if the consumer had NOT
// recreated its passes after the realloc, it would still be sampling the
// deleteLater()'d old array -- the exact failure the spec's negative control
// describes ("never set mustRecreatePasses -> the array-realloc case renders
// the old texture").
//
// HONEST FINDING, from static analysis while writing this test -- the
// plain-swap FRAME-change leg (case "the frame follows a live handle swap",
// tagged [!mayfail]) may be red on the current engine, and that redness is
// the finding, not a test bug:
//   * meshesUnchanged (:4497-4515) has NO dynamic-slot term: a handle swap
//     with an unchanged materials fingerprint keeps m_outputSpec.meshes'
//     shared_ptr, so the dyn aux entries emitted at the last rebuildMDI
//     (:3568-3583, "baseColorDyn<slot>") still name the OLD QRhiTexture, and
//     downstream never sees geometryChanged (NodeRenderer.cpp:546-549 is
//     shared_ptr identity).
//   * The MaterialGPU arena upload (:3952-3956) is gated on
//     (!sameMaterialsContent || channelReallocated) -- both false for a plain
//     swap -- so the material's tex_ref_dynamic(slot) index change never
//     reaches the GPU either.
// The registry-level "which path" case stays green either way (the slot IS
// rerouted and the arrays ARE untouched); only the on-screen propagation of
// the swap is in question. If [!mayfail] reports a failure there, the fix is
// product-side: give meshesUnchanged (:4497) a dynamic-slot fingerprint term
// so a slot-map change republishes the meshes vector.
//
// NEGATIVE CONTROL (product-side, one line, for the orchestrator):
//   src/plugins/score-plugin-gfx/Gfx/Graph/ScenePreprocessorNode.cpp:3402 --
//   change `return arrayReallocated;` to `return false;`. The realloc still
//   happens (:3250-3251 deleteLater + newTextureArray) but is never reported:
//   channelReallocated (:3939) stays false, auxBuffersChanged (:4094) stays
//   false, meshesUnchanged (:4497) stays true, the meshes vector is not
//   republished, and the consumer keeps the destroyed 1-layer array bound --
//   the array-realloc case renders the old texture (or garbage / a Vulkan
//   validation abort). The "texture-array grow" case's phase-2 pixel and
//   layer assertions go red. That is exactly the spec's control.
//
// Intended registration (tests/gfx/CMakeLists.txt), mirroring the
// test_gfx_dynamic_slot block. The OffsetAllocator include dir is REQUIRED
// (GpuResourceRegistry.hpp includes <offsetAllocator.hpp> at :11); the
// source-file inclusion is the same insurance test_gfx_dynamic_slot carries
// (this TU never constructs or destroys a GpuResourceRegistry -- it only
// reads the one owned by the sink OutputNode via RenderList::registry() -- so
// ~Allocator should not be ODR-used, but the extra TU is harmless and keeps
// the block identical to the proven one):
//
//   score_add_gfx_test(material_texture_swap GfxMaterialTextureSwap.cpp)
//   target_sources(test_gfx_material_texture_swap PRIVATE
//     "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
//   target_include_directories(test_gfx_material_texture_swap PRIVATE
//     "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_material_texture_swap
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_material_texture_swap
// The verdict is pixels + registry state: unavailable backends SKIP, never
// fall back to Null.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64; // frame is 64 x 64

// -----------------------------------------------------------------------------
// Shaders, written by the test into a QTemporaryDir (the synthesised-fixture
// pattern; make_raster_node reads files from disk). Descriptor JSON lives in
// the fragment file, exactly like tests/gfx/corpus/syn-scene-solid.fs.

// Shared vertex stage: NDC passthrough (same as syn-scene-solid.vs, proven
// against the ScenePreprocessor slab path) + a uv derived from position so no
// texcoord attribute is needed. uv depends only on x/y, so the pixel oracle
// is immune to per-backend clip-space Y flips as long as probes sit on the
// middle row and colours vary only along x.
constexpr const char* kVs = R"__(void main()
{
    v_uv = position.xy * 0.5 + 0.5;
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * position;
}
)__";

// Dynamic-slot consumer: four screen quarters sample the four dynamic
// BaseColor slots (kMaxDynamicSlots == 4, GpuResourceRegistry.hpp:299). The
// slots are AUXILIARY textures resolved by name from the geometry's
// auxiliary_textures ("baseColorDyn<slot>", ScenePreprocessorNode.cpp:3568);
// an unpublished slot falls back to the renderer's 1x1 placeholder, whose
// content is undefined -- so the oracle only ever asserts on the quarter
// whose slot is expected to be LIVE, never on placeholder quarters.
constexpr const char* kFsDyn = R"__(/*{
  "DESCRIPTION": "P1-20 dynamic-slot probe: quarter i of the screen samples baseColorDyn[i], so WHERE a colour appears identifies WHICH dynamic slot the registry routed the live texture to, and the colour identifies WHICH QRhiTexture is bound there.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "baseColorDyn0", "TYPE": "image" },
    { "NAME": "baseColorDyn1", "TYPE": "image" },
    { "NAME": "baseColorDyn2", "TYPE": "image" },
    { "NAME": "baseColorDyn3", "TYPE": "image" }
  ]
}*/
void main()
{
    vec3 c;
    if(v_uv.x < 0.25)      c = texture(baseColorDyn0, v_uv).rgb;
    else if(v_uv.x < 0.5)  c = texture(baseColorDyn1, v_uv).rgb;
    else if(v_uv.x < 0.75) c = texture(baseColorDyn2, v_uv).rgb;
    else                   c = texture(baseColorDyn3, v_uv).rgb;
    isf_FragColor = vec4(c, 1.0);
}
)__";

// Bucket-array consumer: samples layer 0 of BaseColor bucket 0
// ("baseColorArray0", the suffixed per-bucket aux from
// ScenePreprocessorNode.cpp:3554; ARRAY:true emits sampler2DArray, see
// libisf isf.cpp parse_auxiliary_texture IS_ARRAY/ARRAY keys).
constexpr const char* kFsArr = R"__(/*{
  "DESCRIPTION": "P1-20 bucket-array probe: samples layer 0 of the BaseColor channel's bucket-0 QRhiTextureArray. If the array is reallocated (layer growth) and the engine propagates it, the on-screen colour follows layer 0 of the NEW array; a consumer left bound to the destroyed old array is the negative-control failure.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "baseColorArray0", "TYPE": "image", "ARRAY": true }
  ]
}*/
void main()
{
    isf_FragColor = vec4(texture(baseColorArray0, vec3(v_uv, 0.0)).rgb, 1.0);
}
)__";

bool writeTextFile(const QString& path, const char* text)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  const QByteArray bytes = QByteArray::fromRawData(text, qstrlen(text));
  return f.write(bytes) == bytes.size();
}

// -----------------------------------------------------------------------------
// Scene fixture: one CPU quad (x,y in [-0.8, 0.8], z = 0) -- the same
// CPU-backed mesh_primitive shape the loaders publish, which the
// preprocessor's slab path uploads. Built ONCE per scenario and shared by
// every published scene_state, so the mesh fingerprint
// (ScenePreprocessorNode.cpp:4389-4413: draw stable_id + per-attribute
// upstream-buffer identity, 0 for CPU-sourced) stays EQUAL across phases --
// which is precisely what routes the plain-swap phase onto the
// materials-fast-path under test.
std::shared_ptr<const std::vector<ossia::scene_node_ptr>> makeQuadRoots()
{
  constexpr float e = 0.8f;
  auto positions = std::make_shared<std::vector<float>>(std::vector<float>{
      -e, -e, 0.f, //
      e,  -e, 0.f, //
      e,  e,  0.f, //
      -e, -e, 0.f, //
      e,  e,  0.f, //
      -e, e,  0.f, //
  });
  auto indices = std::make_shared<std::vector<uint32_t>>(
      std::vector<uint32_t>{0, 1, 2, 3, 4, 5});

  auto posRes = std::make_shared<ossia::buffer_resource>();
  {
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(positions, positions->data());
    bd.byte_size = int64_t(positions->size() * sizeof(float));
    bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
    posRes->resource = bd;
    posRes->dirty_index = 1;
  }
  auto idxRes = std::make_shared<ossia::buffer_resource>();
  {
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(indices, indices->data());
    bd.byte_size = int64_t(indices->size() * sizeof(uint32_t));
    bd.usage_hint = ossia::buffer_data::usage::index_buffer;
    idxRes->resource = bd;
    idxRes->dirty_index = 1;
  }

  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(posRes);
  prim.index_buffer = idxRes;
  {
    ossia::vertex_attribute a;
    a.semantic = ossia::attribute_semantic::position;
    a.format = ossia::vertex_format::float3;
    a.buffer_index = 0;
    a.byte_offset = 0;
    a.byte_stride = 12;
    a.rate = ossia::vertex_attribute::input_rate::per_vertex;
    prim.attributes.push_back(a);
  }
  prim.topology = ossia::primitive_topology::triangles;
  prim.index_type = ossia::index_format::uint32;
  prim.vertex_count = 6;
  prim.index_count = 6;
  prim.bounds = ossia::compute_aabb_from_positions(positions->data(), 6);
  prim.stable_id = 0x9120A11Bu; // nonzero: the mesh fingerprint requires it
  // prim.material stays null on purpose: the shaders under test sample the
  // channel aux textures directly, and a null material draws fine (the
  // instancer prototype path does the same). Keeping the prim
  // material-agnostic lets the SAME mesh shared_ptr serve every phase's
  // materials list without a rebuild of the mesh itself.

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::move(children);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(root));
  return roots;
}

std::shared_ptr<ossia::scene_state> makeState(
    std::shared_ptr<const std::vector<ossia::scene_node_ptr>> roots,
    std::vector<ossia::material_component_ptr> mats, int64_t version)
{
  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  auto mv = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::move(mats));
  st->materials = std::move(mv);
  st->version = version;
  st->dirty_index = version;
  return st;
}

// A source-less DYNAMIC base-color ref: native handle set, source null --
// the exact clone shape MaterialOverrideTest.cpp pins on the CPU side
// ("source reset -> ScenePreprocessor's channelDynamicHandle() sees
// DYNAMIC"). stable_id is preserved across the swap so the materials
// fingerprint (stable_id-keyed, ScenePreprocessorNode.cpp:3012-3029) stays
// equal -- routing the swap onto the fast path, as the real MaterialOverride
// clone does ("identity metadata inherited so downstream fingerprints see
// the same logical material").
ossia::material_component_ptr makeDynMaterial(QRhiTexture* t, uint64_t id)
{
  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = id;
  m->tag = "p120-dyn";
  m->base_color_texture.texture.native_handle = t;
  m->base_color_texture.texture.bindless_index = 0;
  m->base_color_texture.source = nullptr;
  return m;
}

// A STATIC base-color ref: an in-memory PNG (embedded_data + mime_type,
// decoded by decodeTextureSource, ScenePreprocessorNode.cpp:2969-2986;
// content_hash 0 -> always decoded, no AssetTable coupling). Sized EXACTLY
// kTextureLayerSize^2 (1024, GpuResourceRegistry.hpp:294) with the default
// sampler config so it lands in the SAME bucket the preprocessor
// pre-allocates at init (:806-846) -- making the phase-2 realloc a pure
// layer-count growth of bucket 0 rather than a new-bucket creation, which
// keeps the consumer's "baseColorArray0" name stable across both phases.
ossia::material_component_ptr makeStaticMaterial(QColor fill, uint64_t id)
{
  QImage img(
      score::gfx::GpuResourceRegistry::kTextureLayerSize,
      score::gfx::GpuResourceRegistry::kTextureLayerSize,
      QImage::Format_RGBA8888);
  img.fill(fill);
  QByteArray png;
  {
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
  }
  auto bytes = std::make_shared<std::vector<uint8_t>>(
      reinterpret_cast<const uint8_t*>(png.constData()),
      reinterpret_cast<const uint8_t*>(png.constData()) + png.size());

  auto src = std::make_shared<ossia::texture_source>();
  src->embedded_data = std::move(bytes);
  src->mime_type = "image/png";
  src->content_hash = 0;

  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = id;
  m->tag = "p120-static";
  m->base_color_texture.source = std::move(src);
  return m;
}

// -----------------------------------------------------------------------------
// Registry snapshot of the BaseColor channel, taken on the render thread by
// the harness renderer each frame and read from the (same-thread,
// synchronous offscreen fixture) test body between render() calls. Raw
// pointers are captured for IDENTITY comparison only, never dereferenced.
struct ChannelSnap
{
  bool taken = false;
  std::vector<QRhiTexture*> bucketArrays; // per bucket: array pointer
  std::vector<int> bucketLayers;          // per bucket: layer count
  std::vector<QRhiTexture*> dyn;          // dynamic slot -> texture
};

void takeSnap(score::gfx::RenderList& r, ChannelSnap& out)
{
  auto& reg = r.registry();
  const auto& ch = reg.textureChannel(
      score::gfx::GpuResourceRegistry::TextureChannel::BaseColor);
  ChannelSnap s;
  s.taken = true;
  s.bucketArrays.reserve(ch.buckets.size());
  s.bucketLayers.reserve(ch.buckets.size());
  for(const auto& b : ch.buckets)
  {
    s.bucketArrays.push_back(b.array);
    s.bucketLayers.push_back(b.layers);
  }
  s.dyn = ch.dynamicTextures;
  out = std::move(s);
}

// -----------------------------------------------------------------------------
// The harness node: a data-only score::gfx scene producer (the
// GfxInstancerShrink pattern), publishing its scene_state to the downstream
// ScenePreprocessor exactly as every real scene producer does --
// NodeRenderer::process(port, scene_spec, edge.source) from
// runInitialPasses, every frame; the preprocessor short-circuits on state
// pointer + version (ScenePreprocessorNode.cpp:3760-3764).

struct MatSwapNode final : score::gfx::ProcessNode
{
  enum class Mode
  {
    DynamicSwap,
    StaticGrow
  };
  Mode mode = Mode::DynamicSwap;

  std::shared_ptr<const std::vector<ossia::scene_node_ptr>> roots
      = makeQuadRoots();

  // DynamicSwap: the test flips requestedPhase between render() calls; the
  // renderer creates the textures (it owns the QRhi) and republishes.
  std::atomic<int> requestedPhase{1};
  std::atomic<int> appliedPhase{0};
  mutable QRhiTexture* texA{};
  mutable QRhiTexture* texB{};

  // StaticGrow: states are pure CPU data; the test swaps the pending state
  // directly between render() calls (single-threaded offscreen fixture).
  std::shared_ptr<ossia::scene_state> pendingState;

  // Written by the renderer every frame, read by the test between frames.
  mutable ChannelSnap snap;

  MatSwapNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~MatSwapNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct MatSwapRenderer final : score::gfx::NodeRenderer
{
  MatSwapNode& self;
  ossia::scene_spec m_scene;

  explicit MatSwapRenderer(const MatSwapNode& n)
      : NodeRenderer{n}
      , self{const_cast<MatSwapNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }

  static QRhiTexture*
  makeFilledTexture(QRhi& rhi, QRhiResourceUpdateBatch& res, QColor fill,
                    const char* name)
  {
    // Plain RGBA8 (no sRGB flag): the dynamic path binds the producer's own
    // texture verbatim, so what we upload is what the shader samples.
    auto* t = rhi.newTexture(QRhiTexture::RGBA8, QSize{16, 16}, 1);
    t->setName(name);
    t->create();
    QImage img(16, 16, QImage::Format_RGBA8888);
    img.fill(fill);
    QRhiTextureSubresourceUploadDescription sub(img);
    QRhiTextureUploadEntry entry(0, 0, sub);
    res.uploadTexture(t, QRhiTextureUploadDescription({entry}));
    return t;
  }

  void applyDynPhase(
      int phase, score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
  {
    auto& rhi = *r.state.rhi;
    if(phase == 1)
    {
      // Phase 1: the material carries texA (RED). First arrival: the
      // materials fingerprint changes ([] -> [0xA1]) so the full
      // rebuildChannel path runs once, rebuildMDI publishes the meshes with
      // aux "baseColorDyn0" -> texA, and quarter 0 turns red.
      self.texA = makeFilledTexture(
          rhi, res, QColor(255, 0, 0, 255), "P120::texA_red");
      m_scene.state = makeState(
          self.roots, {makeDynMaterial(self.texA, 0xA1)}, /*version=*/1);
    }
    else
    {
      // Phase 2: THE PLAIN HANDLE SWAP. A brand-new QRhiTexture (GREEN),
      // same 16x16 RGBA8 size/format, on a material clone with the SAME
      // stable_id -- exactly what MaterialOverride republishes on a wired
      // texture change (no port event; dirty republish only). texA is kept
      // alive: the guard is about routing, and the current engine may keep
      // texA bound downstream (see the header) -- destroying it here would
      // convert a routing bug into a UAF and muddy the verdict.
      self.texB = makeFilledTexture(
          rhi, res, QColor(0, 255, 0, 255), "P120::texB_green");
      m_scene.state = makeState(
          self.roots, {makeDynMaterial(self.texB, 0xA1)}, /*version=*/2);
    }
    self.appliedPhase.store(phase);
  }

  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*) override
  {
    if(self.mode == MatSwapNode::Mode::DynamicSwap)
    {
      const int want = self.requestedPhase.load();
      if(want != self.appliedPhase.load())
        applyDynPhase(want, r, res);
    }
    else
    {
      if(self.pendingState && m_scene.state != self.pendingState)
        m_scene.state = self.pendingState;
    }

    // Registry snapshot AFTER the phase logic: reflects the preprocessor's
    // state as of the END of the previous frame (this producer updates
    // before the preprocessor in topological order), which is settled by the
    // time the test reads it -- each phase renders several frames.
    takeSnap(r, self.snap);
  }

  // Publish to the downstream sink exactly as MergeGeometriesNode /
  // scene_outputs_storage do -- every frame; consumers short-circuit on
  // state pointer identity + version.
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!m_scene.state)
      return;
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it
        = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, m_scene, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    delete self.texA;
    self.texA = nullptr;
    delete self.texB;
    self.texB = nullptr;
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
MatSwapNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new MatSwapRenderer{*this};
}

// -----------------------------------------------------------------------------
// Pixel probes. Quad covers px [6.4, 57.6] in both axes; probes sit on the
// middle row, inside the quad, at the CENTER of a screen quarter (quarter
// boundaries at px 16/32/48 for kSize 64).
std::array<uint8_t, 4> probe(const ReadbackImage& img, int x)
{
  return img.at(x, img.height / 2);
}
constexpr int kQ0x = 10; // uv.x ~ 0.16 -> baseColorDyn0
constexpr int kQ1x = 24; // uv.x ~ 0.38 -> baseColorDyn1
constexpr int kMidx = 32;

constexpr std::array<uint8_t, 4> kRed{255, 0, 0, 255};
constexpr std::array<uint8_t, 4> kGreen{0, 255, 0, 255};
constexpr std::array<uint8_t, 4> kBlue{0, 0, 255, 255};
constexpr std::array<uint8_t, 4> kYellow{255, 255, 0, 255};
constexpr int kTol = 40;

// -----------------------------------------------------------------------------
// Scenario A: the plain dynamic-handle swap.
struct DynOutcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  int appliedPhase = 0;

  ChannelSnap snap1, snap2;
  QRhiTexture* texA{};
  QRhiTexture* texB{};

  bool valid1 = false, valid2 = false;
  std::array<uint8_t, 4> q0p1{}, q1p1{}, q0p2{}, q1p2{};
  bool framesDiffer = false;
};

DynOutcome run_dynamic_swap(score::gfx::GraphicsApi api)
{
  DynOutcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    QTemporaryDir tmp;
    if(!tmp.isValid())
    {
      out.error = "cannot create temporary shader dir";
      return;
    }
    const QString vsPath = tmp.filePath("p120-dyn.vs");
    const QString fsPath = tmp.filePath("p120-dyn.fs");
    if(!writeTextFile(vsPath, kVs) || !writeTextFile(fsPath, kFsDyn))
    {
      out.error = "cannot write shader fixtures";
      return;
    }

    GfxPipeline p;
    auto harness_uptr = std::make_unique<MatSwapNode>();
    harness_uptr->mode = MatSwapNode::Mode::DynamicSwap;
    auto* harness = harness_uptr.get();

    const int hn = p.addNode(std::move(harness_uptr));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vsPath, fsPath);
    if(hn < 0 || flat < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* sceneOut = p.nodeSceneOut(hn, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!sceneOut || !flatIn || !flatOut)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(sceneOut, flatIn);
    p.wire(flatOut, p.geometryIn(raster, 0));
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

    // ---- Phase 1: dynamic material bound to texA (red). ----
    p.render(5);
    out.snap1 = harness->snap;
    const auto img1 = p.readback(sink);
    out.valid1 = img1.width == kSize && img1.height == kSize
                 && !img1.bytes.isEmpty();
    if(out.valid1)
    {
      out.q0p1 = probe(img1, kQ0x);
      out.q1p1 = probe(img1, kQ1x);
    }

    // ---- Phase 2: swap the handle to texB (green), same size/format,
    //      same material stable_id -- the fast-path change kind. ----
    harness->requestedPhase.store(2);
    p.render(6);
    out.appliedPhase = harness->appliedPhase.load();
    out.snap2 = harness->snap;
    out.texA = harness->texA;
    out.texB = harness->texB;
    const auto img2 = p.readback(sink);
    out.valid2 = img2.width == kSize && img2.height == kSize
                 && !img2.bytes.isEmpty();
    if(out.valid2)
    {
      out.q0p2 = probe(img2, kQ0x);
      out.q1p2 = probe(img2, kQ1x);
    }
    out.framesDiffer = img1.bytes != img2.bytes;
  });
  return out;
}

// -----------------------------------------------------------------------------
// Scenario B: the array-reallocating change (static sources, layer growth).
struct ArrOutcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  ChannelSnap snap1, snap2;

  bool valid1 = false, valid2 = false;
  std::array<uint8_t, 4> mid1{}, mid2{};
  bool framesDiffer = false;
};

ArrOutcome run_array_grow(score::gfx::GraphicsApi api)
{
  ArrOutcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    QTemporaryDir tmp;
    if(!tmp.isValid())
    {
      out.error = "cannot create temporary shader dir";
      return;
    }
    const QString vsPath = tmp.filePath("p120-arr.vs");
    const QString fsPath = tmp.filePath("p120-arr.fs");
    if(!writeTextFile(vsPath, kVs) || !writeTextFile(fsPath, kFsArr))
    {
      out.error = "cannot write shader fixtures";
      return;
    }

    GfxPipeline p;
    auto harness_uptr = std::make_unique<MatSwapNode>();
    harness_uptr->mode = MatSwapNode::Mode::StaticGrow;
    auto* harness = harness_uptr.get();
    const auto roots = harness->roots;

    // Phase 1: ONE static source (BLUE) -> layer 0 of bucket 0 (which the
    // preprocessor pre-allocated at init with 1 white layer,
    // ScenePreprocessorNode.cpp:806-846); wantLayers stays 1, so the phase-1
    // arrival uploads INTO the existing array without reallocating it.
    //
    // Phase 2: TWO materials with two DISTINCT same-size sources; the
    // materials fingerprint changes and wantLayers becomes 2 != 1 -> the
    // :3241-3260 loop deleteLater()s the old array and newTextureArray()s a
    // 2-layer replacement -- the legitimate rebuild path. The DRAWN colour
    // is layer 0 = the FIRST material's source (YELLOW, walk order =
    // materials order); the magenta spare exists only to force the growth.
    //
    // Colour choice: BaseColor arrays carry QRhiTexture::sRGB
    // (GpuResourceRegistry.cpp textureChannelFlags), so only pure-0/1
    // channels are used -- invariant under sRGB decode.
    auto st1 = makeState(
        roots, {makeStaticMaterial(QColor(0, 0, 255, 255), 0xB1)},
        /*version=*/1);
    auto st2 = makeState(
        roots,
        {makeStaticMaterial(QColor(255, 255, 0, 255), 0xB2),
         makeStaticMaterial(QColor(255, 0, 255, 255), 0xB3)},
        /*version=*/2);

    const int hn = p.addNode(std::move(harness_uptr));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vsPath, fsPath);
    if(hn < 0 || flat < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* sceneOut = p.nodeSceneOut(hn, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!sceneOut || !flatIn || !flatOut)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(sceneOut, flatIn);
    p.wire(flatOut, p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    harness->pendingState = st1;

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // ---- Phase 1: one blue source in bucket 0, layer 0. ----
    p.render(5);
    out.snap1 = harness->snap;
    const auto img1 = p.readback(sink);
    out.valid1 = img1.width == kSize && img1.height == kSize
                 && !img1.bytes.isEmpty();
    if(out.valid1)
      out.mid1 = probe(img1, kMidx);

    // ---- Phase 2: layer growth 1 -> 2 forces the array realloc. ----
    harness->pendingState = st2;
    p.render(6);
    out.snap2 = harness->snap;
    const auto img2 = p.readback(sink);
    out.valid2 = img2.width == kSize && img2.height == kSize
                 && !img2.bytes.isEmpty();
    if(out.valid2)
      out.mid2 = probe(img2, kMidx);
    out.framesDiffer = img1.bytes != img2.bytes;
  });
  return out;
}

std::string rgba(std::array<uint8_t, 4> c)
{
  return "(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
         + std::to_string(c[2]) + "," + std::to_string(c[3]) + ")";
}

} // namespace

// =============================================================================
// Case 1 (change kind: plain handle swap) -- WHICH PATH: the no-rebuild one.
// The registry reroutes the dynamic slot; NO QRhiTextureArray is touched.
// All-green expected: every assertion here is engine behaviour verified in
// rebuildDynamicSlots / resolveDynamicSlot / the rebuildChannel fast path.
TEST_CASE(
    "P1-20: a live material texture handle swap reroutes the dynamic slot "
    "and reallocates NO texture array",
    "[gfx][scene][material][dynamic-slot][p1-20]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run_dynamic_swap(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.appliedPhase == 2);
  REQUIRE(r.snap1.taken);
  REQUIRE(r.snap2.taken);
  REQUIRE(r.valid1);
  REQUIRE(r.valid2);
  REQUIRE(r.texA != nullptr);
  REQUIRE(r.texB != nullptr);

  // Phase 1 renders at all: the initial full rebuild published the dyn-0 aux
  // and quarter 0 shows texA's red. (This also proves the dynamic-material
  // scene path works end-to-end before the swap is judged.)
  INFO("phase1 q0=" << rgba(r.q0p1) << " q1=" << rgba(r.q1p1));
  CHECK(near(r.q0p1, kRed, kTol));

  // Registry, phase 1: exactly one dynamic BaseColor slot, holding texA.
  REQUIRE(r.snap1.dyn.size() == 1);
  CHECK(r.snap1.dyn[0] == r.texA);

  // --- WHICH PATH the swap took, part 1: the registry rerouted the slot.
  // resolveDynamicSlot misses on texB's fresh globalResourceId while slot 0
  // is still stamped live at resolve time, so texB deterministically lands
  // at slot 1 (GpuResourceRegistry.cpp:481-499). Slot 0 is either swept to
  // null (sweepStaleDynamicTextureSlots via sweepMeshSlabs) or still holds
  // texA when no sweep ran since -- both are legal; what may NEVER happen is
  // texB overwriting slot 0 in place or a bucket array being touched.
  REQUIRE(r.snap2.dyn.size() >= 2);
  CHECK(r.snap2.dyn[1] == r.texB);
  CHECK((r.snap2.dyn[0] == nullptr || r.snap2.dyn[0] == r.texA));

  // --- WHICH PATH, part 2: NO rebuild. Every BaseColor bucket's
  // QRhiTextureArray pointer and layer count are identical across the swap
  // (the rebuildChannel fast path at ScenePreprocessorNode.cpp:3102-3105
  // returned false; the :3241-3260 realloc loop never ran).
  REQUIRE(!r.snap1.bucketArrays.empty());
  CHECK(r.snap2.bucketArrays == r.snap1.bucketArrays);
  CHECK(r.snap2.bucketLayers == r.snap1.bucketLayers);
}

// =============================================================================
// Case 2 (change kind: plain handle swap) -- the FRAME-follows-the-handle
// contract from the spec ("the frame changes when the handle changes").
// Static analysis (see the file header) suggested the current engine might
// never propagate a dynamic-slot-only change to the drawn frame
// (meshesUnchanged has no dynamic-slot term; the MaterialGPU upload is
// fingerprint-gated). MEASURED by the orchestrator: it passes
// deterministically on OpenGL and Vulkan -- the per-frame dyn refresh
// (ScenePreprocessorNode.cpp:3093-3095) is sufficient in this scenario.
// The case therefore GATES (the authored [!mayfail] was removed after the
// measurement: a passing mayfail would hide a future regression of exactly
// this contract).
TEST_CASE(
    "P1-20: the frame follows a live handle swap without a rebuild",
    "[gfx][scene][material][dynamic-slot][p1-20]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run_dynamic_swap(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.appliedPhase == 2);
  REQUIRE(r.valid1);
  REQUIRE(r.valid2);

  INFO(
      "phase1 q0=" << rgba(r.q0p1) << " q1=" << rgba(r.q1p1) << " | phase2 q0="
                   << rgba(r.q0p2) << " q1=" << rgba(r.q1p2)
                   << " | framesDiffer=" << r.framesDiffer);
  INFO(
      "If q1 of phase 2 is not green: the swap was rerouted in the registry "
      "(case 1 proves it) but never republished -- meshesUnchanged "
      "(ScenePreprocessorNode.cpp:4497) lacks a dynamic-slot term, so the "
      "baseColorDyn aux entries and the MaterialGPU slot index stay stale.");

  // Sanity: phase 1 drew texA's red in quarter 0.
  CHECK(near(r.q0p1, kRed, kTol));
  // THE CONTRACT: after the swap, the live slot's quarter shows texB's
  // green -- the frame changed with the handle, with no rebuild (case 1
  // asserts the no-rebuild half on the same run shape).
  CHECK(near(r.q1p2, kGreen, kTol));
  CHECK(r.framesDiffer);
}

// =============================================================================
// Case 3 (change kind: layer growth of a static channel) -- WHICH PATH: the
// legitimate rebuild. The QRhiTextureArray IS reallocated (pointer changes,
// layers 1 -> 2) and the drawn frame follows the NEW array -- which is the
// observable proof that the realloc was reported downstream and the consumer
// recreated/rebound its passes. This case is the one the product-side
// negative control (ScenePreprocessorNode.cpp:3402 -> `return false;`) must
// turn red: the realloc would still happen, but the frame would keep
// sampling the destroyed old array.
TEST_CASE(
    "P1-20: a material texture change that grows the channel array "
    "reallocates it and the frame samples the new array",
    "[gfx][scene][material][texture-array][p1-20]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run_array_grow(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.snap1.taken);
  REQUIRE(r.snap2.taken);
  REQUIRE(r.valid1);
  REQUIRE(r.valid2);

  INFO("phase1 mid=" << rgba(r.mid1) << " phase2 mid=" << rgba(r.mid2));

  // Phase 1: the single blue source landed in bucket 0 layer 0 and is on
  // screen (the init-fallback white layer was overwritten in place --
  // 1 layer wanted, 1 layer present, no realloc needed for the upload).
  CHECK(near(r.mid1, kBlue, kTol));
  REQUIRE(!r.snap1.bucketArrays.empty());
  REQUIRE(!r.snap2.bucketArrays.empty());
  CHECK(r.snap1.bucketLayers[0] == 1);

  // --- WHICH PATH the change took: the rebuild. Layer growth is the
  // decisive realloc evidence (a 2-layer array can only come from
  // newTextureArray at ScenePreprocessorNode.cpp:3251); the pointer change
  // corroborates it (old and new array coexist within the realloc frame --
  // deleteLater defers destruction -- so the addresses cannot alias).
  CHECK(r.snap2.bucketLayers[0] == 2);
  CHECK(r.snap2.bucketArrays[0] != r.snap1.bucketArrays[0]);
  // Same bucket count: this change kind grows bucket 0 in place, it does not
  // create a second bucket (both sources are kTextureLayerSize/RGBA8/default
  // sampler -- the same (format, size, sampler) tuple).
  CHECK(r.snap2.bucketArrays.size() == r.snap1.bucketArrays.size());

  // --- The rebuild TOOK EFFECT downstream: layer 0 of the NEW array (the
  // first material's yellow) is on screen. A consumer left bound to the
  // deleteLater()'d old array -- the negative-control failure mode -- keeps
  // blue (or garbage / a validation abort) here.
  CHECK(near(r.mid2, kYellow, kTol));
  CHECK(r.framesDiffer);
}
