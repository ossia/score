// UNIT — the CPU scene flattener (Gfx/Graph/SceneGPUState.{hpp,cpp}).
//
// flattenScene / packMaterial / packMaterialExtensions / primitiveToGeometry
// are pure functions over plain ossia::scene_spec data: no QRhi, no display,
// no document. Everything the ScenePreprocessor publishes to shaders is
// derived from what these produce, so the algebra pinned here is the floor
// under every scene render.

#include <Gfx/Graph/CameraMath.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QVector3D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using namespace score::gfx;
using Catch::Approx;

namespace
{
using payloads = std::vector<ossia::scene_payload>;

std::shared_ptr<ossia::scene_node>
makeNode(uint64_t id, payloads children)
{
  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = id;
  n->children = std::make_shared<const payloads>(std::move(children));
  return n;
}

ossia::scene_transform translation(float x, float y, float z)
{
  ossia::scene_transform t;
  t.translation[0] = x;
  t.translation[1] = y;
  t.translation[2] = z;
  return t;
}

ossia::scene_transform scaling(float s)
{
  ossia::scene_transform t;
  t.scale[0] = t.scale[1] = t.scale[2] = s;
  return t;
}

ossia::scene_transform slottedTranslation(float x, float y, float z, uint32_t slot)
{
  auto t = translation(x, y, z);
  t.raw_slot.arena = 1;
  t.raw_slot.size = sizeof(float) * 16;
  t.raw_slot.offset = slot * t.raw_slot.size;
  t.raw_slot.internal_index = slot;
  return t;
}

//! A minimal but valid drawable primitive: one CPU vertex buffer, one
//! position attribute, non-zero vertex_count. flattenScene drops primitives
//! that fail either of the latter two.
ossia::mesh_primitive makeTriangle(uint64_t stable_id = 0)
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
  prim.stable_id = stable_id;

  ossia::vertex_attribute pos;
  pos.semantic = ossia::attribute_semantic::position;
  pos.format = ossia::vertex_format::float3;
  pos.buffer_index = 0;
  pos.byte_offset = 0;
  pos.byte_stride = 12;
  prim.attributes.push_back(pos);

  return prim;
}

std::shared_ptr<ossia::mesh_component> makeMesh(ossia::mesh_primitive prim)
{
  auto mc = std::make_shared<ossia::mesh_component>();
  mc->primitives.push_back(std::move(prim));
  return mc;
}

std::shared_ptr<ossia::light_component> makeLight(uint32_t slot, bool withProducer)
{
  auto l = std::make_shared<ossia::light_component>();
  l->type = ossia::light_type::directional;
  if(withProducer)
  {
    l->raw_slot.arena = 2;
    l->raw_slot.size = sizeof(RawLightData);
    l->raw_slot.offset = slot * sizeof(RawLightData);
    l->raw_slot.internal_index = slot;
  }
  return l;
}

std::shared_ptr<ossia::camera_component> makeCamera(float znear, float zfar)
{
  auto c = std::make_shared<ossia::camera_component>();
  c->znear = znear;
  c->zfar = zfar;
  return c;
}

ossia::scene_spec specOf(
    std::vector<ossia::scene_node_ptr> roots,
    std::vector<ossia::material_component_ptr> materials = {},
    std::vector<ossia::camera_component_ptr> cameras = {},
    ossia::scene_node_id active = {})
{
  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(
      std::move(roots));
  if(!materials.empty())
    st->materials
        = std::make_shared<const std::vector<ossia::material_component_ptr>>(
            std::move(materials));
  if(!cameras.empty())
    st->cameras
        = std::make_shared<const std::vector<ossia::camera_component_ptr>>(
            std::move(cameras));
  st->active_camera_id = active;

  ossia::scene_spec spec;
  spec.state = st;
  return spec;
}

QVector3D origin(const QMatrix4x4& m)
{
  return m.map(QVector3D{0.f, 0.f, 0.f});
}
}

TEST_CASE("flattenScene: an empty scene produces nothing", "[scene][flatten]")
{
  FlatScene out;

  SECTION("null scene_state")
  {
    flattenScene(ossia::scene_spec{}, out, 16.f / 9.f);
  }
  SECTION("state present but no roots")
  {
    flattenScene(specOf({}), out, 16.f / 9.f);
  }
  SECTION("a root holding nothing")
  {
    flattenScene(specOf({makeNode(1, {})}), out, 16.f / 9.f);
  }

  CHECK(out.draws.empty());
  CHECK(out.lightArenaSlots.empty());
  CHECK(out.materials.empty());
  CHECK(out.cameras.empty());
  CHECK(out.worldTransforms.empty());
  CHECK(out.activeCameraIndex == -1);
  CHECK_FALSE(out.hasCamera);
}

