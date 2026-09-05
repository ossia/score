// =============================================================================
// P2-13 — `unrendered payloads are transported, and say so`.
//
// FOUR of the fifteen ossia::scene_payload alternatives have no renderer:
// gaussian_splat_component_ptr, voxel_field_component_ptr,
// point_cloud_component_ptr and volume_component_ptr
// (geometry_port.hpp:1165-1168). The engine says so itself, in the only place
// in the whole `src/` tree that mentions them —
// src/plugins/score-plugin-gfx/Gfx/Graph/SceneGPUState.cpp:617-618, the last
// statement of FlattenVisitor::visitPayload, after the else-if chain has ended:
//
//     // gaussian_splat, voxel_field, point_cloud, volume — not rendered yet,
//     // but the types are transported. Renderers will handle them later.
//
// READ THAT LIST CAREFULLY: `point_cloud` there means
// ossia::point_cloud_component, which is NOT the rendered
// ossia::primitive_cloud_component sitting one branch above it at
// SceneGPUState.cpp:587-615 (that one IS collected, into
// FlatScene::primitive_clouds, and the ScenePreprocessor buckets it by
// format_id). The two are different alternatives of the same variant and the
// spec row's shorthand invites confusing them; every assertion below names the
// exact type.
//
// This test pins BOTH halves of that sentence.
//
//   (a) TRANSPORTED. The payloads survive every scene-cable hop that rebuilds
//       scene data: ossia::merge_scenes (geometry_port.cpp:335-562) and the
//       Threedim::SceneGraphFilter walker, whose Walker::rewrite is the one
//       real per-payload visitor in the chain — it allocates a fresh
//       std::vector<scene_payload> and copies survivors into it one by one
//       (SceneGraphFilter.cpp:471-499). Survival is asserted by shared_ptr
//       IDENTITY plus a field read-back (splat_count, voxel_count/resolution,
//       point_count/point_size, channel count/resolution), so a hop that
//       replaced a payload with a default-constructed one of the same kind
//       would still be caught.
//
//   (b) NOTHING CLAIMS TO HAVE DRAWN THEM. score::gfx::flattenScene
//       (SceneGPUState.cpp:725) is the single CPU seam between the scene tree
//       and the GPU: everything the ScenePreprocessor uploads or draws is
//       built from the FlatScene it fills (SceneGPUState.hpp:489-594). After
//       flattening a scene made of nothing but these four payloads, EVERY
//       FlatScene counter is zero — no draw call, no primitive-cloud entry,
//       no instance, no scene_data auxiliary, no light slot, no world
//       transform, no camera. And adding all four to a scene that DOES draw
//       leaves every counter bit-identical to the same scene without them:
//       a counter-space difference oracle, which is stronger than "== 0"
//       because it cannot be satisfied by flattening having failed outright.
//       The mesh control (draws == 1) is the in-test positive control that
//       proves these counters can be non-zero at all.
//
// COUNTERS, NOT PIXELS. No QRhi, no QWindow, no display, no document, no
// file I/O except the one source-text guard below. Runs on a host with no
// graphics stack, same as tests/unit/SceneFlattenTest.cpp and
// tests/gfx/SceneMergeMemo.cpp (the P1-4 GPU-less scene-assembly precedent).
//
// WHY tests/threedim AND NOT tests/unit. tests/unit/SceneFlattenTest.cpp is
// the sibling flattenScene suite and would have been the natural home, but
// this case needs the SceneGraphFilter transport hop compiled in, and only
// tests/threedim/CMakeLists.txt has threedim_test_includes() — the helper
// that resolves <Threedim/...> plus the plugin's deep 3rdparty include set.
// score::gfx::flattenScene is reached by LINKING score_plugin_gfx, never by
// recompiling its TU (see the linkage note in the registration block).
//
// NOTHING IN THE PRODUCT EVER BUILDS ONE OF THESE. Measured:
//
//     grep -rn 'gaussian_splat\|voxel_field_component\|point_cloud_component\
//       \|volume_component' src/
//     -> src/plugins/score-plugin-gfx/Gfx/Graph/SceneGPUState.cpp:617
//
// exactly one hit, the admission comment. No process constructs these
// components, so no real score can carry one and the test must synthesise
// them. "Transported" is a property of the type system and of the copying
// hops, not of an observed producer->consumer path — which is precisely why
// it is worth pinning: the day someone adds the producer, the transport must
// already work.
//
// ossia::scene_node::get_component<T>() / is<T>() are DECLARED but never
// defined anywhere in the tree (geometry_port.hpp:1195-1198 has no out-of-
// line definition), so the payload lookups here go through
// ossia::get_if<T>(&payload) like the product code does.
//
// -----------------------------------------------------------------------------
// SPEC CORRECTIONS (SPEC-SCENE-RENDER-TESTS.md §3.3, row P2-13).
//   1. The row cites `SceneGPUState.cpp:617-618` with no path; the file is
//      src/plugins/score-plugin-gfx/Gfx/Graph/SceneGPUState.cpp (score-plugin-
//      GFX, not -threedim). The two line numbers are exact.
//   2. "make the visitor drop them" names no reachable hook -- see the
//      NEGATIVE CONTROL section below for the three that do exist.
//   3. The claim itself holds: the payloads ARE transported and ARE not drawn,
//      verified against the code, so this case is written green (no
//      [!shouldfail] pin).
//   4. Not in the spec, worth knowing: no process anywhere in score constructs
//      one of these four components (the grep above), so this case guards a
//      type-system contract for a producer that does not exist yet, not any
//      user-reachable path. That is consistent with §3.3's own framing
//      ("several guard code no user reaches").
//
// -----------------------------------------------------------------------------
// REGISTRATION (append inside tests/threedim/CMakeLists.txt; ctest target
// name: `test_threedim_scene_payload_transport`):
//
//     # P2-13: the four payload kinds SceneGPUState.cpp:617-618 admits it does
//     # not render (gaussian_splat, voxel_field, point_cloud, volume) survive
//     # merge_scenes + the SceneGraphFilter payload rebuild, and contribute
//     # ZERO entries to every FlatScene counter. Compiles SceneGraphFilter.cpp
//     # (the transport visitor under test); ossia::merge_scenes comes from
//     # libossia through score_lib_base, and score::gfx::flattenScene is
//     # class-free SCORE_PLUGIN_GFX_EXPORT (SceneGPUState.hpp:597-601), so
//     # score_plugin_gfx is LINKED, not recompiled -- recompiling an exported
//     # TU into a test ODR-violates against the shared library. Qt Gui for the
//     # QMatrix4x4 in FlatScene. Pure CPU: no QRhi, no display, no document.
//     score_add_test(test_threedim_scene_payload_transport
//       SOURCES ScenePayloadTransportTest.cpp
//         "${THREEDIM_DIR}/SceneGraphFilter.cpp"
//       PLUGINS score_plugin_gfx
//       LIBS ${QT_PREFIX}::Gui)
//     threedim_test_includes(test_threedim_scene_payload_transport)
//     target_compile_definitions(test_threedim_scene_payload_transport PRIVATE
//       GFX_SRC_DIR="${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx/Gfx")
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROL (product-side, do not commit).
//
// The spec's phrasing -- "make the visitor drop them" -- does not name a real
// hook, and the FlattenVisitor is NOT it: that visitor has no branch for these
// four kinds at all (they fall off the end of the else-if chain at
// SceneGPUState.cpp:616 into the comment at :617), and it takes the scene by
// `const ossia::scene_spec&` (SceneGPUState.hpp:598), so there is nothing
// there to drop. The real per-payload visitor is the filter walker:
//
//   PRIMARY -- src/plugins/score-plugin-threedim/Threedim/SceneGraphFilter.cpp
//   :495-496, inside Walker::rewrite's non-scene_node branch. Replace
//
//       if(keep_self)
//         new_children->push_back(payload);
//
//   with
//
//       if(keep_self
//          && !ossia::get_if<ossia::gaussian_splat_component_ptr>(&payload)
//          && !ossia::get_if<ossia::voxel_field_component_ptr>(&payload)
//          && !ossia::get_if<ossia::point_cloud_component_ptr>(&payload)
//          && !ossia::get_if<ossia::volume_component_ptr>(&payload))
//         new_children->push_back(payload);
//
//   MUST REDDEN: every assertion in
//     "P2-13 (a): the SceneGraphFilter payload rebuild keeps all four kinds"
//     and in "P2-13 (a+b): merge -> filter -> flatten end to end" that reads a
//     payload out of the FILTERED tree.
//   MUST STAY GREEN: the merge-hop case (it runs before the filter), every
//     draw-side counter case (dropping a payload that draws nothing changes no
//     counter), and the mesh control draws == 1 (a mesh_component is not one of
//     the four). That green/red split is the point: it shows the two halves are
//     independently anchored, and that the counter half cannot mask a
//     transport regression.
//
//   SECONDARY, for the merge hop (which has NO payload visitor -- merge_scenes
//   re-shares whole roots by shared_ptr at geometry_port.cpp:438-441, so a
//   payload-level drop is not expressible there): delete the
//   `merged_roots->push_back(root);` at geometry_port.cpp:441. The merge case's
//   survival assertions redden (REQUIRE on roots->size() == 2), and so does
//   everything downstream of it.
//
//   THIRD, for the counter half -- it must be able to go red too, or "== 0" is
//   vacuous: at SceneGPUState.cpp:616, before the admission comment, add
//
//       else if(auto* pcl = ossia::get_if<ossia::point_cloud_component_ptr>(&payload))
//       {
//         if(*pcl)
//           out.primitive_clouds.push_back({{}, parentWorld, 0xFFFFFFFFu});
//       }
//
//   Every "primitive_clouds is empty" / "counters identical with and without"
//   assertion reddens; the transport cases stay green.
//
// STATUS: written against the sources cited above, all line numbers read at
// authoring time. NOT COMPILED AND NOT RUN in this session (the task forbade
// building), so "green on the current tree" is UNVERIFIED, as is every
// negative-control outcome predicted above -- they are derived from the code,
// not measured. Run all three before recording this row in the ledger.
// =============================================================================

