// L3 regression guard — split/threedim finding #4 (Instancer keeps a stale
// QRhiBuffer* for a SECONDARY Points attribute buffer -> GPU use-after-free).
//
// Instancer::rebuild() routes instance_transforms / instance_colors from
// arbitrary Points attribute buffers (buffers[1], buffers[2], ...) and stores
// those raw QRhiBuffer* inside the persistent m_wrapped_state. operator()()'s
// upstream-change detection, however, only cached the PRIMARY buffer
// (buffers[0]) handle plus the vertex count. A producer that reallocated a
// SECONDARY attribute buffer (a new QRhiBuffer for the transform_matrix /
// color0 attribute) while keeping buffers[0] and the vertex count identical,
// and without raising dirty_mesh, left upstream_changed false: rebuild() was
// skipped and m_wrapped_state kept handing ScenePreprocessor the OLD (now
// dangling) secondary handle -> GPU use-after-free / device crash.
//
// The fix folds EVERY Points buffer handle into an FNV-1a fingerprint compared
// each frame, so any secondary-buffer replacement forces a rebuild.
//
// This is a pure logic test (no GPU): we wire a Points mesh whose
// transform_matrix attribute resolves to buffers[1], tick once (rebuild routes
// instance_transforms at buffers[1]), then REPLACE buffers[1]'s handle while
// leaving buffers[0], the vertex count and dirty_mesh untouched, and tick
// again. Post-fix, the published instance_transforms now points at the NEW
// buffer and the state version has advanced; pre-fix it still points at the
// stale handle and the version is frozen (RED).

#include <Threedim/Instancer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
const ossia::instance_component*
findInstance(const std::shared_ptr<ossia::scene_state>& st)
{
  if(!st || !st->roots || st->roots->empty())
    return nullptr;
  const auto& n0 = (*st->roots)[0];
  if(!n0 || !n0->children)
    return nullptr;
  for(const auto& p : *n0->children)
    if(const auto* inst = ossia::get_if<ossia::instance_component_ptr>(&p))
      if(*inst)
        return inst->get();
  return nullptr;
}

// native_handle currently backing a wrapped instance-attribute buffer.
void* handleOf(const ossia::buffer_resource_ptr& r)
{
  if(!r)
    return nullptr;
  const auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&r->resource);
  return gpu ? gpu->native_handle : nullptr;
}
} // namespace

TEST_CASE(
    "Instancer rebuilds when a secondary Points buffer handle changes",
    "[threedim][instancer][f4]")
{
  Threedim::Instancer node;

  // Prototype scene so findFirstMesh() resolves a prototype and rebuild()
  // emits an instance_component.
  {
    auto proto = std::make_shared<ossia::mesh_component>();
    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(ossia::mesh_component_ptr(std::move(proto)));
    auto root = std::make_shared<ossia::scene_node>();
    root->children = std::move(children);
    auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    roots->push_back(std::move(root));
    auto st = std::make_shared<ossia::scene_state>();
    st->roots = std::move(roots);
    node.inputs.scene_in.scene.state = std::move(st);
  }

  // Points cloud: buffers[0] = positions (primary), buffers[1] =
  // transform_matrix (secondary). input[i] -> buffers[i]; the
  // transform_matrix attribute binds to input[1] -> buffers[1], so
  // instance_transforms is routed out of the SECONDARY buffer.
  int posBuf = 0;      // primary, never changes
  int xformBuf0 = 0;   // secondary, original allocation
  int xformBuf1 = 0;   // secondary, reallocation

  auto& mesh = node.inputs.points.mesh;
  mesh.buffers.resize(2);
  mesh.buffers[0].handle = &posBuf;
  mesh.buffers[0].byte_size = 100 * 12; // 100 vec3 positions
  mesh.buffers[1].handle = &xformBuf0;
  mesh.buffers[1].byte_size = 100 * 64; // 100 mat4 == room for 100 instances

  mesh.input.resize(2);
  mesh.input[0].buffer = 0;
  mesh.input[0].byte_offset = 0;
  mesh.input[1].buffer = 1;
  mesh.input[1].byte_offset = 0;

  mesh.attributes.resize(2);
  mesh.attributes[0].binding = 0;
  mesh.attributes[0].semantic = halp::attribute_semantic::position;
  mesh.attributes[0].byte_offset = 0;
  mesh.attributes[1].binding = 1;
  mesh.attributes[1].semantic = halp::attribute_semantic::transform_matrix;
  mesh.attributes[1].byte_offset = 0;

  mesh.vertices = 100;
  node.inputs.points.dirty_mesh = false;

  // First tick: rebuild routes instance_transforms at buffers[1] == &xformBuf0.
  node();
  const ossia::instance_component* inst0 = findInstance(node.m_wrapped_state);
  REQUIRE(inst0 != nullptr);
  REQUIRE(handleOf(inst0->instance_transforms) == &xformBuf0);
  const int64_t version0 = node.m_wrapped_state->version;

  // The producer reallocates ONLY the secondary buffer (new QRhiBuffer for the
  // transform_matrix attribute). buffers[0], the vertex count, and dirty_mesh
  // are all left untouched — exactly the case the primary-only cache missed.
  mesh.buffers[1].handle = &xformBuf1;
  node.inputs.points.dirty_mesh = false;

  // Second tick.
  node();
  const ossia::instance_component* inst1 = findInstance(node.m_wrapped_state);
  REQUIRE(inst1 != nullptr);

  // The fix: the fingerprint over ALL Points buffers changed, forcing a
  // rebuild that re-routes instance_transforms onto the NEW buffer and bumps
  // the state version. Pre-fix, both assertions fail (stale handle, frozen
  // version).
  CHECK(handleOf(inst1->instance_transforms) == &xformBuf1);
  CHECK(node.m_wrapped_state->version > version0);
}
