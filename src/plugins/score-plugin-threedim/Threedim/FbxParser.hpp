#pragma once
#include <halp/file_port.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace Threedim
{

// Internal FBX parsing class — drives ufbx + builds an ossia::scene_spec
// out of an FBX file's bytes. Not a halp node in its own right (the
// user-facing entry point is AssetLoader). AssetLoader calls the static
// `ins::fbx_t::process` to obtain an apply-lambda, applies it against a
// throwaway FbxParser instance, then copies out `m_raw_state`.
class FbxParser
{
public:
  struct ins
  {
    struct fbx_t : halp::file_port<"FBX file">
    {
      static std::function<void(FbxParser&)> process(file_type data);
    } fbx;
  } inputs;

  void rebuild_scene();

  // -- Rich scene staging (drives rebuild_scene) -----------------------------
  // Built once per `process()` call. Lives on the execution thread; rebuilt
  // into ossia::scene_spec by rebuild_scene().
  struct ScenePart
  {
    // Per-attribute CPU buffers, one shared_ptr per stream. Each spans
    // vertex_count elements of the matching format. Empty pointers indicate
    // the attribute is absent on this part.
    std::shared_ptr<std::vector<float>> positions;  // 3 floats per vertex (always present)
    std::shared_ptr<std::vector<float>> normals;    // 3 floats per vertex
    std::shared_ptr<std::vector<float>> texcoords;  // 2 floats per vertex
    std::shared_ptr<std::vector<float>> colors;     // 4 floats per vertex (RGBA)
    std::shared_ptr<std::vector<float>> tangents;   // 4 floats per vertex

    // Skinning: top-4 joints + weights per vertex. joints holds uint16 per
    // component (4 per vertex); weights holds float (4 per vertex). Both
    // are populated iff the mesh has a skin deformer.
    std::shared_ptr<std::vector<uint16_t>> joints0;
    std::shared_ptr<std::vector<float>>    weights0;

    uint32_t vertex_count{0};

    // Index into FbxParser::m_materials. -1 = no material assigned.
    int material_index{-1};

    // Index into FbxParser::m_skeleton_joints_*, i.e. how many joints exist
    // — stored on the ScenePart to propagate skin_index to mesh_component.
    // 0 = no skin.
    int skin_joint_count{0};

    // Local-space AABB over `positions`. Computed once by extract_part
    // (or whoever fills ScenePart) and carried into mesh_primitive by
    // part_to_primitive. Empty aabb = "not yet computed"; downstream
    // GPU culling treats empty as infinite.
    ossia::aabb bounds{};
  };

  struct SceneNode
  {
    std::string name;
    ossia::scene_transform local_transform;  // node's local TRS
    int parent_index{-1};                    // index into m_scene_nodes (-1 = root)
    std::vector<ScenePart> parts;            // 0..N mesh parts (one per material)

    // Optional attached components — populated during extraction when the
    // ufbx_node carries them. `rebuild_scene` adds them as scene_payloads.
    std::shared_ptr<ossia::light_component> light;
    std::shared_ptr<ossia::camera_component> camera;
  };

  std::vector<SceneNode> m_scene_nodes;
  std::vector<std::shared_ptr<ossia::material_component>> m_materials;

  // One global skeleton built from all skin clusters encountered. Published
  // to scene_state.skeletons[0]; mesh_component::skin_index is 0 for any
  // mesh that uses skinning. Empty if the FBX has no skinning.
  std::shared_ptr<ossia::skeleton_component> m_skeleton;

  // Rich scene state emitted by rebuild_scene — full hierarchy with
  // materials, lights, cameras, skeletons. AssetLoader consumes this
  // via the apply-lambda returned by ins::fbx_t::process.
  std::shared_ptr<const ossia::scene_state> m_raw_state;
};

}
