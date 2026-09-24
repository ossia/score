// N95 — a scene_transform or light whose raw_slot was freed must not reach the
// preprocessor's world_transforms / scene_light_indices.
//
// An Asset Loader released and re-initialised keeps publishing the scene it
// wrapped before the release, stamped with its first RawTransform slot. The
// free-list hands that index to the Light on re-init, so two transforms name
// slot 0 and the one uploaded last wins. The walk order of merged producers
// varies from run to run (seen on Metal), so the point light sat either at its
// position or at the origin. flattenScene with the RenderList's registry drops
// refs that are not live in their arena, whatever the walk order.

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <vector>

using namespace score::test::gfx;
using Reg = score::gfx::GpuResourceRegistry;
using Catch::Approx;

namespace
{
using payloads = std::vector<ossia::scene_payload>;

ossia::scene_node_ptr makeNode(payloads children)
{
  auto n = std::make_shared<ossia::scene_node>();
  n->children = std::make_shared<const payloads>(std::move(children));
  return n;
}

ossia::scene_transform slotted(float x, float y, float z, ossia::gpu_slot_ref ref)
{
  ossia::scene_transform t;
  t.translation[0] = x;
  t.translation[1] = y;
  t.translation[2] = z;
  t.raw_slot = ref;
  return t;
}

ossia::mesh_component_ptr makeTriangleMesh()
{
  auto verts = std::make_shared<std::vector<float>>(
      std::vector<float>{0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f});
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(verts, verts->data());
  bd.byte_size = (int64_t)(verts->size() * sizeof(float));
  br->resource = bd;

  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(br);
  prim.vertex_count = 3;
  ossia::vertex_attribute pos;
  pos.semantic = ossia::attribute_semantic::position;
  pos.format = ossia::vertex_format::float3;
  pos.buffer_index = 0;
  pos.byte_offset = 0;
  pos.byte_stride = 12;
  prim.attributes.push_back(pos);

  auto mc = std::make_shared<ossia::mesh_component>();
  mc->primitives.push_back(std::move(prim));
  return mc;
}

ossia::light_component_ptr makeLight(ossia::gpu_slot_ref ref)
{
  auto l = std::make_shared<ossia::light_component>();
  l->type = ossia::light_type::point;
  l->raw_slot = ref;
  return l;
}

struct Result
{
  bool skipped{};
  std::string why;
  uint32_t liveXformIndex{};
  uint32_t liveLightIndex{};
  bool staleXformSameIndex{};
  bool staleLightSameIndex{};
  score::gfx::FlatScene withRegistry;
  score::gfx::FlatScene withoutRegistry;
};

Result run(score::gfx::GraphicsApi backend, bool staleLast)
{
  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(backend, probed))
    {
      r.skipped = true;
      r.why = probed;
      return;
    }
    auto st = score::gfx::createRenderState(backend, QSize{32, 32}, nullptr);
    if(!st || !st->rhi)
    {
      r.skipped = true;
      r.why = "no rhi";
      return;
    }
    QRhi& rhi = *st->rhi;
    {
      Reg reg;
      auto* batch = rhi.nextResourceUpdateBatch();
      reg.init(rhi, *batch);

      auto oldXform = reg.allocate(Reg::Arena::RawTransform, 64);
      auto oldLight = reg.allocate(Reg::Arena::RawLight, 64);
      const auto staleXformRef = reg.toOssiaRef(oldXform);
      const auto staleLightRef = reg.toOssiaRef(oldLight);
      reg.free(oldXform);
      reg.free(oldLight);
      auto newXform = reg.allocate(Reg::Arena::RawTransform, 64);
      auto newLight = reg.allocate(Reg::Arena::RawLight, 64);
      const auto liveXformRef = reg.toOssiaRef(newXform);
      const auto liveLightRef = reg.toOssiaRef(newLight);

      r.liveXformIndex = liveXformRef.internal_index;
      r.liveLightIndex = liveLightRef.internal_index;
      r.staleXformSameIndex
          = staleXformRef.internal_index == liveXformRef.internal_index;
      r.staleLightSameIndex
          = staleLightRef.internal_index == liveLightRef.internal_index;

      auto loaderNode = makeNode(
          {ossia::scene_payload{slotted(0.f, 0.f, 0.f, staleXformRef)},
           ossia::scene_payload{makeTriangleMesh()},
           ossia::scene_payload{makeLight(staleLightRef)}});
      auto lightNode = makeNode(
          {ossia::scene_payload{slotted(4.f, 3.f, -2.f, liveXformRef)},
           ossia::scene_payload{makeLight(liveLightRef)}});

      auto state = std::make_shared<ossia::scene_state>();
      state->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(
          staleLast ? std::vector<ossia::scene_node_ptr>{lightNode, loaderNode}
                    : std::vector<ossia::scene_node_ptr>{loaderNode, lightNode});
      ossia::scene_spec spec;
      spec.state = state;

      score::gfx::flattenScene(spec, r.withRegistry, 1.f, &reg);
      score::gfx::flattenScene(spec, r.withoutRegistry, 1.f);

      reg.free(newXform);
      reg.free(newLight);
      reg.destroy();
    }
    st->destroy();
  });
  return r;
}
}

TEST_CASE(
    "flattenScene drops transform and light slot refs freed before a re-init",
    "[gfx][scene][flatten][n95]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const bool staleLast = GENERATE(false, true);
  CAPTURE(backend_name(backend), staleLast);

  auto r = run(backend, staleLast);
  if(r.skipped)
    SKIP(r.why);

  REQUIRE(r.staleXformSameIndex);
  REQUIRE(r.staleLightSameIndex);

  REQUIRE(r.withoutRegistry.worldTransforms.size() == 2);

  const auto& fs = r.withRegistry;
  REQUIRE(fs.worldTransforms.size() == 1);
  CHECK(fs.worldTransforms[0].transform_slot == r.liveXformIndex);
  CHECK(fs.worldTransforms[0].world(0, 3) == Approx(4.f));
  CHECK(fs.worldTransforms[0].world(1, 3) == Approx(3.f));
  CHECK(fs.worldTransforms[0].world(2, 3) == Approx(-2.f));

  REQUIRE(fs.draws.size() == 1);
  CHECK(fs.draws[0].transform_slot == 0xFFFFFFFFu);

  REQUIRE(fs.lightArenaSlots.size() == 2);
  const auto live = staleLast ? fs.lightArenaSlots[0] : fs.lightArenaSlots[1];
  const auto stale = staleLast ? fs.lightArenaSlots[1] : fs.lightArenaSlots[0];
  CHECK(live == r.liveLightIndex);
  CHECK(stale == 0xFFFFFFFFu);
}
