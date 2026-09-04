// =============================================================================
// A24(a) -- THE UNIFIED-MDI GEOMETRY MUST FIT QT'S D3D11 VERTEX-BINDING CAP.
//
// QD3D11CommandBuffer::MAX_VERTEX_BUFFER_BINDING_COUNT is 8 (qrhid3d11_p.h:411)
// and QRhiD3D11::setVertexInput warns then CLAMPS to it (qrhid3d11.cpp:1234-
// 1237): a ninth vertex buffer is not recorded, the D3D11 input slot stays
// unbound, and every attribute on it reads zeroes. Nothing throws and nothing
// is skipped -- the draw is simply wrong.
//
// ScenePreprocessorNode's unified-MDI geometry used to publish nine bindings:
// six per-vertex streams (position, normal, uv0, tangent, color0, uv1) plus
// three per-instance ones (translation, instance_color0, instance_draw_id).
// remapPipelineVertexInputs (Utils.cpp:831-848) compacts that layout down to
// the bindings an attribute actually lands on, which is why most shaders stay
// well under the cap -- but a shader reading every stream lands on all nine and
// the compaction has nothing left to drop. That is the case the helmets
// document hits, and the case this file pins.
//
// The fix is on the producer: translation and instance_color0 are both
// per-instance vec4s stepping at rate 1, so they share ONE 32-byte binding
// (translation at byte 0, color at byte 16) instead of one binding each.
// Eight bindings, and a shader reading everything fits.
//
// WHY THIS TEST IS MEANINGFUL WITHOUT A D3D11 DEVICE. There is no Direct3D in
// this environment, so the clamp itself cannot be observed here. What CAN be
// observed, on every backend, is the property whose violation causes it: the
// count of vertex bindings the real ScenePreprocessorNode publishes. That count
// is backend-independent -- it is computed in rebuildMDI, not in the RHI -- so
// asserting it here is asserting exactly the thing Windows would otherwise
// silently truncate. Revert the interleave and this file goes red on Vulkan,
// OpenGL and Metal alike.
//
// TWO ORACLES, NEITHER OF WHICH A BLANK FRAME PASSES.
//
//  1. LAYOUT. A pass-through spy node sits between the preprocessor and the
//     raster consumer and reads the geometry_spec the preprocessor actually
//     published (NodeRenderer::geometry). It asserts the binding count is <= 8,
//     that translation and instance_color0 share a binding at offsets 0 and 16
//     of a 32-byte per-instance stride, and that instance_draw_id keeps its own
//     4-byte binding. The count assertion is the D3D11 one; the offsets are
//     what makes the packing readable rather than merely small.
//
//  2. PIXELS. syn-scene-inst-packed reads BOTH halves of that slot. The
//     preprocessor fills a regular (non-instance-group) slot with translation
//     (0,0,0,0) and color (1,1,1,1), so the two halves hold different values
//     and swapping them is visible:
//       * color read at offset 0 instead of 16 -> (0,0,0) -> the quad is drawn
//         but black, indistinguishable from background -> the lit-pixel count
//         collapses.
//       * translation read at offset 16 instead of 0 -> (1,1,1) -> the quad
//         shifts a full NDC unit right and up, entirely off screen -> the
//         lit-pixel count collapses.
//     The test requires a white quad of the RIGHT SIZE in the MIDDLE of the
//     frame with black corners, so a uniformly white frame fails just as a
//     blank one does.
//
// The scene the preprocessor is fed is deliberately the simplest thing that
// makes it allocate the per-instance concat arrays: one CPU-backed quad, one
// draw, slot_cursor == 1. No Instancer is needed -- rebuildMDI publishes the
// per-instance bindings for any scene with at least one slot, which is exactly
// why the ninth binding was there for every scene document and not only the
// instanced ones.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

constexpr int kSize = 64;
// Half-width of the quad in NDC: it covers x,y in [-0.5, 0.5], i.e. the middle
// 32x32 of a 64x64 frame. Small enough that the corners stay background and
// large enough that a one-NDC-unit displacement leaves the frame entirely.
constexpr float kHalf = 0.5f;