TEST_CASE("flattenScene: the fallback eye used when a scene has no camera", "[scene][flatten]")
{
  FlatScene out;
  flattenScene(specOf({makeNode(1, {})}), out, 16.f / 9.f);

  // FlatScene's own doc-comment says this eye is (0,1,3); SceneGPUState.cpp
  // writes (0,0,3). The code is the contract.
  CHECK(out.cameraPosition.x() == Approx(0.f));
  CHECK(out.cameraPosition.y() == Approx(0.f));
  CHECK(out.cameraPosition.z() == Approx(3.f));
  CHECK(out.cameraFov == Approx(60.f));
  CHECK(out.cameraNear == Approx(0.1f));
  CHECK(out.cameraFar == Approx(1000.f));
  CHECK_FALSE(out.hasCamera);

  const auto seen = out.viewMatrix.map(QVector3D{0.f, 0.f, 0.f});
  CHECK(seen.z() == Approx(-3.f));
}

TEST_CASE("flattenScene: reuse does not carry the previous scene's camera", "[scene][flatten]")
{
  auto cam = makeCamera(2.f, 2000.f);
  auto root = makeNode(
      1, {ossia::scene_payload{translation(0.f, 0.f, 50.f)},
          ossia::scene_payload{ossia::camera_component_ptr{cam}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);
  REQUIRE(out.hasCamera);
  REQUIRE(out.cameraPosition.z() == Approx(50.f));

  // The empty-scene path returns before the fallback block would rewrite the
  // legacy mirror, so clear() is the only thing that can reset it. A consumer
  // that reads the mirror without consulting hasCamera must not see the
  // previous scene.
  flattenScene(ossia::scene_spec{}, out, 1.f);
  CHECK_FALSE(out.hasCamera);
  CHECK(out.activeCameraIndex == -1);
  CHECK(out.cameraPosition.z() == Approx(0.f));
  CHECK(out.cameraNear == Approx(0.1f));
  CHECK(out.cameraFar == Approx(1000.f));
  CHECK(out.cameraFov == Approx(60.f));
  CHECK(out.viewMatrix.isIdentity());
  CHECK(out.projectionMatrix.isIdentity());
}

TEST_CASE("flattenScene: flattening clears whatever the caller passed in", "[scene][flatten]")
{
  FlatScene out;
  out.draws.emplace_back();
  out.lightArenaSlots.push_back(3u);
  out.materials.emplace_back();
  out.cameras.emplace_back();
  out.activeCameraIndex = 4;
  out.hasCamera = true;

  flattenScene(ossia::scene_spec{}, out, 1.f);

  CHECK(out.draws.empty());
  CHECK(out.lightArenaSlots.empty());
  CHECK(out.materials.empty());
  CHECK(out.cameras.empty());
  CHECK(out.activeCameraIndex == -1);
  CHECK_FALSE(out.hasCamera);
}

TEST_CASE("flattenScene: world transforms compose down the parent chain", "[scene][flatten]")
{
  auto child = makeNode(
      2, {ossia::scene_payload{slottedTranslation(0.f, 2.f, 0.f, 9u)},
          ossia::scene_payload{
              ossia::mesh_component_ptr{makeMesh(makeTriangle(11))}}});
  auto root = makeNode(
      1, {ossia::scene_payload{slottedTranslation(1.f, 0.f, 0.f, 7u)},
          ossia::scene_payload{ossia::scene_node_ptr{child}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);

  REQUIRE(out.draws.size() == 1);
  const auto p = origin(out.draws[0].worldTransform);
  CHECK(p.x() == Approx(1.f));
  CHECK(p.y() == Approx(2.f));
  CHECK(p.z() == Approx(0.f));

  CHECK(out.draws[0].transform_slot == 9u);
  CHECK(out.draws[0].stable_id == 11u);

  REQUIRE(out.worldTransforms.size() == 2);
  CHECK(out.worldTransforms[0].transform_slot == 7u);
  CHECK(origin(out.worldTransforms[0].world).x() == Approx(1.f));
  CHECK(out.worldTransforms[1].transform_slot == 9u);
  CHECK(origin(out.worldTransforms[1].world).y() == Approx(2.f));
}

TEST_CASE("flattenScene: the parent transform is applied on the left", "[scene][flatten]")
{
  auto child = makeNode(
      2, {ossia::scene_payload{translation(1.f, 0.f, 0.f)},
          ossia::scene_payload{
              ossia::mesh_component_ptr{makeMesh(makeTriangle(1))}}});
  auto root = makeNode(
      1, {ossia::scene_payload{scaling(2.f)},
          ossia::scene_payload{ossia::scene_node_ptr{child}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);

  REQUIRE(out.draws.size() == 1);
  // parent * child: the child's 1-unit offset is scaled by the parent.
  // The other order would leave it at 1.
  CHECK(origin(out.draws[0].worldTransform).x() == Approx(2.f));
}

TEST_CASE("flattenScene: a child's transform does not leak to its siblings", "[scene][flatten]")
{
  auto a = makeNode(
      2, {ossia::scene_payload{translation(0.f, 5.f, 0.f)},
          ossia::scene_payload{
              ossia::mesh_component_ptr{makeMesh(makeTriangle(1))}}});
  auto b = makeNode(
      3, {ossia::scene_payload{
             ossia::mesh_component_ptr{makeMesh(makeTriangle(2))}}});
  auto root = makeNode(
      1, {ossia::scene_payload{translation(1.f, 0.f, 0.f)},
          ossia::scene_payload{ossia::scene_node_ptr{a}},
          ossia::scene_payload{ossia::scene_node_ptr{b}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);

  REQUIRE(out.draws.size() == 2);
  CHECK(origin(out.draws[0].worldTransform).y() == Approx(5.f));
  CHECK(origin(out.draws[1].worldTransform).y() == Approx(0.f));
  CHECK(origin(out.draws[1].worldTransform).x() == Approx(1.f));
}

TEST_CASE("flattenScene: an inactive node prunes its whole subtree", "[scene][flatten]")
{
  auto child = makeNode(
      2, {ossia::scene_payload{
             ossia::mesh_component_ptr{makeMesh(makeTriangle(1))}}});
  auto root = makeNode(1, {ossia::scene_payload{ossia::scene_node_ptr{child}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);
  CHECK(out.draws.size() == 1);

  child->active = false;
  flattenScene(specOf({root}), out, 1.f);
  CHECK(out.draws.empty());

  // `visible` is a render-time toggle, not a walk-time prune: the flattener
  // must still emit the draw (SceneFilterNode is what drops it).
  child->active = true;
  child->visible = false;
  flattenScene(specOf({root}), out, 1.f);
  CHECK(out.draws.size() == 1);
}

TEST_CASE("flattenScene: a primitive with no buffers or no vertices is dropped", "[scene][flatten]")
{
  auto mc = std::make_shared<ossia::mesh_component>();
  mc->primitives.push_back(ossia::mesh_primitive{});
  auto zero = makeTriangle(1);
  zero.vertex_count = 0;
  mc->primitives.push_back(std::move(zero));
  mc->primitives.push_back(makeTriangle(2));

  auto root = makeNode(
      1, {ossia::scene_payload{ossia::mesh_component_ptr{mc}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);
  REQUIRE(out.draws.size() == 1);
  CHECK(out.draws[0].stable_id == 2u);
}

TEST_CASE("flattenScene: cameras are collected with their world placement", "[scene][flatten]")
{
  auto camA = makeCamera(0.5f, 500.f);
  auto camB = makeCamera(0.25f, 250.f);

  auto nodeA = makeNode(
      10, {ossia::scene_payload{translation(4.f, 0.f, 0.f)},
           ossia::scene_payload{ossia::camera_component_ptr{camA}}});
  auto nodeB = makeNode(
      20, {ossia::scene_payload{translation(0.f, 0.f, 8.f)},
           ossia::scene_payload{ossia::camera_component_ptr{camB}}});
  auto root = makeNode(
      1, {ossia::scene_payload{ossia::scene_node_ptr{nodeA}},
          ossia::scene_payload{ossia::scene_node_ptr{nodeB}}});

  SECTION("the first camera wins when no active_camera_id is set")
  {
    FlatScene out;
    flattenScene(specOf({root}), out, 1.f);

    REQUIRE(out.cameras.size() == 2);
    CHECK(out.cameras[0].component == camA);
    CHECK(out.cameras[0].node_id.value == 10u);
    CHECK(out.cameras[1].node_id.value == 20u);
    CHECK(origin(out.cameras[0].worldTransform).x() == Approx(4.f));
    CHECK(origin(out.cameras[1].worldTransform).z() == Approx(8.f));

    CHECK(out.activeCameraIndex == 0);
    CHECK(out.hasCamera);
    CHECK(out.cameraNear == Approx(0.5f));
    CHECK(out.cameraFar == Approx(500.f));
    CHECK(out.cameraPosition.x() == Approx(4.f));
    CHECK(origin(out.viewMatrix).x() == Approx(-4.f));
  }

  SECTION("active_camera_id selects by the node the camera hangs off")
  {
    FlatScene out;
    flattenScene(specOf({root}, {}, {}, ossia::scene_node_id{20}), out, 1.f);

    REQUIRE(out.cameras.size() == 2);
    CHECK(out.activeCameraIndex == 1);
    CHECK(out.cameraNear == Approx(0.25f));
    CHECK(out.cameraPosition.z() == Approx(8.f));
  }

  SECTION("an unmatched active_camera_id falls back to the first camera")
  {
    FlatScene out;
    flattenScene(specOf({root}, {}, {}, ossia::scene_node_id{999}), out, 1.f);
    CHECK(out.activeCameraIndex == 0);
  }
}

TEST_CASE("flattenScene: a scene_state camera is deduped against the tree walk", "[scene][flatten]")
{
  auto cam = makeCamera(0.1f, 100.f);
  auto nodeA = makeNode(
      10, {ossia::scene_payload{translation(0.f, 0.f, 6.f)},
           ossia::scene_payload{ossia::camera_component_ptr{cam}}});
  auto root = makeNode(1, {ossia::scene_payload{ossia::scene_node_ptr{nodeA}}});

  FlatScene out;
  flattenScene(specOf({root}, {}, {cam}), out, 1.f);

  REQUIRE(out.cameras.size() == 1);
  CHECK(origin(out.cameras[0].worldTransform).z() == Approx(6.f));
}

TEST_CASE("flattenScene: light arena slots keep the producer-less sentinel", "[scene][flatten][issue171]")
{
  auto withSlot = makeLight(5u, true);
  auto without = makeLight(0u, false);

  auto root = makeNode(
      1, {ossia::scene_payload{ossia::light_component_ptr{withSlot}},
          ossia::scene_payload{ossia::light_component_ptr{without}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);

  REQUIRE(out.lightArenaSlots.size() == 2);
  CHECK(out.lightArenaSlots[0] == 5u);
  CHECK(out.lightArenaSlots[1] == 0xFFFFFFFFu);

  // The compact shader-facing index list drops the sentinel entries.
  std::size_t addressable = 0;
  for(auto s : out.lightArenaSlots)
    if(s != 0xFFFFFFFFu)
      ++addressable;
  CHECK(addressable == 1);
}

TEST_CASE("flattenScene: the same light reached twice contributes one slot", "[scene][flatten][issue171]")
{
  auto light = makeLight(5u, true);
  auto a = makeNode(2, {ossia::scene_payload{ossia::light_component_ptr{light}}});
  auto b = makeNode(3, {ossia::scene_payload{ossia::light_component_ptr{light}}});
  auto root = makeNode(
      1, {ossia::scene_payload{ossia::scene_node_ptr{a}},
          ossia::scene_payload{ossia::scene_node_ptr{b}}});

  FlatScene out;
  flattenScene(specOf({root}), out, 1.f);
  CHECK(out.lightArenaSlots.size() == 1);
}

TEST_CASE("flattenScene: material indices resolve against scene_state.materials", "[scene][flatten]")
{
  auto matA = std::make_shared<ossia::material_component>();
  auto matB = std::make_shared<ossia::material_component>();
  auto orphan = std::make_shared<ossia::material_component>();

  auto primB = makeTriangle(1);
  primB.material = matB;
  auto primOrphan = makeTriangle(2);
  primOrphan.material = orphan;
  auto primNone = makeTriangle(3);

  auto mc = std::make_shared<ossia::mesh_component>();
  mc->primitives.push_back(std::move(primB));
  mc->primitives.push_back(std::move(primOrphan));
  mc->primitives.push_back(std::move(primNone));

  auto root = makeNode(1, {ossia::scene_payload{ossia::mesh_component_ptr{mc}}});

  FlatScene out;
  flattenScene(specOf({root}, {matA, matB}), out, 1.f);

  REQUIRE(out.materials.size() == 2);
  REQUIRE(out.material_extensions.size() == 2);
  REQUIRE(out.draws.size() == 3);
  CHECK(out.draws[0].materialIndex == 1);
  CHECK(out.draws[1].materialIndex == -1);
  CHECK(out.draws[2].materialIndex == -1);
}

TEST_CASE("packMaterial: the feature mask is derived bit by bit", "[scene][material]")
{
  using namespace material_feature;

  SECTION("a default material sets no feature bits")
  {
    ossia::material_component mc;
    CHECK(packMaterial(mc).feature_mask == 0u);
  }

  SECTION("base-colour texture + MASK + doubleSided")
  {
    ossia::material_component mc;
    mc.base_color_texture.source
        = std::make_shared<const ossia::texture_source>();
    mc.alpha = ossia::alpha_mode::mask;
    mc.double_sided = true;

    const auto gpu = packMaterial(mc);
    CHECK(
        gpu.feature_mask
        == (has_base_color_texture | alpha_non_opaque | alpha_mask
            | double_sided));
  }

  SECTION("BLEND sets alpha_non_opaque and alpha_blend but not alpha_mask")
  {
    ossia::material_component mc;
    mc.alpha = ossia::alpha_mode::blend;
    CHECK(packMaterial(mc).feature_mask == (alpha_non_opaque | alpha_blend));
  }

  SECTION("the caster opt-outs are inverted — set means disabled")
  {
    ossia::material_component mc;
    mc.shadow_caster = false;
    mc.reflection_caster = false;
    CHECK(
        packMaterial(mc).feature_mask
        == (shadow_caster_disabled | reflection_caster_disabled));
  }

  SECTION("texcoord sets are packed two bits per channel and clamped to 1")
  {
    ossia::material_component mc;
    mc.base_color_texture.texcoord_set = 1;
    mc.normal_texture.texcoord_set = 7;
    CHECK(packMaterial(mc).feature_mask == ((1u << 20) | (1u << 24)));
  }

  SECTION("a non-default specular is a feature; the glTF default is not")
  {
    ossia::material_component mc;
    CHECK((packMaterial(mc).feature_mask & has_specular) == 0u);
    mc.specular.factor = 0.5f;
    CHECK((packMaterial(mc).feature_mask & has_specular) != 0u);
  }
}

TEST_CASE("packMaterial: factors and texture refs", "[scene][material]")
{
  ossia::material_component mc;
  mc.base_color_factor[0] = 0.25f;
  mc.base_color_factor[3] = 0.5f;
  mc.metallic_factor = 0.75f;
  mc.roughness_factor = 0.125f;
  mc.occlusion_strength = 0.375f;
  mc.unlit = true;
  mc.emissive_factor[1] = 2.f;
  mc.emissive_strength = 3.f;
  mc.alpha_cutoff = 0.9f;

  const auto gpu = packMaterial(mc);
  CHECK(gpu.baseColor[0] == Approx(0.25f));
  CHECK(gpu.baseColor[3] == Approx(0.5f));
  CHECK(gpu.metallicRoughnessOcclusionUnlit[0] == Approx(0.75f));
  CHECK(gpu.metallicRoughnessOcclusionUnlit[1] == Approx(0.125f));
  CHECK(gpu.metallicRoughnessOcclusionUnlit[2] == Approx(0.375f));
  CHECK(gpu.metallicRoughnessOcclusionUnlit[3] == Approx(1.f));
  CHECK(gpu.emissive_strength[1] == Approx(2.f));
  CHECK(gpu.emissive_strength[3] == Approx(3.f));
  CHECK(gpu.alpha_cutoff == Approx(0.9f));
  CHECK(gpu.hit_group_id == 0u);

  // The refs are filled later, by ScenePreprocessor::patchMaterialRefsFromCache.
  for(auto ref : gpu.textureRefs)
    CHECK(ref == tex_ref_none());
  CHECK(gpu.occlusion_textureRef == tex_ref_none());

  CHECK(sizeof(MaterialGPU) == 80);
}

TEST_CASE("tex_ref packing round-trips through the shader's decode expressions", "[scene][material]")
{
  const auto decodeSource = [](uint32_t ref) { return (ref >> 30) & 0x3u; };
  const auto decodeBucket = [](uint32_t ref) { return (ref >> 23) & 0x7Fu; };
  const auto decodeLayer = [](uint32_t ref) { return ref & 0x007FFFFFu; };

  for(auto [bucket, layer] :
      {std::pair<uint32_t, uint32_t>{0u, 0u}, {15u, 1023u}, {127u, 0x7FFFFFu}})
  {
    const auto ref = tex_ref_static(bucket, layer);
    CHECK(ref != tex_ref_none());
    CHECK(decodeSource(ref) == 1u);
    CHECK(decodeBucket(ref) == bucket);
    CHECK(decodeLayer(ref) == layer);
  }

  const auto dyn = tex_ref_dynamic(3u);
  CHECK(dyn != tex_ref_none());
  CHECK(decodeSource(dyn) == 2u);
  CHECK(decodeLayer(dyn) == 3u);
}

TEST_CASE("primitiveToGeometry: buffers, bindings and counts mirror the primitive", "[scene][flatten]")
{
  std::shared_ptr<ossia::geometry> geom;
  {
    auto prim = makeTriangle(1);
    prim.topology = ossia::primitive_topology::triangle_strip;

    ossia::vertex_attribute uv;
    uv.semantic = ossia::attribute_semantic::texcoord0;
    uv.format = ossia::vertex_format::float2;
    uv.buffer_index = 0;
    uv.byte_offset = 36;
    uv.byte_stride = 8;
    prim.attributes.push_back(uv);

    geom = primitiveToGeometry(prim);
  }

  REQUIRE(geom);
  CHECK(geom->vertices == 3);
  CHECK(geom->indices == 0);
  CHECK(geom->instances == 1);
  CHECK(geom->index.buffer == -1);
  CHECK(geom->topology == ossia::geometry::triangle_strip);
  REQUIRE(geom->buffers.size() == 1);

  // Distinct strides into the same buffer must not collapse into one binding:
  // that would push every attribute through the first stride.
  REQUIRE(geom->bindings.size() == 2);
  CHECK(geom->bindings[0].byte_stride == 12);
  CHECK(geom->bindings[1].byte_stride == 8);
  REQUIRE(geom->input.size() == 2);
  CHECK(geom->input[0].buffer == 0);
  CHECK(geom->input[1].buffer == 0);

  REQUIRE(geom->attributes.size() == 2);
  CHECK(geom->attributes[0].binding == 0);
  CHECK(geom->attributes[0].semantic == ossia::attribute_semantic::position);
  CHECK(geom->attributes[1].binding == 1);
  CHECK(geom->attributes[1].byte_offset == 36);
}

TEST_CASE("primitiveToGeometry: an index buffer becomes the last buffer entry", "[scene][flatten]")
{
  auto prim = makeTriangle(1);
  auto idx = std::make_shared<std::vector<uint16_t>>(
      std::vector<uint16_t>{0, 1, 2});
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(idx, idx->data());
  bd.byte_size = 6;
  bd.usage_hint = ossia::buffer_data::usage::index_buffer;
  br->resource = bd;
  prim.index_buffer = br;
  prim.index_type = ossia::index_format::uint16;
  prim.index_count = 3;

  auto geom = primitiveToGeometry(prim);
  REQUIRE(geom);
  REQUIRE(geom->buffers.size() == 2);
  CHECK(geom->index.buffer == 1);
  CHECK(geom->index.format == decltype(geom->index)::uint16);
  CHECK(geom->indices == 3);
}

TEST_CASE("primitiveToGeometry: the result outlives its source primitive", "[scene][flatten]")
{
  std::shared_ptr<ossia::geometry> geom;
  {
    auto prim = makeTriangle(1);
    geom = primitiveToGeometry(prim);
  }
  REQUIRE(geom);
  REQUIRE(geom->buffers.size() == 1);

  auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&geom->buffers[0].data);
  REQUIRE(cpu);
  REQUIRE(cpu->raw_data);
  CHECK(cpu->byte_size == 36);
  CHECK(static_cast<const float*>(cpu->raw_data.get())[3] == Approx(1.f));
}
