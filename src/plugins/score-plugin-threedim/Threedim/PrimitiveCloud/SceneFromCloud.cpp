#include "SceneFromCloud.hpp"

#include <cstdint>
#include <string>

namespace Threedim::PrimitiveCloud
{

std::shared_ptr<ossia::scene_state> sceneStateFromCloud(
    ossia::primitive_cloud_component_ptr cloud,
    std::string_view source_label)
{
  if(!cloud)
    return nullptr;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::primitive_cloud_component_ptr{cloud});

  auto node = std::make_shared<ossia::scene_node>();
  // Stable id keyed on the cloud's raw_data pointer. Required by the
  // registry's slot allocator: a 0 id is uncacheable and the cloud
  // disappears between frames.
  uint64_t key = 0;
  if(cloud->raw_data)
    key = (uint64_t)((uintptr_t)cloud->raw_data.get());
  if(key == 0)
    key = (uint64_t)((uintptr_t)cloud.get());
  node->id.value = key;
  node->name = source_label.empty()
                   ? std::string("primitive_cloud")
                   : std::string(source_label);
  node->children = std::move(children);

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(node));

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(roots);
  state->version = 1;
  state->dirty_index = 1;
  return state;
}

} // namespace Threedim::PrimitiveCloud
