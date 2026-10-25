#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Wire format of the glTF live link: a DCC (the Blender add-on,
// https://github.com/ossia/score-blender-livelink) connects over TCP and
// streams frames.
// That repository's PROTOCOL.md is the specification for writing a sender.
//
// Every integer and float is little-endian. A frame is
//
//   u32 length            byte count of what follows (type + body)
//   u8  type
//   u8  body[length - 1]
//
// A `str` below is a u16 byte count followed by that many UTF-8 bytes.
//
//   hello      (1)  UTF-8 JSON, see "Negotiation" below
//   scene      (2)  u64 seq, then a complete binary glTF (.glb)
//
// Every other type is u64 seq, u32 count, then count records:
//
//   transforms (3)  str node, f32 translation[3], f32 rotation[4] (x, y, z, w),
//                   f32 scale[3]: the node's local TRS
//   joints     (4)  str armature node, u32 bone count, then per bone a
//                   transforms record named after the bone: bone names are
//                   only unique within their armature
//   weights    (5)  str node, u16 n, f32 weights[n]: the morph target weights
//                   of the node's mesh
//   visibility (6)  str node, u8 visible
//   lights     (7)  str node, u8 type (0 directional, 1 point, 2 spot),
//                   f32 color[3], f32 intensity, f32 range (0: infinite),
//                   f32 inner cone, f32 outer cone (radians)
//   cameras    (8)  str node, u8 projection (0 perspective, 1 orthographic),
//                   f32 yfov (radians), f32 aspect (0: unset), f32 znear,
//                   f32 zfar, f32 xmag, f32 ymag
//   materials  (9)  str material, u32 fields, f32 base color[4],
//                   f32 metallic, f32 roughness, f32 emissive[3],
//                   f32 emissive strength; `fields` says which are meant
//   remove    (10)  str node: removes the node, and its subtree, from the
//                   scene
//   environment (12) str world, f32 ambient[3], f32 strength: the light
//                   the world sends from every direction (glTF has none)
//   active camera (13) str node: the camera node to render from, of the
//                   several a scene may hold (glTF has no notion of it)
//
// and, laid out like a scene frame:
//
//   patch     (11)  u64 seq, then a binary glTF of a few objects, whose root
//                   nodes are added to the scene's roots
//
// Patches and removals change the scene without sending all of it: an object
// added, deleted, or re-exported alone (a removal and a patch of the same
// node). They apply in seq order, after the scene frame they follow.
//
// All values are what the glTF of the last scene frame would hold for them:
// node names are glTF node names, units are glTF units, axes are glTF axes.
// A record overrides that value in the last scene, so a scene is sent only
// when geometry, materials' structure or the hierarchy change.
//
// `seq` is a single counter shared by every frame type, increasing on the
// sender. Comparing seqs is how the receiver drops overrides that a later
// scene already contains.
//
// Negotiation
// -----------
// The sender opens with a hello, {"protocol": 1, "app": ..., "features":
// [...]}, and the receiver answers with its own: "protocol" and "accepts",
// the names of the frame types it handles (accepted_frames below). A sender
// uses only frame types the receiver accepts. One that gets no answer takes
// the receiver for one that does not negotiate, and sends it scene frames
// only. glTF is the baseline every sender can fall back to: an exporter only
// has to produce a .glb to drive the receiver; every other frame type is an
// optimisation over re-sending it.
//
// Native frames
// -------------
// What a glTF export would carry, without the export: the sender's own
// evaluated meshes and its images' original files, and a JSON scene that
// refers to them by key. They apply in seq order with patches and removals.
//
//   mesh      (14)  u64 seq, str key, u16 primitive count, then per primitive
//                   u32 vertex count v, u32 index count i (0: not indexed),
//                   u8 attributes (1 normal, 2 texcoord0, 4 texcoord1,
//                   8 color0, 16 joints0 + weights0), u16 morph target count
//                   t, f32 position[3v], then the attributes present in that
//                   order (normal[3v], texcoord[2v], texcoord1[2v],
//                   color[4v], u16 joints[4v] then f32 weights[4v]), then
//                   u32 indices[i], then per morph target u8 (1 position, 2
//                   normal deltas) and those f32 deltas[3v]. glTF axes and
//                   UV convention; one primitive per material slot. Joints
//                   index the "joints" of the skin of the node using it.
//   image     (15)  u64 seq, str key, str MIME type, then the encoded image
//                   file (PNG, JPEG: whatever the receiver can decode)
//   native_scene (16) u64 seq, u8 reset, then UTF-8 JSON:
//                   {"nodes": [{"name", "parent" (index in "nodes", -1 for
//                     none: names repeat, bones), "t": [3], "r": [4] (xyzw),
//                     "s": [3], "visible", "mesh": key, "materials": [name
//                     per primitive], "light": [type, r, g, b, intensity,
//                     range, inner, outer], "camera": [projection, yfov,
//                     aspect, znear, zfar, xmag, ymag], "weights": [morph
//                     target weights], "skin": {"armature": node index, "joints":
//                     [bone node names, under the armature node],
//                     "inverse_bind": [16 per joint, column-major]}}, ...]
//                     (parents before children),
//                    "materials": {key: {"name", "base_color": [4], "metallic",
//                     "roughness", "emissive": [3], "emissive_strength",
//                     "alpha": "opaque" | "mask" | "blend", "alpha_cutoff",
//                     "double_sided", "unlit", and the KHR_materials_*
//                     factors: "ior", "transmission", "specular",
//                     "specular_color": [3], "clearcoat",
//                     "clearcoat_roughness", "sheen_color": [3],
//                     "sheen_roughness", "anisotropy", "anisotropy_rotation",
//                     "iridescence", "iridescence_ior",
//                     "iridescence_thickness": [min, max];
//                     "textures": {slot: {"image": key, "uv": set,
//                     "wrap": "repeat" | "clamp" | "mirror",
//                     "filter": "linear" | "nearest", "scale", "strength",
//                     "transform": {"offset": [2], "rotation", "scale": [2]}
//                     (KHR_texture_transform)}}}}}
//                   ("name" is the material's, for streamed values; nodes
//                   name materials by key), with slots as glTF's: base_color,
//                   transmission (R), specular (A), specular_color,
//                   clearcoat (R), clearcoat_roughness (G), clearcoat_normal,
//                   sheen_color, sheen_roughness (A), metallic_roughness (G roughness,
//                   B metallic), normal, occlusion (R), emissive. With reset,
//                   the nodes replace the whole scene, as a scene frame;
//                   without, they are added to it, as a patch. Meshes and
//                   images are kept by key until a reset no longer uses them.
namespace Threedim::LiveLink
{

constexpr uint16_t default_port = 9877;

// A glTF beyond this is refused: the receiver buffers a whole frame in memory
// before it can parse it, and a corrupted length must not allocate gigabytes.
constexpr uint32_t max_frame_size = 512u * 1024u * 1024u;

enum class frame_type : uint8_t
{
  hello = 1,
  scene = 2,
  transforms = 3,
  joints = 4,
  weights = 5,
  visibility = 6,
  lights = 7,
  cameras = 8,
  materials = 9,
  remove = 10,
  patch = 11,
  environment = 12,
  active_camera = 13,
  mesh = 14,
  image = 15,
  native_scene = 16,
};

constexpr int protocol_version = 1;

// The hello's "accepts": every frame type this receiver handles.
inline constexpr std::string_view accepted_frames[] = {
    "scene",       "patch",  "remove",        "transforms", "joints",
    "weights",     "visibility", "lights",    "cameras",    "materials",
    "environment", "active_camera", "mesh",   "image",      "native_scene"};

namespace detail
{
// memcpy keeps the reads alignment-safe; every platform score ships on is
// little-endian, which the static_assert pins.
static_assert(
    std::endian::native == std::endian::little,
    "the live link wire format is read in place as little-endian");

struct reader
{
  std::string_view in;