// One CPU-backed quad, the shape glTF / OBJ loaders publish and the
// preprocessor's slab path uploads (ScenePreprocessorNode.cpp:1889
// extractCpuAttribute<12>(position)).
std::shared_ptr<ossia::scene_state> makeQuadScene()
{
  auto positions = std::make_shared<std::vector<float>>(std::vector<float>{
      -kHalf, -kHalf, 0.f, //
      kHalf,  -kHalf, 0.f, //
      kHalf,  kHalf,  0.f, //
      -kHalf, -kHalf, 0.f, //
      kHalf,  kHalf,  0.f, //
      -kHalf, kHalf,  0.f, //
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
  prim.stable_id = 0xB1D19AC4u;

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

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

// --- The scene producer ------------------------------------------------------
// Data-only score::gfx producer publishing a fixed scene_spec the way every
// scene producer does (NodeRenderer::process(port, scene_spec, edge.source)).

struct QuadSceneNode final : score::gfx::ProcessNode
{
  std::shared_ptr<ossia::scene_state> state = makeQuadScene();

  QuadSceneNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~QuadSceneNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct QuadSceneRenderer final : score::gfx::NodeRenderer
{
  QuadSceneNode& self;
  ossia::scene_spec m_scene;

  explicit QuadSceneRenderer(const QuadSceneNode& n)
      : NodeRenderer{n}
      , self{const_cast<QuadSceneNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_scene.state = self.state;
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
  }

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
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, m_scene, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_scene = {}; }
};

score::gfx::NodeRenderer*
QuadSceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new QuadSceneRenderer{*this};
}

// --- The layout spy ----------------------------------------------------------
// A geometry pass-through that records the vertex-input layout of the
// geometry_spec the preprocessor published, then forwards it unchanged so the
// raster consumer downstream still draws the real thing.

struct GeometryLayout
{
  bool captured = false;
  int bindings = 0;
  int attributes = 0;
  int inputs = 0;

  int translation_binding = -1;
  int translation_offset = -1;
  int translation_format = -1;
  int color_binding = -1;
  int color_offset = -1;
  int color_format = -1;
  int draw_id_binding = -1;
  int draw_id_offset = -1;

  uint32_t packed_stride = 0;
  int packed_classification = -1;
  int packed_step_rate = -1;
  uint32_t draw_id_stride = 0;
};

struct LayoutSpyNode final : score::gfx::ProcessNode
{
  mutable GeometryLayout layout;

  LayoutSpyNode()
  {
    input.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  ~LayoutSpyNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct LayoutSpyRenderer final : score::gfx::NodeRenderer
{
  LayoutSpyNode& self;
  ossia::geometry_spec m_outputSpec;

  explicit LayoutSpyRenderer(const LayoutSpyNode& n)
      : NodeRenderer{n}
      , self{const_cast<LayoutSpyNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
    m_outputSpec = this->geometry;
    if(!m_outputSpec.meshes || m_outputSpec.meshes->meshes.empty())
      return;

    const auto& g = m_outputSpec.meshes->meshes[0];
    auto& L = self.layout;
    L.captured = true;
    L.bindings = (int)g.bindings.size();
    L.attributes = (int)g.attributes.size();
    L.inputs = (int)g.input.size();

    for(const auto& a : g.attributes)
    {
      switch(a.semantic)
      {
        case ossia::attribute_semantic::translation:
          L.translation_binding = a.binding;
          L.translation_offset = (int)a.byte_offset;
          L.translation_format = (int)a.format;
          break;
        case ossia::attribute_semantic::instance_color0:
          L.color_binding = a.binding;
          L.color_offset = (int)a.byte_offset;
          L.color_format = (int)a.format;
          break;
        case ossia::attribute_semantic::instance_draw_id:
          L.draw_id_binding = a.binding;
          L.draw_id_offset = (int)a.byte_offset;
          break;
        default:
          break;
      }
    }
    if(L.translation_binding >= 0 && L.translation_binding < L.bindings)
    {
      const auto& b = g.bindings[L.translation_binding];
      L.packed_stride = b.byte_stride;
      L.packed_classification = (int)b.classification;
      L.packed_step_rate = b.step_rate;
    }
    if(L.draw_id_binding >= 0 && L.draw_id_binding < L.bindings)
      L.draw_id_stride = g.bindings[L.draw_id_binding].byte_stride;
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!m_outputSpec.meshes)
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
    rn_it->second->process(port_idx, m_outputSpec, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_outputSpec = {}; }
};

score::gfx::NodeRenderer*
LayoutSpyNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new LayoutSpyRenderer{*this};
}

// --- Pixel analysis ----------------------------------------------------------

struct Pixels
{
  int white = 0;      // near-(255,255,255)
  int nonBlack = 0;   // anything lit at all
  int centreR = -1, centreG = -1, centreB = -1;
  int cornerLit = 0;  // lit pixels in the four 8x8 corners
};

Pixels analyze(const ReadbackImage& img)
{
  Pixels p;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto c = img.at(x, y);
      const bool lit = int(c[0]) + int(c[1]) + int(c[2]) > 24;
      if(lit)
        ++p.nonBlack;
      if(c[0] > 200 && c[1] > 200 && c[2] > 200)
        ++p.white;
      const bool corner
          = (x < 8 || x >= img.width - 8) && (y < 8 || y >= img.height - 8);
      if(corner && lit)
        ++p.cornerLit;
    }
  const auto c = img.center();
  p.centreR = c[0];
  p.centreG = c[1];
  p.centreB = c[2];
  return p;
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  GeometryLayout layout;
  bool valid = false;
  Pixels px{};
};

Outcome run_case(score::gfx::GraphicsApi api)
{
  Outcome out;
  out.backend = backend_name(api);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    const int src = p.addNode(std::make_unique<QuadSceneNode>());
    const int pre
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    auto spy_uptr = std::make_unique<LayoutSpyNode>();
    auto* spy_node = spy_uptr.get();
    const int spy = p.addNode(std::move(spy_uptr));
    const int raster = p.addRaster(
        corpus("syn-scene-inst-packed.vs"), corpus("syn-scene-inst-packed.fs"));
    if(src < 0 || pre < 0 || spy < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    p.wire(p.nodeSceneOut(src, 0), p.nodeSceneIn(pre, 0));
    p.wire(p.nodeGeometryOut(pre, 0), p.nodeGeometryIn(spy, 0));
    p.wire(p.nodeGeometryOut(spy, 0), p.geometryIn(raster, 0));
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

    p.render(4);
    out.layout = spy_node->layout;

    const auto img = p.readback(sink);
    out.valid = img.valid();
    if(out.valid)
      out.px = analyze(img);
  });
  return out;
}

} // namespace

