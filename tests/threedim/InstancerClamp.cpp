// L3 regression guard — split/threedim finding #2 (Instancer instance_count
// clamp). MUST-HAVE.
//
// With no Points geometry wired, effective_count was taken straight from the
// user's Count spinbox (up to 1,000,000) and stamped into
// instance_component::instance_count with NO relationship to the wired
// Transforms buffer's capacity. Downstream ScenePreprocessor then issued a
// strided GPU copy of `instance_count` regions out of the source QRhiBuffer
// with no capacity guard -> reads far past the buffer end -> Vulkan/RHI OOB
// copy / device-lost. The fix clamps instance_count to the tightest buffer
// capacity in rebuild().
//
// We construct the Instancer, wire a prototype mesh (so rebuild() publishes an
// instance_component) and a 100-mat4 (6400-byte) Transforms buffer, set
// Count=1,000,000, and assert the published instance_count is clamped to 100.
// The pre-fix engine publishes 1,000,000 (RED). Pure logic — no GPU needed;
// rebuild() only touches the Transforms buffer's byte_size, never the handle.

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
} // namespace

TEST_CASE(
    "Instancer clamps instance_count to the Transforms buffer capacity",
    "[threedim][instancer][f2]")
{
  Threedim::Instancer node;

  // Prototype scene: a single node carrying a mesh_component so findFirstMesh()
  // resolves a prototype and rebuild() emits an instance_component.
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

  // Transforms buffer: 100 mat4 instances = 100 * 64 = 6400 bytes. The handle
  // just needs to be non-null; rebuild() only consults byte_size / byte_offset.
  int dummy = 0;
  node.inputs.transforms.buffer.handle = &dummy;
  node.inputs.transforms.buffer.byte_size = 6400;
  node.inputs.transforms.buffer.byte_offset = 0;

  node.inputs.format.value = Threedim::Instancer::Mat4;
  node.inputs.count.value = 1000000;

  node.rebuild();

  const ossia::instance_component* inst = findInstance(node.m_wrapped_state);
  REQUIRE(inst != nullptr);
  // 6400 bytes / 64 bytes-per-mat4 = 100 instances; NOT the raw 1,000,000.
  CHECK(inst->instance_count == 100u);
}

TEST_CASE(
    "Instancer leaves an in-capacity Count untouched",
    "[threedim][instancer][f2]")
{
  Threedim::Instancer node;
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

  int dummy = 0;
  node.inputs.transforms.buffer.handle = &dummy;
  node.inputs.transforms.buffer.byte_size = 6400; // room for 100 mat4
  node.inputs.transforms.buffer.byte_offset = 0;
  node.inputs.format.value = Threedim::Instancer::Mat4;
  node.inputs.count.value = 50; // well within capacity

  node.rebuild();

  const ossia::instance_component* inst = findInstance(node.m_wrapped_state);
  REQUIRE(inst != nullptr);
  CHECK(inst->instance_count == 50u);
}