  template <typename T>
  bool operator()(T& out) noexcept
  {
    if(in.size() < sizeof(T))
      return false;
    std::memcpy(&out, in.data(), sizeof(T));
    in.remove_prefix(sizeof(T));
    return true;
  }

  template <typename T, std::size_t N>
  bool operator()(T (&out)[N]) noexcept
  {
    for(auto& v : out)
      if(!(*this)(v))
        return false;
    return true;
  }

  bool operator()(std::string& out)
  {
    uint16_t len{};
    if(!(*this)(len) || in.size() < len)
      return false;
    out.assign(in.data(), len);
    in.remove_prefix(len);
    return true;
  }
};

struct writer
{
  std::string& out;

  template <typename T>
  void operator()(const T& v)
  {
    char buf[sizeof(T)];
    std::memcpy(buf, &v, sizeof(T));
    out.append(buf, sizeof(T));
  }

  template <typename T, std::size_t N>
  void operator()(const T (&v)[N])
  {
    for(const auto& x : v)
      (*this)(x);
  }

  void operator()(const std::string& s)
  {
    const auto len = uint16_t(std::min<std::size_t>(s.size(), 0xFFFF));
    (*this)(len);
    out.append(s.data(), len);
  }
};
}

struct transform_record
{
  std::string name;
  float translation[3]{0.f, 0.f, 0.f};
  float rotation[4]{0.f, 0.f, 0.f, 1.f};
  float scale[3]{1.f, 1.f, 1.f};