TEST_CASE(
    "ScenePreprocessor's unified-MDI geometry fits Qt's 8-vertex-binding D3D11 "
    "cap: translation and instance_color0 share one interleaved per-instance "
    "binding",
    "[gfx][scene][instancing][d3d11][a24]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  const auto r = run_case(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());

  // ---- Oracle 1: the layout the preprocessor published. ----
  REQUIRE(r.layout.captured);
  INFO(
      "bindings=" << r.layout.bindings << " attributes=" << r.layout.attributes
                  << " inputs=" << r.layout.inputs);

  // The per-instance streams must be there at all: without them there is
  // nothing to pack and the count below would pass vacuously.
  REQUIRE(r.layout.translation_binding >= 0);
  REQUIRE(r.layout.color_binding >= 0);
  REQUIRE(r.layout.draw_id_binding >= 0);

  // THE D3D11 ASSERTION. QD3D11CommandBuffer::MAX_VERTEX_BUFFER_BINDING_COUNT
  // is 8; a ninth binding is dropped by qrhid3d11.cpp:1234-1237 and its
  // attributes read zeroes. Nine is what this geometry published before
  // translation and instance_color0 were interleaved.
  CHECK(r.layout.bindings <= 8);
  CHECK(r.layout.bindings == 8);
  // One vertex-input entry per binding: drawSingleMesh binds them positionally.
  CHECK(r.layout.inputs == r.layout.bindings);

  // The packing itself: same binding, translation at byte 0, color at byte 16
  // of a 32-byte per-instance slot stepping once per instance.
  CHECK(r.layout.color_binding == r.layout.translation_binding);
  CHECK(r.layout.translation_offset == 0);
  CHECK(r.layout.color_offset == 16);
  CHECK(r.layout.packed_stride == 32u);
  CHECK(
      r.layout.packed_classification
      == (int)ossia::geometry::binding::per_instance);
  CHECK(r.layout.packed_step_rate == 1);
  CHECK(
      r.layout.translation_format == (int)ossia::geometry::attribute::float3);
  CHECK(r.layout.color_format == (int)ossia::geometry::attribute::float4);

  // draw_id stays on its own 4-byte binding: it is a uint diff-uploaded from a
  // CPU mirror as one contiguous range, which interleaving would scatter.
  CHECK(r.layout.draw_id_binding != r.layout.translation_binding);
  CHECK(r.layout.draw_id_offset == 0);
  CHECK(r.layout.draw_id_stride == 4u);

  // ---- Oracle 2: both halves of the slot, read back as pixels. ----
  REQUIRE(r.valid);
  INFO(
      "white=" << r.px.white << " nonBlack=" << r.px.nonBlack << " corners="
               << r.px.cornerLit << " centre=(" << r.px.centreR << ","
               << r.px.centreG << "," << r.px.centreB << ")");

  // The quad covers NDC [-0.5,0.5]^2 -> the middle 32x32 of a 64x64 frame:
  // 1024 pixels, allowing for edge rules. Reading the color half at offset 0
  // paints (0,0,0) and collapses `white`; reading the translation half at
  // offset 16 shifts the quad a full NDC unit off screen and collapses both
  // counts.
  CHECK(r.px.white > 700);
  CHECK(r.px.white < 1400);
  // ... and it is a quad, not a full-frame fill: the corners stay background.
  CHECK(r.px.cornerLit == 0);
  // The centre is the identity color the preprocessor writes into a regular
  // slot, (1,1,1,1) -- not the (0,0,0,0) translation half next to it.
  CHECK(r.px.centreR > 200);
  CHECK(r.px.centreG > 200);
  CHECK(r.px.centreB > 200);
}
