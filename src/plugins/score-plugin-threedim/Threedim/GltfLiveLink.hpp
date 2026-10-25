#pragma once
#include <Threedim/LiveLink/NativeScene.hpp>
#include <Threedim/LiveLink/Server.hpp>
#include <Threedim/TinyObj.hpp>
#include <Threedim/TransformHelper.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Receives a scene streamed live from a DCC -- the Blender add-on,
// https://github.com/ossia/score-blender-livelink -- and publishes it like
// Asset Loader does.
//
// The node listens on a TCP port (protocol: LiveLink/Protocol.hpp). The sender
// pushes the scene when its structure changes: a complete binary glTF, a glTF
// patch of a few objects, removals, or native frames (vertex buffers, images
// and a JSON scene, LiveLink/NativeScene.hpp). Parsing a glTF and generating a
// native mesh's tangents happen on the worker, so the render thread never
// waits for them; changes are applied in the order they were sent.
//
// Everything else arrives as values that override the last scene, applied in
// one structure-sharing clone of the tree (as AnimationPlayer does for sampled
// channels), so a change costs no reparse:
//
//  - object transforms, by glTF node name: the first scene_transform of the
//    node is replaced;
//  - bone poses, by armature and bone name: the bone node's transform and the
//    skeleton joint the renderer skins with;
//  - morph target (shape key) weights: the node's mesh is re-blended on the
//    CPU (MorphTargets.hpp), since the renderer does not morph;
//  - visibility: scene_node::visible;
//  - light and camera parameters: the node's component is replaced;
//  - material factors, by material name: the material is replaced, in
//    scene_state::materials and in every primitive that uses it.
//
// The scene keeps being published after the DCC disconnects; a reconnecting
// sender sends a full scene first.
//
// Position / Rotation / Scale wrap the received scene at a root TRS, exactly
// as for Asset Loader.
class GltfLiveLink
{
public:
  halp_meta(name, "glTF Live Link")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "gltf_live_link")
  halp_meta(
      description,
      "Receive a 3D scene streamed live from Blender: geometry and materials "
      "as glTF, object transforms as they move.")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/gltf-live-link.html")
  halp_meta(uuid, "b9446bac-dbeb-4672-a9b6-db6e6cdaef2e")

  struct ins
  {
    halp::spinbox_i32<"Port", halp::irange{1, 65535, LiveLink::default_port}> port;
    // Loopback by default: anything on the network that can reach the port
    // can replace the scene. 0.0.0.0 accepts a DCC on another machine.
    halp::lineedit<"Bind address", "127.0.0.1"> bind;
    halp::toggle<"Listen", halp::toggle_setup{.init = true}> listen;

    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;

    // What of the received scene is used. Off, the renderer falls back to
    // what the rest of the score graph provides: its own camera and lights,
    // the default material, unskinned meshes in their bind pose.
    halp::toggle<"Cameras", halp::toggle_setup{.init = true}> cameras;
    halp::toggle<"Lights", halp::toggle_setup{.init = true}> lights;
    halp::toggle<"Materials", halp::toggle_setup{.init = true}> materials;
    halp::toggle<"Skinning", halp::toggle_setup{.init = true}> skinning;
    // Streamed values: off keeps what the last exported scene holds.
    halp::toggle<"Bone poses", halp::toggle_setup{.init = true}> bone_poses;
    halp::toggle<"Shape keys", halp::toggle_setup{.init = true}> shape_keys;
    halp::toggle<"World", halp::toggle_setup{.init = true}> world;
    // Position / Rotation / Scale place the scene in front of the cameras;
    // on, the cameras move with it (and the view does not change).
    halp::toggle<"Move cameras", halp::toggle_setup{.init = false}> move_cameras;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  GltfLiveLink();
  ~GltfLiveLink();

  void operator()();

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  // Parses a received .glb. GltfParser loads from a path, so the bytes are
  // written to a per-port file first: the same path on every update keeps
  // the parser's path-derived material ids, and with them downstream caches,
  // stable across updates.
  // A patch (a few objects, added to the scene) is parsed the same way.
  // A native mesh gets its tangents there too: MikkTSpace on a large mesh
  // takes seconds, which the server's thread, reading the sender's frames,
  // cannot spend without the sender timing out.
  struct glb_job
  {
    std::string glb;
    std::string path;
    uint64_t seq{};
    bool patch{};
  };
  // Shared: the task pool's jobs are small.
  struct mesh_job
  {
    std::shared_ptr<LiveLink::structural_message> message;
  };
  using job = std::variant<glb_job, mesh_job>;
  struct worker
  {
    std::function<void(job)> request;

    static std::function<void(GltfLiveLink&)> work(job j);
  } worker;

  // Applies a parsed scene. `material_names` parallels parsed->materials.
  // Public for the worker's closure and the tests.
  void set_scene(
      std::shared_ptr<const ossia::scene_state> parsed, uint64_t seq,
      std::vector<std::string> material_names = {});

  // A parse finished (state is null when it failed).
  void on_parsed(
      bool patch, uint64_t seq, std::shared_ptr<const ossia::scene_state> state,
      std::vector<std::string> material_names);

  // A native mesh's tangents are generated.
  void on_mesh_ready(LiveLink::structural_message m);

  // Adds a parsed patch's root nodes, materials and skeletons to the scene,
  // its node ids moved past the scene's.
  void add_patch(
      std::shared_ptr<const ossia::scene_state> patch, uint64_t seq,
      std::vector<std::string> material_names = {});

  // Removes the named nodes, their subtrees, and the materials left unused.
  void remove_nodes(const std::vector<std::string>& names, uint64_t seq);

  // Applies received values (and requests the parse of a received scene).
  // Public for the tests, which drive the node without a socket.
  void apply(LiveLink::inbox box);
  void set_transform(const LiveLink::transform_record& r, uint64_t seq);

  // Raw scene as parsed, then with the streamed values applied, then
  // wrapped in the Position / Rotation / Scale controls (published).
  std::shared_ptr<const ossia::scene_state> m_parsed_state;
  std::shared_ptr<const ossia::scene_state> m_overridden_state;
  std::shared_ptr<const ossia::scene_state> m_wrapped_state;

  // What the streamed values derived from m_parsed_state on the last
  // rebuild, with what each came from: the source component and the seq of
  // the record applied to it. The next rebuild hands back the same pointer
  // for an unchanged pair, so that a change to one material, light or shape
  // key does not make every other overridden one look new to the renderer
  // (its material and light slots and its mesh uploads are keyed by
  // pointer), nor re-blend every morphed mesh.
  template <typename Ptr>
  struct derived
  {
    const void* source{};
    uint64_t seq{};
    Ptr result;
  };
  struct derived_mesh
  {
    uint64_t weights_seq{};
    std::vector<const ossia::material_component*> materials;
    ossia::mesh_component_ptr result;
  };
  struct derived_cache
  {
    // By scene_node id; materials by index in scene_state::materials.
    using mesh_key = std::pair<uint64_t, const ossia::mesh_component*>;
    std::unordered_map<uint64_t, derived<ossia::light_component_ptr>> lights;
    std::unordered_map<uint64_t, derived<ossia::camera_component_ptr>> cameras;
    std::unordered_map<std::size_t, derived<ossia::material_component_ptr>> materials;
    std::map<mesh_key, derived_mesh> meshes;
  } m_derived;

private:
  void restart_server();
  void request(job j);
  void apply_pending();
  void rebuild_lookups();
  void receive();
  void rebuild_overridden_state();
  void apply_overrides();
  std::shared_ptr<const ossia::scene_state>
  filter_components(std::shared_ptr<const ossia::scene_state> st);
  void rebuild_wrapped_state();

  std::unique_ptr<LiveLink::server> m_server;
  std::string m_server_bind;
  int m_server_port{-1};
  bool m_server_failed{};
  int m_retry_countdown{};

  // Seq of the scene in m_parsed_state, and of the newest one handed to the
  // worker: a parse that finishes after a newer one was requested is dropped.
  uint64_t m_scene_seq{};
  uint64_t m_requested_scene_seq{};

  // Scenes, patches and removals received, waiting to be applied in seq
  // order: the first ones still being parsed hold back those after them.
  struct pending_change
  {
    uint64_t seq{};
    enum kind_t : uint8_t
    {
      scene,
      patch,
      remove,
      native // a mesh, an image or a native scene: `message`
    } kind{};
    bool ready{};
    std::shared_ptr<const ossia::scene_state> state;
    // Material names of a parsed scene or patch, node names of a removal.
    std::vector<std::string> names;
    LiveLink::structural_message message;

    // Older meshes and images are not made obsolete by a newer scene: the
    // native scene sent after them refers to them.
    bool is_resource() const noexcept { return kind == native && message.is_resource(); }
  };
  std::vector<pending_change> m_pending;
  LiveLink::native_resources m_native;
  void apply_native(LiveLink::structural_message& m);

  // Parallel to m_parsed_state->materials.
  std::vector<std::string> m_material_names;
  uint64_t m_max_node_id{};
  std::optional<LiveLink::stamped<LiveLink::environment_record>> m_environment;
  std::optional<LiveLink::stamped<LiveLink::node_name_record>> m_active_camera;

  // Streamed values, newest per key. Each keeps the seq it came with, so
  // that a new scene drops exactly those it already contains.
  LiveLink::latest<LiveLink::transform_record> m_transforms;
  LiveLink::latest<LiveLink::joint_record> m_joints;
  LiveLink::latest<LiveLink::weights_record> m_weights;
  LiveLink::latest<LiveLink::visibility_record> m_visibility;
  LiveLink::latest<LiveLink::light_record> m_lights;
  LiveLink::latest<LiveLink::camera_record> m_cameras;
  LiveLink::latest<LiveLink::material_record> m_materials;

  // `dirty` is the scene_port flag the value's change raises.
  template <typename Record>
  void store(
      LiveLink::latest<Record>& map, std::string key, uint64_t seq, Record r,
      uint8_t dirty);

  // Name lookups into m_parsed_state: glTF node name -> scene_node ids, a
  // bone (LiveLink::joint_key of any ancestor's name and the bone's name) ->
  // scene_node id, material name -> its indices in scene_state::materials (a
  // patch brings its own copies of the materials it uses).
  std::unordered_multimap<std::string, uint64_t> m_ids_by_name;
  std::unordered_map<std::string, uint64_t> m_ids_by_joint;
  std::unordered_multimap<std::string, std::size_t> m_material_index;
  bool m_overrides_dirty{};
  uint8_t m_overrides_dirt{};
  bool m_scene_dirty{};

  CachedTRS m_cached_xform;

  // The toggles the published scene was built with.
  struct import_options
  {
    bool cameras, lights, materials, skinning, bone_poses, shape_keys, world,
        move_cameras;
    bool operator==(const import_options&) const noexcept = default;
  };
  import_options current_options() const noexcept;
  std::optional<import_options> m_options;
  // Meshes without their materials (Materials off), by source mesh: kept
  // so that the same mesh comes out as the same pointer on every rebuild.
  std::unordered_map<const ossia::mesh_component*, ossia::mesh_component_ptr>
      m_unmaterialed;
  int64_t m_version_counter{0};

  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;
  ossia::gpu_slot_ref m_xform_ref{};
};

}