#include <Threedim/SceneGraphFilter.hpp>

#include <Gfx/Graph/SceneGPUState.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/variant.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

using namespace score::gfx;

namespace
{
using Filter = Threedim::SceneGraphFilter;
using payloads = std::vector<ossia::scene_payload>;

// --- fixture builders --------------------------------------------------------

std::shared_ptr<ossia::scene_node>
make_node(const char* name, uint64_t id, payloads kids = {})
{
  auto n = std::make_shared<ossia::scene_node>();
  n->name = name;
  n->id.value = id;
  n->children = std::make_shared<payloads>(std::move(kids));
  return n;
}

std::shared_ptr<ossia::scene_state>
make_state(std::vector<ossia::scene_node_ptr> roots, int64_t version = 1)
{
  auto s = std::make_shared<ossia::scene_state>();
  s->roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>(std::move(roots));
  s->version = version;
  return s;
}

ossia::scene_spec spec_of(ossia::scene_state_ptr st)
{
  ossia::scene_spec sp;
  sp.state = std::move(st);
  return sp;
}

//! A CPU-resident buffer_resource, the buffer_data alternative of
//! buffer_resource::resource (geometry_port.hpp:514-539).
ossia::buffer_resource_ptr make_buffer(std::vector<float> values)
{
  auto v = std::make_shared<std::vector<float>>(std::move(values));
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(v, v->data());
  bd.byte_size = (int64_t)(v->size() * sizeof(float));
  br->resource = bd;
  return br;
}

//! A minimal but valid drawable primitive: one CPU vertex buffer, one position
//! attribute, non-zero vertex_count. FlattenVisitor::visitMesh drops primitives
//! failing either of the latter two (SceneGPUState.cpp:662-663). Same shape as
//! tests/unit/SceneFlattenTest.cpp's makeTriangle.
ossia::mesh_component_ptr make_triangle_mesh(uint64_t stable_id)
{
  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(
      make_buffer({0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f}));
  prim.vertex_count = 3;
  prim.stable_id = stable_id;

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

// The four unrendered kinds, each with distinctive field values so a hop that
// swapped in a default-constructed replacement is caught by more than pointer
// identity. Field names per geometry_port.hpp:982-1119.

constexpr uint32_t kSplatCount = 4321u;
constexpr uint8_t kSplatShDegree = 2u;
constexpr uint32_t kVoxelCount = 777u;
constexpr uint32_t kVoxelRes[3] = {8u, 16u, 32u};
constexpr uint64_t kPointCount = 123456u;
constexpr float kPointSize = 3.5f;
constexpr uint32_t kVolumeRes[3] = {4u, 5u, 6u};
constexpr float kVoxelSize = 0.25f;

ossia::gaussian_splat_component_ptr make_splat()
{
  auto s = std::make_shared<ossia::gaussian_splat_component>();
  s->positions = make_buffer({1.f, 2.f, 3.f});
  s->opacities = make_buffer({0.5f});
  s->splat_count = kSplatCount;
  s->sh_degree = kSplatShDegree;
  return s;
}

ossia::voxel_field_component_ptr make_voxel_field()
{
  auto v = std::make_shared<ossia::voxel_field_component>();
  v->densities = make_buffer({0.1f, 0.2f});
  v->voxel_count = kVoxelCount;
  v->resolution[0] = kVoxelRes[0];
  v->resolution[1] = kVoxelRes[1];
  v->resolution[2] = kVoxelRes[2];
  return v;
}

ossia::point_cloud_component_ptr make_point_cloud()
{
  auto p = std::make_shared<ossia::point_cloud_component>();
  p->positions = make_buffer({0.f, 1.f, 2.f});
  p->point_count = kPointCount;
  p->point_size = kPointSize;
  return p;
}

ossia::volume_component_ptr make_volume()
{
  auto v = std::make_shared<ossia::volume_component>();
  ossia::volume_channel ch;
  ch.data = make_buffer({1.f, 2.f, 3.f, 4.f});
  ch.data_type = ossia::volume_channel::type::scalar_float;
  v->channels.push_back(ch);
  v->resolution[0] = kVolumeRes[0];
  v->resolution[1] = kVolumeRes[1];
  v->resolution[2] = kVolumeRes[2];
  v->voxel_size = kVoxelSize;
  return v;
}

//! Every payload of kind `Ptr` carried directly by `n`.
template <typename Ptr>
std::vector<Ptr> payloads_of(const ossia::scene_node& n)
{
  std::vector<Ptr> out;
  if(n.children)
    for(const auto& p : *n.children)
      if(auto* v = ossia::get_if<Ptr>(&p))
        out.push_back(*v);
  return out;
}

//! The single payload of kind `Ptr` on `n`, or a null ptr.
template <typename Ptr>
Ptr payload_of(const ossia::scene_node& n)
{
  auto all = payloads_of<Ptr>(n);
  return all.size() == 1 ? all.front() : Ptr{};
}

//! Assert the four unrendered payloads on `n` are the very objects handed in,
//! with their data intact. `where` names the hop for the failure message.
void check_four_survived(
    const ossia::scene_node& n, const ossia::gaussian_splat_component_ptr& splat,
    const ossia::voxel_field_component_ptr& voxels,
    const ossia::point_cloud_component_ptr& points,
    const ossia::volume_component_ptr& volume, const char* where)
{
  INFO("hop: " << where);

  const auto s = payload_of<ossia::gaussian_splat_component_ptr>(n);
  REQUIRE(s);
  CHECK(s.get() == splat.get());
  CHECK(s->splat_count == kSplatCount);
  CHECK(s->sh_degree == kSplatShDegree);
  CHECK(s->positions.get() == splat->positions.get());
  REQUIRE(s->positions);
  CHECK(ossia::get_if<ossia::buffer_data>(&s->positions->resource) != nullptr);

  const auto v = payload_of<ossia::voxel_field_component_ptr>(n);
  REQUIRE(v);
  CHECK(v.get() == voxels.get());
  CHECK(v->voxel_count == kVoxelCount);
  CHECK(v->resolution[0] == kVoxelRes[0]);
  CHECK(v->resolution[1] == kVoxelRes[1]);
  CHECK(v->resolution[2] == kVoxelRes[2]);
  CHECK(v->densities.get() == voxels->densities.get());

  const auto p = payload_of<ossia::point_cloud_component_ptr>(n);
  REQUIRE(p);
  CHECK(p.get() == points.get());
  CHECK(p->point_count == kPointCount);
  CHECK(p->point_size == kPointSize);
  CHECK(p->positions.get() == points->positions.get());

  const auto vol = payload_of<ossia::volume_component_ptr>(n);
  REQUIRE(vol);
  CHECK(vol.get() == volume.get());
  REQUIRE(vol->channels.size() == 1);
  CHECK(vol->channels[0].data.get() == volume->channels[0].data.get());
  CHECK(vol->resolution[0] == kVolumeRes[0]);
  CHECK(vol->resolution[1] == kVolumeRes[1]);
  CHECK(vol->resolution[2] == kVolumeRes[2]);
  CHECK(vol->voxel_size == kVoxelSize);
}

// --- the draw-side counter surface -------------------------------------------

//! Every countable field of FlatScene (SceneGPUState.hpp:489-568). These are
//! all the ScenePreprocessor has to work from, so "nothing claims to have
//! drawn them" == all of these stay at their empty values.
struct Counters
{
  std::size_t draws{};
  std::size_t lightArenaSlots{};
  std::size_t materials{};
  std::size_t material_extensions{};
  std::size_t skins{};
  std::size_t worldTransforms{};
  std::size_t scene_data{};
  std::size_t instances{};
  std::size_t primitive_clouds{};
  std::size_t cameras{};
  int activeCameraIndex{-1};
  bool hasCamera{false};
};

Counters counters_of(const FlatScene& f)
{
  return Counters{
      f.draws.size(),
      f.lightArenaSlots.size(),
      f.materials.size(),
      f.material_extensions.size(),
      f.skins.size(),
      f.worldTransforms.size(),
      f.scene_data.size(),
      f.instances.size(),
      f.primitive_clouds.size(),
      f.cameras.size(),
      f.activeCameraIndex,
      f.hasCamera};
}

void check_counters_equal(const Counters& a, const Counters& b)
{
  CHECK(a.draws == b.draws);
  CHECK(a.lightArenaSlots == b.lightArenaSlots);
  CHECK(a.materials == b.materials);
  CHECK(a.material_extensions == b.material_extensions);
  CHECK(a.skins == b.skins);
  CHECK(a.worldTransforms == b.worldTransforms);
  CHECK(a.scene_data == b.scene_data);
  CHECK(a.instances == b.instances);
  CHECK(a.primitive_clouds == b.primitive_clouds);
  CHECK(a.cameras == b.cameras);
  CHECK(a.activeCameraIndex == b.activeCameraIndex);
  CHECK(a.hasCamera == b.hasCamera);
}

void check_counters_all_zero(const Counters& c)
{
  CHECK(c.draws == 0u);
  CHECK(c.lightArenaSlots == 0u);
  CHECK(c.materials == 0u);
  CHECK(c.material_extensions == 0u);
  CHECK(c.skins == 0u);
  CHECK(c.worldTransforms == 0u);
  CHECK(c.scene_data == 0u);
  CHECK(c.instances == 0u);
  CHECK(c.primitive_clouds == 0u);
  CHECK(c.cameras == 0u);
  CHECK(c.activeCameraIndex == -1);
  CHECK(c.hasCamera == false);
}

payloads four_unrendered(
    const ossia::gaussian_splat_component_ptr& splat,
    const ossia::voxel_field_component_ptr& voxels,
    const ossia::point_cloud_component_ptr& points,
    const ossia::volume_component_ptr& volume)
{
  return payloads{splat, voxels, points, volume};
}
} // namespace

// =============================================================================
// (b) NOTHING CLAIMS TO HAVE DRAWN THEM
// =============================================================================

TEST_CASE(
    "P2-13 (b): a scene of only unrendered payloads flattens to zero of "
    "everything",
    "[scene][flatten][payload][P2-13]")
{
  const auto splat = make_splat();
  const auto voxels = make_voxel_field();
  const auto points = make_point_cloud();
  const auto volume = make_volume();

  auto root
      = make_node("Unrendered", 1, four_unrendered(splat, voxels, points, volume));
  const auto spec = spec_of(make_state({root}));

  FlatScene flat;
  flattenScene(spec, flat, 16.f / 9.f);

  // The scene is NOT empty (scene_state::empty() is roots-based,
  // geometry_port.hpp:1297), so flattenScene really walked it and really
  // reached FlattenVisitor::visitPayload for each of the four.
  REQUIRE_FALSE(spec.state->empty());

  check_counters_all_zero(counters_of(flat));
}

TEST_CASE(
    "P2-13 (b): the unrendered payloads add nothing to a scene that does draw",
    "[scene][flatten][payload][P2-13]")
{
  const auto mesh = make_triangle_mesh(0xABCDEFu);

  // Control: the mesh alone. This is the positive control -- it proves the
  // counters CAN be non-zero, so the zeros above are not vacuous.
  FlatScene meshOnly;
  {
    auto root = make_node("Drawable", 1, payloads{mesh});
    flattenScene(spec_of(make_state({root})), meshOnly, 1.f);
  }
  REQUIRE(meshOnly.draws.size() == 1);
  CHECK(meshOnly.draws[0].stable_id == 0xABCDEFu);

  // Same mesh, with all four unrendered payloads around it -- interleaved
  // before and after, so a visitor that mishandled one and fell out of the
  // walk would lose the mesh too.
  const auto splat = make_splat();
  const auto voxels = make_voxel_field();
  const auto points = make_point_cloud();
  const auto volume = make_volume();

  auto mixed = make_node(
      "Mixed", 1, payloads{splat, voxels, mesh, points, volume});
  const auto spec = spec_of(make_state({mixed}));

  FlatScene withPayloads;
  flattenScene(spec, withPayloads, 1.f);

  // Difference oracle in counter space: identical, field for field.
  check_counters_equal(counters_of(withPayloads), counters_of(meshOnly));

  // ... and the one draw is still the mesh's, not something else that took
  // its place.
  REQUIRE(withPayloads.draws.size() == 1);
  CHECK(withPayloads.draws[0].stable_id == 0xABCDEFu);

  // flattenScene took the scene by const& (SceneGPUState.hpp:597-601): the
  // input tree is untouched by the walk, payloads and all.
  CHECK(mixed->children->size() == 5);
  check_four_survived(*mixed, splat, voxels, points, volume, "after flattenScene");
}

// =============================================================================
// (a) TRANSPORTED
// =============================================================================

TEST_CASE(
    "P2-13 (a): ossia::merge_scenes carries all four kinds through",
    "[scene][merge][payload][P2-13]")
{
  const auto splat = make_splat();
  const auto voxels = make_voxel_field();
  const auto points = make_point_cloud();
  const auto volume = make_volume();

  auto unrendered = make_node(
      "Unrendered", 1, four_unrendered(splat, voxels, points, volume));
  auto drawable
      = make_node("Drawable", 2, payloads{make_triangle_mesh(0x11u)});

  const auto a = spec_of(make_state({unrendered}, 3));
  const auto b = spec_of(make_state({drawable}, 5));

  const ossia::scene_spec inputs[2]{a, b};
  const auto merged
      = ossia::merge_scenes(std::span<const ossia::scene_spec>{inputs, 2});

  REQUIRE(merged.state);
  REQUIRE(merged.state->roots);
  REQUIRE(merged.state->roots->size() == 2);
  CHECK(merged.state->version == 6); // max(3,5) + 1, geometry_port.cpp:558

  // Roots are re-shared by shared_ptr (geometry_port.cpp:438-441), so the
  // payload vector is the same object -- assert it, then read the payloads.
  const auto& mergedUnrendered = (*merged.state->roots)[0];
  REQUIRE(mergedUnrendered);
  CHECK(mergedUnrendered.get() == unrendered.get());
  CHECK(mergedUnrendered->children.get() == unrendered->children.get());
  check_four_survived(
      *mergedUnrendered, splat, voxels, points, volume, "merge_scenes");

  // And the merged scene still draws exactly the one mesh the other
  // contributor brought: the unrendered half contributes nothing.
  FlatScene flat;
  flattenScene(merged, flat, 1.f);
  REQUIRE(flat.draws.size() == 1);
  CHECK(flat.draws[0].stable_id == 0x11u);
  CHECK(flat.primitive_clouds.empty());
  CHECK(flat.instances.empty());
  CHECK(flat.scene_data.empty());
}

TEST_CASE(
    "P2-13 (a): the SceneGraphFilter payload rebuild keeps all four kinds",
    "[scene][filter][payload][P2-13]")
{
  const auto splat = make_splat();
  const auto voxels = make_voxel_field();
  const auto points = make_point_cloud();
  const auto volume = make_volume();
  const auto mesh = make_triangle_mesh(0x22u);

  // "Doomed" does not match the filter, so it is dropped -- which sets
  // any_dropped and forces Walker::rewrite off its share-if-unchanged fast
  // path (SceneGraphFilter.cpp:505-526) and through the real payload rebuild
  // loop at :471-499. Without a dropped sibling the walker would return the
  // ORIGINAL node pointer and this case would prove nothing about the copy.
  auto doomed = make_node("Doomed", 9);
  auto kept = make_node(
      "Kept", 1,
      payloads{splat, voxels, mesh, points, volume, ossia::scene_node_ptr{doomed}});

  Filter f;
  f.inputs.scene_in.scene.state = make_state({kept}, 7);
  f.inputs.mode.value = Filter::ByName;
  f.inputs.names.value = {"Kept"};

  f();

  const auto& out = f.outputs.scene_out.scene.state;
  REQUIRE(out);
  REQUIRE(out->roots);
  REQUIRE(out->roots->size() == 1);

  const auto& outKept = (*out->roots)[0];
  REQUIRE(outKept);
  // A child WAS dropped, so this is a clone with a rebuilt payload vector --
  // the situation the survival claim is actually about.
  CHECK(outKept.get() != kept.get());
  CHECK(outKept->children.get() != kept->children.get());
  CHECK(outKept->name == "Kept");

  // Five of six payloads survive: the four unrendered ones and the mesh.
  REQUIRE(outKept->children);
  CHECK(outKept->children->size() == 5);
  check_four_survived(*outKept, splat, voxels, points, volume, "SceneGraphFilter");
  CHECK(payload_of<ossia::mesh_component_ptr>(*outKept).get() == mesh.get());
  CHECK(payloads_of<ossia::scene_node_ptr>(*outKept).empty());

  // The input is not mutated by the filter.
  CHECK(kept->children->size() == 6);
  check_four_survived(*kept, splat, voxels, points, volume, "filter input");
}

TEST_CASE(
    "P2-13 (a+b): merge -> filter -> flatten end to end",
    "[scene][merge][filter][flatten][payload][P2-13]")
{
  const auto splat = make_splat();
  const auto voxels = make_voxel_field();
  const auto points = make_point_cloud();
  const auto volume = make_volume();
  const auto mesh = make_triangle_mesh(0x33u);

  auto doomed = make_node("Doomed", 9);
  auto kept = make_node(
      "Kept", 1,
      payloads{splat, voxels, mesh, points, volume, ossia::scene_node_ptr{doomed}});
  auto other = make_node("Other", 2, payloads{make_triangle_mesh(0x44u)});

  const ossia::scene_spec inputs[2]{
      spec_of(make_state({kept}, 1)), spec_of(make_state({other}, 2))};
  const auto merged
      = ossia::merge_scenes(std::span<const ossia::scene_spec>{inputs, 2});
  REQUIRE(merged.state);

  Filter f;
  f.inputs.scene_in.scene.state = merged.state;
  f.inputs.mode.value = Filter::ByName;
  f.inputs.names.value = {"Kept"};
  f();

  const auto& filtered = f.outputs.scene_out.scene.state;
  REQUIRE(filtered);
  REQUIRE(filtered->roots);
  REQUIRE(filtered->roots->size() == 1); // "Other" filtered away

  const auto& outKept = (*filtered->roots)[0];
  REQUIRE(outKept);
  check_four_survived(
      *outKept, splat, voxels, points, volume, "merge -> filter");

  FlatScene flat;
  flattenScene(spec_of(filtered), flat, 1.f);

  // One mesh in, one draw out; the four unrendered payloads are still nobody's
  // draw call.
  REQUIRE(flat.draws.size() == 1);
  CHECK(flat.draws[0].stable_id == 0x33u);
  CHECK(flat.primitive_clouds.empty());
  CHECK(flat.instances.empty());
  CHECK(flat.scene_data.empty());
  CHECK(flat.lightArenaSlots.empty());
  CHECK(flat.cameras.empty());
  CHECK(flat.worldTransforms.empty());
}

// =============================================================================
// "AND SAY SO" -- the admission itself
// =============================================================================

TEST_CASE(
    "P2-13: SceneGPUState still admits these four kinds are not rendered",
    "[scene][flatten][payload][P2-13][source]")
{
  // The expectations above are pinned to a documented gap, not to a desired
  // behaviour: the day a renderer lands for one of these kinds, this guard
  // fires and tells whoever landed it to come re-derive the counter half.
  // Source-text only, so it SKIPs (never fails) when the tree is not at hand.
#ifndef GFX_SRC_DIR
  SKIP("GFX_SRC_DIR not defined: no source tree to read the admission from");
#else
  const std::string path = std::string(GFX_SRC_DIR) + "/Graph/SceneGPUState.cpp";
  std::ifstream in(path, std::ios::binary);
  if(!in.good())
    SKIP("cannot read " + path);

  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  // SceneGPUState.cpp:617-618, verbatim.
  const auto pos = src.find("not rendered yet");
  INFO("expected the FlattenVisitor::visitPayload admission at "
       "SceneGPUState.cpp:617-618");
  REQUIRE(pos != std::string::npos);

  const std::string admission = src.substr(pos > 120 ? pos - 120 : 0, 260);
  INFO("admission text: " << admission);
  CHECK(admission.find("gaussian_splat") != std::string::npos);
  CHECK(admission.find("voxel_field") != std::string::npos);
  CHECK(admission.find("point_cloud") != std::string::npos);
  CHECK(admission.find("volume") != std::string::npos);
  CHECK(admission.find("the types are transported") != std::string::npos);
#endif
}