  static constexpr std::size_t min_size = 2 + 10 * sizeof(float);
  bool read(detail::reader& r)
  {
    return r(name) && r(translation) && r(rotation) && r(scale);
  }
  void write(detail::writer& w) const
  {
    w(name), w(translation), w(rotation), w(scale);
  }
};

struct armature_pose
{
  std::string armature;
  std::vector<transform_record> bones;

  static constexpr std::size_t min_size = 2 + 4;
  bool read(detail::reader& r)
  {
    uint32_t count{};
    if(!r(armature) || !r(count) || count > r.in.size() / transform_record::min_size)
      return false;
    bones.resize(count);
    for(auto& b : bones)
      if(!b.read(r))
        return false;
    return true;
  }
  void write(detail::writer& w) const
  {
    w(armature), w(uint32_t(bones.size()));
    for(const auto& b : bones)
      b.write(w);
  }
};

struct weights_record
{
  std::string name;
  std::vector<float> weights;

  static constexpr std::size_t min_size = 2 + 2;
  bool read(detail::reader& r)
  {
    uint16_t n{};
    if(!r(name) || !r(n) || r.in.size() < n * sizeof(float))
      return false;
    weights.resize(n);
    for(float& f : weights)
      r(f);
    return true;
  }
  void write(detail::writer& w) const
  {
    w(name), w(uint16_t(weights.size()));
    for(float f : weights)
      w(f);
  }
};

struct visibility_record
{
  std::string name;
  uint8_t visible{1};

  static constexpr std::size_t min_size = 2 + 1;
  bool read(detail::reader& r) { return r(name) && r(visible); }
  void write(detail::writer& w) const { w(name), w(visible); }
};

struct light_record
{
  enum : uint8_t { directional = 0, point = 1, spot = 2 };

  std::string name;
  uint8_t type{point};
  float color[3]{1.f, 1.f, 1.f};
  float intensity{1.f};
  float range{0.f};
  float inner_cone{0.f};
  float outer_cone{0.7853981f};

  static constexpr std::size_t min_size = 2 + 1 + 7 * sizeof(float);
  bool read(detail::reader& r)
  {
    return r(name) && r(type) && r(color) && r(intensity) && r(range)
           && r(inner_cone) && r(outer_cone);
  }
  void write(detail::writer& w) const
  {
    w(name), w(type), w(color), w(intensity), w(range), w(inner_cone), w(outer_cone);
  }
};

struct camera_record
{
  enum : uint8_t { perspective = 0, orthographic = 1 };

  std::string name;
  uint8_t projection{perspective};
  float yfov{0.7853981f};
  float aspect{0.f};
  float znear{0.1f};
  float zfar{1000.f};
  float xmag{1.f};
  float ymag{1.f};

