#pragma once
#include <Threedim/LiveLink/Protocol.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Scenes sent in native frames (LiveLink/Protocol.hpp: mesh, image,
// native_scene), built into the same scene_state GltfParser makes from a
// .glb, so that everything downstream of the live link, and the live link's
// own patches, removals and streamed values, cannot tell them apart.
namespace Threedim::LiveLink
{

// The meshes and images received, by key, ready to be shared by the nodes
// of any native scene that names them.
struct native_resources
{
  // Primitives without material (each node using a mesh sets its own), and
  // the mesh's morph targets as mesh_component holds them (MorphTargets.hpp).
  struct mesh
  {
    std::vector<ossia::mesh_primitive> primitives;
    ossia::small_vector<ossia::morph_target, 8> morph_targets;
  };
  std::unordered_map<std::string, mesh> meshes;
  std::unordered_map<std::string, std::shared_ptr<ossia::texture_source>> images;

  // A mesh sent again under the same key replaces the previous one, with new
  // primitive ids: the renderer uploads it anew.
  void add_mesh(
      const mesh_message& m,
      const std::vector<std::shared_ptr<std::vector<float>>>& tangents);
  void add_image(const image_message& m);

  // Drops what `used` does not name.
  void keep_only(
      const std::unordered_set<std::string>& used_meshes,
      const std::unordered_set<std::string>& used_images);
};

struct native_scene
{
  std::shared_ptr<ossia::scene_state> state; // null when the JSON is invalid
  std::vector<std::string> material_names;    // parallel to state->materials
  std::unordered_set<std::string> meshes, images; // the keys it uses
  std::string error;
};

// `seq` makes material ids unique per native scene, as GltfParser's path does
// per file: a material sent again is a new material to the renderer.
native_scene build_native_scene(
    std::string_view json, const native_resources& resources, uint64_t seq);

}
