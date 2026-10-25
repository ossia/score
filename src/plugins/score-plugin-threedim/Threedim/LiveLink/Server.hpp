#pragma once
#include <Threedim/LiveLink/Protocol.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Threedim::LiveLink
{

// A bone of an armature; bone names are only unique within their armature.
struct joint_record
{
  std::string armature;
  transform_record bone;
};

template <typename Record>
struct stamped
{
  uint64_t seq{};
  Record record;
};

// Newest record of each key (a node, material or bone name).
template <typename Record>
using latest = std::unordered_map<std::string, stamped<Record>>;

// A patch, a removal, or a native frame: changes to the scene's structure,
// which, unlike values, must all be applied, in order.
struct structural_message
{
  uint64_t seq{};
  frame_type type{frame_type::patch};
  std::string glb;                // patch
  std::vector<std::string> names; // remove
  std::optional<mesh_message> mesh;          // mesh
  std::vector<std::shared_ptr<std::vector<float>>> tangents; // per primitive
  std::optional<image_message> image;        // image
  std::optional<native_scene_message> scene; // native_scene

  // A native scene with reset replaces the scene, as a scene frame does.
  bool is_reset() const noexcept { return scene && scene->reset; }
  // Kept by key, not shown: older ones are not made obsolete by a reset.
  bool is_resource() const noexcept { return mesh || image; }
};

// Mesh primitives get the tangents the renderer's normal mapping needs, as
// GltfParser gives meshes that have none (Threedim/TangentUtils.hpp).
// One per primitive of `m`, null where it lacks normals or texcoords.
std::vector<std::shared_ptr<std::vector<float>>> generate_tangents(const mesh_message& m);

// What arrived since the last take(), coalesced: only the newest scene, and
// only the newest record of each kind for each node, bone or material,
// survive. The receiving node may tick far slower than the sender sends (or
// not at all while nothing is rendered), and a stale intermediate value is of
// no use to it. Structural messages are kept in order, except those older
// than the newest scene, which contains them.
struct inbox
{
  std::optional<scene_message> scene;
  std::vector<structural_message> structural;
  latest<environment_record> environment;
  latest<node_name_record> active_camera;
  latest<transform_record> transforms;
  latest<joint_record> joints;
  latest<weights_record> weights;
  latest<visibility_record> visibility;
  latest<light_record> lights;
  latest<camera_record> cameras;
  latest<material_record> materials;

  // The sender's hello, as received (JSON); empty until one arrives.
  std::string peer_hello;

  // Bumped on every connection and disconnection, so the node can notice a
  // DCC that came back without polling the socket state.
  int64_t connection_events{};
  bool connected{};

  bool empty() const noexcept
  {
    return !scene && structural.empty() && environment.empty() && active_camera.empty()
           && transforms.empty()
           && joints.empty() && weights.empty() && visibility.empty() && lights.empty()
           && cameras.empty() && materials.empty();
  }
};

// Key of a joint in inbox::joints and wherever bones are looked up by name.
inline std::string joint_key(std::string_view armature, std::string_view bone)
{
  std::string k;
  k.reserve(armature.size() + 1 + bone.size());
  k.append(armature);
  k.push_back('\0');
  k.append(bone);
  return k;
}

// TCP listener for the live link, running its own network thread. The
// newest connection is the one that is read from: a DCC that reconnects
// replaces its own stale session instead of interleaving with it.
//
// Construction binds and listens, and throws (std::exception) when the
// address cannot be bound, e.g. a port already in use. Destruction stops the
// thread and closes every socket; it blocks until the thread has exited.
class server
{
public:
  server(const std::string& bind_address, uint16_t port);
  ~server();
  server(const server&) = delete;
  server& operator=(const server&) = delete;

  // Thread-safe. Moves out everything received so far.
  inbox take();

  uint16_t port() const noexcept;

private:
  struct impl;
  std::unique_ptr<impl> m;
};

}