  static constexpr std::size_t min_size = 2 + 1 + 6 * sizeof(float);
  bool read(detail::reader& r)
  {
    return r(name) && r(projection) && r(yfov) && r(aspect) && r(znear) && r(zfar)
           && r(xmag) && r(ymag);
  }
  void write(detail::writer& w) const
  {
    w(name), w(projection), w(yfov), w(aspect), w(znear), w(zfar), w(xmag), w(ymag);
  }
};

struct material_record
{
  // Which of the values below are meant. A Blender input wired to a node
  // graph has no single value to stream; its field is left out.
  enum : uint32_t
  {
    base_color_rgb = 1u << 0,
    alpha = 1u << 1,
    metallic_field = 1u << 2,
    roughness_field = 1u << 3,
    emissive = 1u << 4, // emissive factor and strength together
  };

  std::string name;
  uint32_t fields{};
  float base_color[4]{1.f, 1.f, 1.f, 1.f};
  float metallic{1.f};
  float roughness{1.f};
  float emissive_factor[3]{0.f, 0.f, 0.f};
  float emissive_strength{1.f};

  static constexpr std::size_t min_size = 2 + 4 + 10 * sizeof(float);
  bool read(detail::reader& r)
  {
    return r(name) && r(fields) && r(base_color) && r(metallic) && r(roughness)
           && r(emissive_factor) && r(emissive_strength);
  }
  void write(detail::writer& w) const
  {
    w(name), w(fields), w(base_color), w(metallic), w(roughness), w(emissive_factor),
        w(emissive_strength);
  }
};

// A node name: what to remove, or which camera to render from.
struct node_name_record
{
  std::string name;

  static constexpr std::size_t min_size = 2;
  bool read(detail::reader& r) { return r(name); }
  void write(detail::writer& w) const { w(name); }
};

struct environment_record
{
  std::string name;
  float ambient[3]{0.f, 0.f, 0.f};
  float strength{1.f};

