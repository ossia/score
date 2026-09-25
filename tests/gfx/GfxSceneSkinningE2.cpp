// Pins skinning data through the Scene Preprocessor, the inputs
// classic_pbr_skinned.frag reads: a mesh carrying JOINTS_0 / WEIGHTS_0 and a
// skin reaches a raw raster as joints_0 (uvec4) / weights_0 (vec4) vertex
// attributes, with a `joint_matrices` auxiliary holding every skeleton's
// joint matrices and per_draws[].skeleton_offset pointing at the draw's first
// one. The preprocessor published none of them, so the pipeline was refused
// for the missing required inputs and nothing drew.
//
// Scene: two skeletons. S0 = {j0 identity, j1 child of j0 translated +1 in x};
// S1 = {j0 translated +1 in x}. Quad A (upper left) is weighted to S0.j1,
// quad B (lower left) to S1.j0, so both land on the right half; reading S0.j0
// for B (ignoring the skeleton offset) would leave B on the left.
//
// The same scene is also published with scene_state.skeletons empty (what
// merge_scenes leaves when a Camera or Light is cabled into the same
// preprocessor input): the skins the meshes reference are packed anyway. And
// with scene_state.skeletons holding copies of the meshes' skeletons, posed
// differently (what AnimationPlayer publishes): the copy with the same joint
// node ids is the one used.
//
// Also pins the integer fallback of an optional vertex input: a uvec4 input
// declared REQUIRED:false with a DEFAULT reads that DEFAULT when the upstream
// geometry has no such attribute (it used to refuse the pipeline).
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

template <typename T>
std::shared_ptr<ossia::buffer_resource> cpuBuffer(std::vector<T> data)
{
  auto vec = std::make_shared<std::vector<T>>(std::move(data));
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(vec, vec->data());
  bd.byte_size = int64_t(vec->size() * sizeof(T));
  bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
  auto br = std::make_shared<ossia::buffer_resource>();
  br->resource = std::move(bd);
  br->dirty_index = 1;
  return br;
}

void addAttribute(
    ossia::mesh_primitive& prim, ossia::attribute_semantic sem,
    ossia::vertex_format fmt, uint32_t stride)
{
  ossia::vertex_attribute a;
  a.semantic = sem;
  a.format = fmt;
  a.buffer_index = (uint32_t)prim.attributes.size();
  a.byte_offset = 0;
  a.byte_stride = stride;
  a.rate = ossia::vertex_attribute::input_rate::per_vertex;
  prim.attributes.push_back(a);
}

// Quad over x in [-0.9, -0.1], y in [y0, y1]; every vertex fully weighted to
// `joint` when `skin` is set.
ossia::mesh_component_ptr quad(
    float y0, float y1, uint64_t id, ossia::skeleton_component_ptr skin,
    uint32_t joint)
{
  const float x0 = -0.9f, x1 = -0.1f;
  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(cpuBuffer<float>(
      {x0, y0, 0, x1, y0, 0, x1, y1, 0, x0, y0, 0, x1, y1, 0, x0, y1, 0}));
  addAttribute(
      prim, ossia::attribute_semantic::position, ossia::vertex_format::float3, 12);
  if(skin)
  {
    std::vector<uint32_t> joints;
    std::vector<float> weights;
    for(int v = 0; v < 6; ++v)
    {
      joints.insert(joints.end(), {joint, 0u, 0u, 0u});
      weights.insert(weights.end(), {1.f, 0.f, 0.f, 0.f});
    }
    prim.vertex_buffers.push_back(cpuBuffer(std::move(joints)));
    addAttribute(
        prim, ossia::attribute_semantic::joints0, ossia::vertex_format::uint32x4, 16);
    prim.vertex_buffers.push_back(cpuBuffer(std::move(weights)));
    addAttribute(
        prim, ossia::attribute_semantic::weights0, ossia::vertex_format::float4, 16);
  }
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = id;
  prim.bounds = {{x0, y0, 0.f}, {x1, y1, 0.f}};

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->skin = std::move(skin);
  mesh->dirty_index = 1;
  return mesh;
}

std::shared_ptr<ossia::skeleton_component>
skeleton(std::vector<std::pair<int, float>> parentAndX, uint64_t firstNodeId)
{
  auto sk = std::make_shared<ossia::skeleton_component>();
  for(auto [parent, x] : parentAndX)
  {
    ossia::skeleton_joint j;
    j.parent_index = parent;
    j.translation[0] = x;
    sk->joints.push_back(j);
    sk->joint_node_ids.push_back(ossia::scene_node_id{firstNodeId++});
  }
  sk->dirty_index = 1;
  return sk;
}

