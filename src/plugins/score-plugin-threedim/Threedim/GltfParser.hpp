#pragma once
#include <halp/file_port.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace Threedim
{

// Internal glTF 2.0 parsing class — uses fastgltf + simdjson to parse
// .gltf / .glb. Not a halp node itself; AssetLoader is the user-facing
// entry point. AssetLoader calls the static `ins::gltf_t::process` to
// obtain an apply-lambda, applies it against a throwaway GltfParser
// instance, then copies out `m_raw_state`.
class GltfParser
{
public:
  struct ins
  {
    struct gltf_t : halp::file_port<"glTF file">
    {
      static std::function<void(GltfParser&)> process(file_type data);
    } gltf;
  } inputs;

  void rebuild_scene();

  // Rich scene staging. Same schema as FbxParser (kept in sync so a future
  // shared helper can consume both).
  struct ScenePart
  {
    std::shared_ptr<std::vector<float>> positions;
    std::shared_ptr<std::vector<float>> normals;
    std::shared_ptr<std::vector<float>> texcoords;
    std::shared_ptr<std::vector<float>> texcoords1;  // glTF TEXCOORD_1
    std::shared_ptr<std::vector<float>> colors;
    std::shared_ptr<std::vector<float>> tangents;
    // Skinning attributes (present when the primitive references a skin).
    // joints: uvec4 bone indices packed as uint32 x 4 per vertex.
    // weights: vec4 bone weights per vertex.
    std::shared_ptr<std::vector<uint32_t>> joints0;
    std::shared_ptr<std::vector<float>> weights0;
    std::shared_ptr<std::vector<uint32_t>> indices; // optional
    uint32_t vertex_count{0};
    uint32_t index_count{0};
    int material_index{-1};
    // Local-space AABB over the POSITION stream. Populated by
    // extract_primitive from the glTF POSITION accessor's min/max when
    // present (spec-required but optionally trusted); otherwise derived
    // by walking positions. Empty aabb = "not yet computed"; downstream
    // GPU culling treats empty as infinite (never cull).
    ossia::aabb bounds{};
    // KHR_materials_variants: per-variant material override index.
    // Indexed by variant (parallel to scene_state::variant_names).
    // -1 at a position = "no override for this variant, use default".
    std::vector<int> variant_material_indices;
  };

  struct SceneNode
  {
    std::string name;
    ossia::scene_transform local_transform;
    int parent_index{-1};
    std::vector<ScenePart> parts;
    std::shared_ptr<ossia::light_component> light;
    std::shared_ptr<ossia::camera_component> camera;
    // glTF skin index. -1 = not skinned. When ≥ 0, the mesh_component
    // emitted from this node's parts gets stamped with skin_index so
    // ScenePreprocessor binds the matching skeleton's joint_matrices
    // auxiliary buffer for the skinning vertex shader to read.
    int32_t skin_index{-1};
    // Stable node_id, derived from the glTF node index + 1. Used by
    // AnimationPlayer to find the node via channel.target_node_id, and
    // by skeleton_component::joint_node_ids to resolve each joint to
    // its node's world transform.
    std::uint64_t stable_id{0};
  };

  std::vector<SceneNode> m_scene_nodes;
  std::vector<std::shared_ptr<ossia::material_component>> m_materials;
  std::vector<std::shared_ptr<ossia::skeleton_component>> m_skeletons;
  // KHR_materials_variants: names (UI-facing) declared at asset scope.
  // Parallel to mesh_primitive::material_variants and
  // scene_state::active_variant_index.
  std::vector<std::string> m_variant_names;

  // Rich scene state emitted by rebuild_scene — full hierarchy with
  // materials, lights, cameras, skeletons. AssetLoader consumes this
  // via the apply-lambda returned by ins::gltf_t::process.
  std::shared_ptr<const ossia::scene_state> m_raw_state;
};

}