  static constexpr std::size_t min_size = 2 + 4 * sizeof(float);
  bool read(detail::reader& r) { return r(name) && r(ambient) && r(strength); }
  void write(detail::writer& w) const { w(name), w(ambient), w(strength); }
};

struct scene_message
{
  uint64_t seq{};
  std::string glb;
};

template <typename Record>
struct records_message
{
  uint64_t seq{};
  std::vector<Record> records;
};
using transforms_message = records_message<transform_record>;

// One primitive of a mesh frame. Absent attributes are null.
struct native_primitive
{
  uint32_t vertex_count{};
  std::shared_ptr<std::vector<float>> positions, normals, texcoords, texcoords1, colors;
  std::shared_ptr<std::vector<uint16_t>> joints; // 4 per vertex
  std::shared_ptr<std::vector<float>> weights;   // 4 per vertex
  std::shared_ptr<std::vector<uint32_t>> indices;
  struct morph
  {
    std::shared_ptr<std::vector<float>> positions, normals; // deltas
  };
  std::vector<morph> targets;
};

struct mesh_message
{
  uint64_t seq{};
  std::string key;
  std::vector<native_primitive> primitives;
};

struct image_message
{
  uint64_t seq{};
  std::string key;
  std::string mime_type;
  std::shared_ptr<std::vector<uint8_t>> bytes;
};

struct native_scene_message
{
  uint64_t seq{};
  bool reset{};
  std::string json;
};

// Body of a scene or patch frame (without the length and type bytes).
inline std::optional<scene_message> parse_scene(std::string_view body)
{
  scene_message m;
  detail::reader r{body};
  if(!r(m.seq))
    return std::nullopt;
  // A .glb starts with the "glTF" magic; anything else cannot be a binary
  // glTF and would only fail later, on a worker, with a vaguer message.
  if(r.in.size() < 12 || r.in.substr(0, 4) != "glTF")
    return std::nullopt;
  m.glb.assign(r.in.data(), r.in.size());
  return m;
}

namespace detail
{
template <typename T>
bool read_array(reader& r, std::size_t count, std::shared_ptr<std::vector<T>>& out)
{
  // Bounded by what the body holds before anything is allocated.
  if(count > r.in.size() / sizeof(T))
    return false;
  out = std::make_shared<std::vector<T>>(count);
  std::memcpy(out->data(), r.in.data(), count * sizeof(T));
  r.in.remove_prefix(count * sizeof(T));
  return true;
}
}

enum mesh_attribute : uint8_t
{
  mesh_normal = 1,
  mesh_texcoord0 = 2,
  mesh_texcoord1 = 4,
  mesh_color0 = 8,
  mesh_skin = 16,
};

inline std::optional<mesh_message> parse_mesh(std::string_view body)
{
  mesh_message m;
  detail::reader r{body};
  uint16_t count{};
  if(!r(m.seq) || !r(m.key) || !r(count))
    return std::nullopt;
  m.primitives.resize(count);
  for(auto& p : m.primitives)
  {
    uint32_t indices{};
    uint8_t attrs{};
    uint16_t targets{};
    if(!r(p.vertex_count) || !r(indices) || !r(attrs) || !r(targets))
      return std::nullopt;
    const std::size_t v = p.vertex_count;
    if(!detail::read_array(r, 3 * v, p.positions))
      return std::nullopt;
    if((attrs & mesh_normal) && !detail::read_array(r, 3 * v, p.normals))
      return std::nullopt;
    if((attrs & mesh_texcoord0) && !detail::read_array(r, 2 * v, p.texcoords))
      return std::nullopt;
    if((attrs & mesh_texcoord1) && !detail::read_array(r, 2 * v, p.texcoords1))
      return std::nullopt;
    if((attrs & mesh_color0) && !detail::read_array(r, 4 * v, p.colors))
      return std::nullopt;
    if((attrs & mesh_skin)
       && (!detail::read_array(r, 4 * v, p.joints) || !detail::read_array(r, 4 * v, p.weights)))
      return std::nullopt;
    if(indices > 0)
    {
      if(!detail::read_array(r, indices, p.indices))
        return std::nullopt;
      for(uint32_t i : *p.indices)
        if(i >= p.vertex_count)
          return std::nullopt;
    }
    else if(v % 3 != 0)
    {
      return std::nullopt; // not indexed: a triangle list
    }
    p.targets.resize(targets);
    for(auto& t : p.targets)
    {
      uint8_t which{};
      if(!r(which))
        return std::nullopt;
      if((which & 1) && !detail::read_array(r, 3 * v, t.positions))
        return std::nullopt;
      if((which & 2) && !detail::read_array(r, 3 * v, t.normals))
        return std::nullopt;
    }
  }
  if(!r.in.empty())
    return std::nullopt;
  return m;
}

inline std::optional<image_message> parse_image(std::string_view body)
{
  image_message m;
  detail::reader r{body};
  if(!r(m.seq) || !r(m.key) || !r(m.mime_type) || r.in.empty())
    return std::nullopt;
  m.bytes = std::make_shared<std::vector<uint8_t>>(
      (const uint8_t*)r.in.data(), (const uint8_t*)r.in.data() + r.in.size());
  return m;
}

inline std::optional<native_scene_message> parse_native_scene(std::string_view body)
{
  native_scene_message m;
  detail::reader r{body};
  uint8_t reset{};
  if(!r(m.seq) || !r(reset))
    return std::nullopt;
  m.reset = reset != 0;
  m.json.assign(r.in.data(), r.in.size());
  return m;
}

// Body of any records frame. All-or-nothing: a truncated or inconsistent
// record rejects the frame rather than applying part of it.
template <typename Record>
std::optional<records_message<Record>> parse_records(std::string_view body)
{
  records_message<Record> m;
  detail::reader r{body};
  uint32_t count{};
  if(!r(m.seq) || !r(count))
    return std::nullopt;

  // Bounds count before the resize below can be asked for an absurd size.
  if(count > r.in.size() / Record::min_size)
    return std::nullopt;

  m.records.resize(count);
  for(auto& rec : m.records)
    if(!rec.read(r))
      return std::nullopt;
  if(!r.in.empty())
    return std::nullopt;
  return m;
}

inline std::optional<transforms_message> parse_transforms(std::string_view body)
{
  return parse_records<transform_record>(body);
}

// Encoders, for the tests and for any C++ sender.
inline std::string encode_frame(frame_type t, std::string_view body)
{
  std::string out;
  out.reserve(5 + body.size());
  detail::writer{out}(uint32_t(body.size() + 1));
  out.push_back(char(t));
  out.append(body);
  return out;
}

inline std::string
encode_scene(uint64_t seq, std::string_view glb, frame_type t = frame_type::scene)
{
  std::string body;
  detail::writer{body}(seq);
  body.append(glb);
  return encode_frame(t, body);
}

inline std::string encode_mesh(
    uint64_t seq, const std::string& key, const std::vector<native_primitive>& prims)
{
  std::string body;
  detail::writer w{body};
  w(seq), w(key), w(uint16_t(prims.size()));
  auto put = [&](const auto& v) {
    if(v)
      body.append((const char*)v->data(), v->size() * sizeof((*v)[0]));
  };
  for(const auto& p : prims)
  {
    const uint8_t attrs = (p.normals ? mesh_normal : 0) | (p.texcoords ? mesh_texcoord0 : 0)
                          | (p.texcoords1 ? mesh_texcoord1 : 0) | (p.colors ? mesh_color0 : 0)
                          | (p.joints && p.weights ? mesh_skin : 0);
    w(p.vertex_count), w(uint32_t(p.indices ? p.indices->size() : 0)), w(attrs),
        w(uint16_t(p.targets.size()));
    put(p.positions), put(p.normals), put(p.texcoords), put(p.texcoords1), put(p.colors);
    if(p.joints && p.weights)
      put(p.joints), put(p.weights);
    put(p.indices);
    for(const auto& t : p.targets)
    {
      w(uint8_t((t.positions ? 1 : 0) | (t.normals ? 2 : 0)));
      put(t.positions), put(t.normals);
    }
  }
  return encode_frame(frame_type::mesh, body);
}

inline std::string encode_image(
    uint64_t seq, const std::string& key, const std::string& mime, std::string_view bytes)
{
  std::string body;
  detail::writer w{body};
  w(seq), w(key), w(mime);
  body.append(bytes);
  return encode_frame(frame_type::image, body);
}

inline std::string encode_native_scene(uint64_t seq, bool reset, std::string_view json)
{
  std::string body;
  detail::writer w{body};
  w(seq), w(uint8_t(reset ? 1 : 0));
  body.append(json);
  return encode_frame(frame_type::native_scene, body);
}

template <typename Record>
std::string encode_records(frame_type t, uint64_t seq, const std::vector<Record>& records)
{
  std::string body;
  detail::writer w{body};
  w(seq), w(uint32_t(records.size()));
  for(const auto& rec : records)
    rec.write(w);
  return encode_frame(t, body);
}

inline std::string
encode_transforms(uint64_t seq, const std::vector<transform_record>& records)
{
  return encode_records(frame_type::transforms, seq, records);
}

// Incremental frame splitter for a byte stream: feed() whatever the socket
// delivered, get called back once per complete frame. Once it has failed (a
// zero or oversized length), the stream cannot be resynchronised and the
// connection should be dropped.
class frame_decoder
{
public:
  template <typename F>
  bool feed(std::string_view bytes, F&& on_frame)
  {
    if(m_failed)
      return false;
    m_buffer.append(bytes);

    std::size_t pos = 0;
    while(m_buffer.size() - pos >= 4)
    {
      uint32_t len{};
      std::memcpy(&len, m_buffer.data() + pos, 4);
      if(len == 0 || len > max_frame_size)
      {
        m_failed = true;
        m_buffer.clear();
        return false;
      }
      if(m_buffer.size() - pos - 4 < len)
        break;

      const auto type = frame_type(uint8_t(m_buffer[pos + 4]));
      on_frame(type, std::string_view{m_buffer.data() + pos + 5, len - 1});
      pos += 4 + len;
    }
    m_buffer.erase(0, pos);
    return true;
  }

  bool failed() const noexcept { return m_failed; }
  std::size_t buffered() const noexcept { return m_buffer.size(); }

private:
  std::string m_buffer;
  bool m_failed{false};
};

}