enum class Skins
{
  None,
  Listed,
  Unlisted,
  PosedCopies
};

std::shared_ptr<ossia::scene_state> makeState(Skins mode)
{
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  auto skeletons = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
  if(mode == Skins::PosedCopies)
  {
    auto s0 = skeleton({{-1, 0.f}, {0, 0.f}}, 100);
    auto s1 = skeleton({{-1, 0.f}}, 200);
    skeletons->push_back(skeleton({{-1, 0.f}, {0, 1.f}}, 100));
    skeletons->push_back(skeleton({{-1, 1.f}}, 200));
    children->push_back(quad(0.1f, 0.9f, 0xE2A11u, s0, 1));
    children->push_back(quad(-0.9f, -0.1f, 0xE2B22u, s1, 0));
  }
  else if(mode != Skins::None)
  {
    auto s0 = skeleton({{-1, 0.f}, {0, 1.f}}, 100);
    auto s1 = skeleton({{-1, 1.f}}, 200);
    if(mode == Skins::Listed)
    {
      skeletons->push_back(s0);
      skeletons->push_back(s1);
    }
    children->push_back(quad(0.1f, 0.9f, 0xE2A11u, s0, 1));
    children->push_back(quad(-0.9f, -0.1f, 0xE2B22u, s1, 0));
  }
  else
  {
    children->push_back(quad(-0.9f, 0.9f, 0xE2C33u, nullptr, 0));
  }
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::move(children);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(root));

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->skeletons = std::move(skeletons);
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

struct SceneNode final : score::gfx::ProcessNode
{
  Skins skins{};
  explicit SceneNode(Skins s)
      : skins{s}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct SceneRenderer final : score::gfx::NodeRenderer
{
  ossia::scene_spec m_scene;
  explicit SceneRenderer(const SceneNode& n)
      : NodeRenderer{n}
  {
    m_scene.state = makeState(n.skins);
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn_it->second->process(int(it - sink->node->input.begin()), m_scene, edge.source);
  }

  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&)
      override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
};

score::gfx::NodeRenderer*
SceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new SceneRenderer{*this};
}

struct Shot
{
  bool skipped{};
  std::string err;
  ReadbackImage img;
};

Shot render(score::gfx::GraphicsApi api, Skins skins, const char* shader)
{
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int scene = p.addNode(std::make_unique<SceneNode>(skins));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus((std::string{shader} + ".vs").c_str()),
        corpus((std::string{shader} + ".fs").c_str()));
    if(scene < 0 || flat < 0 || raster < 0)
    {
      s.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(scene, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.err = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    s.img = p.readback(sink);
    if(!s.img.valid())
      s.err = "readback failed";
  });
  return s;
}

bool green(const std::array<uint8_t, 4>& px)
{
  return px[0] < 40 && px[1] > 215 && px[2] < 40;
}
bool dark(const std::array<uint8_t, 4>& px)
{
  return px[0] < 40 && px[1] < 40 && px[2] < 40;
}
}

TEST_CASE(
    "a skinned scene mesh is deformed by its skeleton through the Scene "
    "Preprocessor",
    "[gfx][scene][skinning]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto skins = GENERATE(Skins::Listed, Skins::Unlisted, Skins::PosedCopies);
  CAPTURE(backend_name(api), int(skins));

  const Shot s = render(api, skins, "e2-scene-skin");
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.err);
  REQUIRE(s.err.empty());

  const auto upLeft = s.img.at(kSize / 4, kSize / 4);
  const auto upRight = s.img.at(3 * kSize / 4, kSize / 4);
  const auto lowLeft = s.img.at(kSize / 4, 3 * kSize / 4);
  const auto lowRight = s.img.at(3 * kSize / 4, 3 * kSize / 4);
  CAPTURE(int(upLeft[1]), int(upRight[1]), int(lowLeft[1]), int(lowRight[1]));
  CHECK(dark(upLeft));
  CHECK(dark(lowLeft));
  CHECK(green(upRight));
  CHECK(green(lowRight));
}

TEST_CASE(
    "an optional integer vertex input reads its DEFAULT when the geometry lacks it",
    "[gfx][scene][vertex-fallback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Shot s = render(api, Skins::None, "e2-scene-int-fallback");
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.err);
  REQUIRE(s.err.empty());

  const auto left = s.img.at(kSize / 4, kSize / 2);
  const auto right = s.img.at(3 * kSize / 4, kSize / 2);
  CAPTURE(int(left[0]), int(left[1]), int(left[2]));
  CHECK(green(left));
  CHECK(dark(right));
}
