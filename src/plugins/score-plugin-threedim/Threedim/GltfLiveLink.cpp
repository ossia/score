#include "GltfLiveLink.hpp"

#include "GltfParser.hpp"
#include "MorphTargets.hpp"

#include <Threedim/LiveLink/Components.hpp>
#include <Threedim/LiveLink/NativeScene.hpp>

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace Threedim
{

namespace
{
// Every few seconds, at the ~60 Hz the node is ticked at: how often a port
// that could not be bound is tried again (the program holding it may quit).
constexpr int bind_retry_ticks = 180;

// Fills the name lookups for `n` and its subtree. `ancestors` are the names
// from the root down to n's parent: a bone is found by its armature's name,
// which is any of them.
void collectNames(
    const ossia::scene_node_ptr& n, std::vector<std::string_view>& ancestors,
    std::unordered_multimap<std::string, uint64_t>& by_name,
    std::unordered_map<std::string, uint64_t>& by_joint)
{
  if(!n)
    return;
  if(!n->name.empty() && n->id.value != 0)
  {
    by_name.emplace(n->name, n->id.value);
    for(auto a : ancestors)
      by_joint.try_emplace(LiveLink::joint_key(a, n->name), n->id.value);
  }
  if(!n->children)
    return;
  ancestors.push_back(n->name);
  for(const auto& child : *n->children)
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
      collectNames(*sub, ancestors, by_name, by_joint);
  ancestors.pop_back();
}

// Everything streamed for one scene_node. Light, camera and weights keep
// their seq: with the source component, it identifies what they derive.
struct NodeOverride
{
  const LiveLink::transform_record* transform{};
  const LiveLink::visibility_record* visibility{};
  const LiveLink::stamped<LiveLink::light_record>* light{};
  const LiveLink::stamped<LiveLink::camera_record>* camera{};
  const LiveLink::stamped<LiveLink::weights_record>* weights{};
};
using NodeOverrides = std::unordered_map<uint64_t, NodeOverride>;
using MaterialRemap
    = std::unordered_map<const ossia::material_component*, ossia::material_component_ptr>;

void setTRS(ossia::scene_transform& t, const LiveLink::transform_record& r)
{
  std::memcpy(t.translation, r.translation, sizeof(t.translation));
  std::memcpy(t.rotation, r.rotation, sizeof(t.rotation));
  std::memcpy(t.scale, r.scale, sizeof(t.scale));
}

ossia::light_component_ptr
overrideLight(const ossia::light_component& src, const LiveLink::light_record& r)
{
  auto lc = std::make_shared<ossia::light_component>(src);
  LiveLink::apply_light(*lc, r);
  lc->dirty_index = src.dirty_index + 1;
  return lc;
}

ossia::camera_component_ptr
overrideCamera(const ossia::camera_component& src, const LiveLink::camera_record& r)
{
  auto cc = std::make_shared<ossia::camera_component>(src);
  LiveLink::apply_camera(*cc, r);
  cc->dirty_index = src.dirty_index + 1;
  return cc;
}

ossia::material_component_ptr
overrideMaterial(const ossia::material_component& src, const LiveLink::material_record& r)
{
  using M = LiveLink::material_record;
  auto mc = std::make_shared<ossia::material_component>(src);
  if(r.fields & M::base_color_rgb)
    std::memcpy(mc->base_color_factor, r.base_color, 3 * sizeof(float));
  if(r.fields & M::alpha)
    mc->base_color_factor[3] = r.base_color[3];
  if(r.fields & M::metallic_field)
    mc->metallic_factor = r.metallic;
  if(r.fields & M::roughness_field)
    mc->roughness_factor = r.roughness;
  if(r.fields & M::emissive)
  {
    std::memcpy(mc->emissive_factor, r.emissive_factor, sizeof(mc->emissive_factor));
    mc->emissive_strength = r.emissive_strength;
  }
  // A new material_component is what makes the preprocessor upload it: it
  // writes a loader material's GPU bytes on first sight of its pointer.
  mc->dirty_index = src.dirty_index + 1;
  return mc;
}

// Gives back what `cache` derived from the same source and seq on the
// previous rebuild, else makes it; either way records it in `fresh`, which
// becomes the cache of the next rebuild.
template <typename Map, typename Key, typename Make>
auto reuseOrMake(
    const Map& cache, Map& fresh, const Key& key, const void* source, uint64_t seq,
    Make&& make)
{
  auto& out = fresh[key];
  if(auto it = cache.find(key);
     it != cache.end() && it->second.source == source && it->second.seq == seq)
    out = it->second;
  else
    out = {source, seq, make()};
  return out.result;
}

// Same cloning rule as AnimationPlayer's: only the branches holding an
// overridden node, or a mesh using a replaced material, are cloned; the
// others are shared as-is so that downstream identity caches stay warm.
//
// The components the overrides derive (lights, cameras, meshes) come from
// `cache` when their source and override did not change since the previous
// rebuild, so that an unrelated change does not hand the renderer new
// pointers for them.
struct OverrideVisitor
{
  using Cache = GltfLiveLink::derived_cache;

  const NodeOverrides& nodes;
  const MaterialRemap& materials;
  const Cache& cache;
  Cache& fresh;

  bool usesRemappedMaterial(const ossia::mesh_component& mc) const noexcept
  {
    if(materials.empty())
      return false;
    for(const auto& p : mc.primitives)
      if(p.material && materials.contains(p.material.get()))
        return true;
    return false;
  }

  bool touches(const ossia::scene_node& n) const noexcept
  {
    if(nodes.contains(n.id.value))
      return true;
    if(!n.has_children())
      return false;
    for(const auto& child : *n.children)
    {
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
      {
        if(*sub && touches(**sub))
          return true;
      }
      else if(auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&child))
      {
        if(*mesh && usesRemappedMaterial(**mesh))
          return true;
      }
    }
    return false;
  }

  ossia::mesh_component_ptr
  mesh(const ossia::mesh_component_ptr& orig, uint64_t node_id, const NodeOverride* ov) const
  {
    const bool morph = ov && ov->weights && !orig->morph_targets.empty();
    if(!morph && !usesRemappedMaterial(*orig))
      return orig;

    const uint64_t weights_seq = morph ? ov->weights->seq : 0;
    std::vector<const ossia::material_component*> mats;
    mats.reserve(orig->primitives.size());
    for(const auto& p : orig->primitives)
    {
      auto it = p.material ? materials.find(p.material.get()) : materials.end();
      mats.push_back(it != materials.end() ? it->second.get() : p.material.get());
    }

    const Cache::mesh_key key{node_id, orig.get()};
    auto& out = fresh.meshes[key];
    if(auto it = cache.meshes.find(key); it != cache.meshes.end()
       && it->second.weights_seq == weights_seq && it->second.materials == mats)
    {
      out = it->second;
      return out.result;
    }

    std::shared_ptr<ossia::mesh_component> m
        = morph ? blendMorphTargets(*orig, ov->weights->record.weights)
                : std::make_shared<ossia::mesh_component>(*orig);
    for(auto& p : m->primitives)
      if(p.material)
        if(auto it = materials.find(p.material.get()); it != materials.end())
          p.material = it->second;
    m->dirty_index = orig->dirty_index + 1;
    out = {weights_seq, std::move(mats), std::move(m)};
    return out.result;
  }

  ossia::scene_node_ptr clone(const ossia::scene_node_ptr& orig) const
  {
    if(!orig || !touches(*orig))
      return orig;

    auto it = nodes.find(orig->id.value);
    const NodeOverride* ov = it != nodes.end() ? &it->second : nullptr;

    std::vector<ossia::scene_payload> children;
    if(orig->children)
      children.reserve(orig->children->size() + 1);

    bool replaced = false;
    if(orig->children)
    {
      for(const auto& payload : *orig->children)
      {
        if(auto* xf = ossia::get_if<ossia::scene_transform>(&payload))
        {
          if(ov && ov->transform && !replaced)
          {
            // The first one: GltfParser prepends one to every node.
            ossia::scene_transform t = *xf;
            setTRS(t, *ov->transform);
            children.push_back(t);
            replaced = true;
            continue;
          }
        }
        else if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&payload))
        {
          children.push_back(clone(*sub));
          continue;
        }
        else if(auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&payload))
        {
          children.push_back(*mesh ? this->mesh(*mesh, orig->id.value, ov) : *mesh);
          continue;
        }
        else if(auto* light = ossia::get_if<ossia::light_component_ptr>(&payload))
        {
          if(ov && ov->light && *light)
          {
            children.push_back(reuseOrMake(
                cache.lights, fresh.lights, orig->id.value, light->get(),
                ov->light->seq, [&] { return overrideLight(**light, ov->light->record); }));
            continue;
          }
        }
        else if(auto* cam = ossia::get_if<ossia::camera_component_ptr>(&payload))
        {
          if(ov && ov->camera && *cam)
          {
            children.push_back(reuseOrMake(
                cache.cameras, fresh.cameras, orig->id.value, cam->get(),
                ov->camera->seq, [&] { return overrideCamera(**cam, ov->camera->record); }));
            continue;
          }
        }
        children.push_back(payload);
      }
    }

    if(ov && ov->transform && !replaced)
    {
      ossia::scene_transform t{};
      setTRS(t, *ov->transform);
      children.insert(children.begin(), t);
    }

    auto node = std::make_shared<ossia::scene_node>(*orig);
    node->children = std::make_shared<const std::vector<ossia::scene_payload>>(
        std::move(children));
    if(ov && ov->visibility)
      node->visible = ov->visibility->visible != 0;
    node->dirty_index = orig->dirty_index + 1;
    return node;
  }
};

QString sceneFilePath(int port)
{
  return QDir::temp().filePath(QStringLiteral("score-livelink-%1.glb").arg(port));
}

QString patchFilePath(int port, uint64_t seq)
{
  return QDir::temp().filePath(
      QStringLiteral("score-livelink-%1-patch-%2.glb").arg(port).arg(seq));
}

template <typename Record>
void dropOlderThan(LiveLink::latest<Record>& map, uint64_t seq)
{
  std::erase_if(map, [seq](const auto& kv) { return kv.second.seq <= seq; });
}

template <typename Record>
void dropNamed(LiveLink::latest<Record>& map, const std::unordered_set<std::string>& names)
{
  std::erase_if(map, [&](const auto& kv) { return names.contains(kv.first); });
}

uint64_t maxNodeId(const ossia::scene_node_ptr& n)
{
  if(!n)
    return 0;
  uint64_t m = n->id.value;
  if(n->children)
    for(const auto& child : *n->children)
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
        m = std::max(m, maxNodeId(*sub));
  return m;
}

// The subtree with every node id moved up by `offset`, so that a patch's
// nodes (numbered from 1 by the parser, like any scene's) cannot collide with
// the scene's own.
ossia::scene_node_ptr offsetIds(const ossia::scene_node_ptr& n, uint64_t offset)
{
  if(!n)
    return n;
  auto out = std::make_shared<ossia::scene_node>(*n);
  if(out->id.value != 0)
    out->id.value += offset;
  if(n->children)
  {
    std::vector<ossia::scene_payload> children;
    children.reserve(n->children->size());
    for(const auto& child : *n->children)
    {
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
        children.push_back(offsetIds(*sub, offset));
      else
        children.push_back(child);
    }
    out->children
        = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(children));
  }
  return out;
}

// The subtree without the nodes named in `names` (and theirs); the same
// pointer when nothing in it is removed, null when n itself is.
ossia::scene_node_ptr
withoutNamed(const ossia::scene_node_ptr& n, const std::unordered_set<std::string>& names)
{
  if(!n || names.contains(n->name))
    return nullptr;
  if(!n->children)
    return n;
  bool changed = false;
  std::vector<ossia::scene_payload> children;
  children.reserve(n->children->size());
  for(const auto& child : *n->children)
  {
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
    {
      auto kept = withoutNamed(*sub, names);
      changed |= kept != *sub;
      if(kept)
        children.push_back(std::move(kept));
      continue;
    }
    children.push_back(child);
  }
  if(!changed)
    return n;
  auto out = std::make_shared<ossia::scene_node>(*n);
  out->children
      = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(children));
  out->dirty_index = n->dirty_index + 1;
  return out;
}

void collectMaterials(
    const ossia::scene_node_ptr& n,
    std::unordered_set<const ossia::material_component*>& out)
{
  if(!n || !n->children)
    return;
  for(const auto& child : *n->children)
  {
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&child))
      collectMaterials(*sub, out);
    else if(auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&child))
      if(*mesh)
        for(const auto& p : (*mesh)->primitives)
          if(p.material)
            out.insert(p.material.get());
  }
}

template <typename T>
std::shared_ptr<const std::vector<T>> concat(
    const std::shared_ptr<const std::vector<T>>& a,
    const std::shared_ptr<const std::vector<T>>& b)
{
  if(!b || b->empty())
    return a;
  auto out = std::make_shared<std::vector<T>>();
  if(a)
    *out = *a;
  out->insert(out->end(), b->begin(), b->end());
  return out;
}
}

GltfLiveLink::GltfLiveLink() = default;
GltfLiveLink::~GltfLiveLink() = default;

std::function<void(GltfLiveLink&)> GltfLiveLink::worker::work(job j)
{
  if(auto* m = std::get_if<mesh_job>(&j))
  {
    auto& msg = *m->message;
    if(msg.mesh)
      msg.tangents = LiveLink::generate_tangents(*msg.mesh);
    return [msg = std::move(m->message)](GltfLiveLink& self) {
      self.on_mesh_ready(std::move(*msg));
    };
  }
  auto& [glb, path, seq, patch] = std::get<glb_job>(j);

  // Workers run concurrently on the task pool, and every update of one port
  // goes through the same file: writing it and parsing it must not
  // interleave with another update's.
  static std::mutex file_mutex;
  std::lock_guard lock{file_mutex};

  bool written = false;
  {
    QFile f{QString::fromStdString(path)};
    written = f.open(QIODevice::WriteOnly | QIODevice::Truncate)
              && f.write(glb.data(), qint64(glb.size())) == qint64(glb.size());
    if(!written)
      qWarning() << "glTF Live Link: could not write" << f.fileName() << ":"
                 << f.errorString();
  }

  std::shared_ptr<const ossia::scene_state> state;
  std::vector<std::string> material_names;
  if(written)
  try
  {
    const halp::text_file_view tv{.bytes = glb, .filename = path};
    if(auto apply = GltfParser::ins::gltf_t::process(tv))
    {
      GltfParser parser;
      apply(parser);
      state = parser.m_raw_state;
      material_names = std::move(parser.m_material_names);
    }
  }
  catch(const std::exception& e)
  {
    qWarning() << "glTF Live Link: could not parse the received scene:"
               << e.what();
    state.reset();
  }

  // A patch's file is used once: its path, made unique by the seq, keeps its
  // materials' ids apart from the scene's and from other patches'.
  if(patch)
    QFile::remove(QString::fromStdString(path));

  if(!state && written)
    qWarning() << "glTF Live Link: could not parse the received"
               << (patch ? "patch" : "scene") << "(" << qint64(glb.size()) << "bytes)";

  // Reported even on failure: the change is still in the queue, and what
  // follows it waits for it.
  return [state = std::move(state), seq, patch,
          names = std::move(material_names)](GltfLiveLink& self) mutable {
    self.on_parsed(patch, seq, std::move(state), std::move(names));
  };
}

void GltfLiveLink::on_parsed(
    bool patch, uint64_t seq, std::shared_ptr<const ossia::scene_state> state,
    std::vector<std::string> material_names)
{
  const auto kind = patch ? pending_change::patch : pending_change::scene;
  for(auto& c : m_pending)
  {
    if(c.seq == seq && c.kind == kind)
    {
      c.ready = true;
      c.state = std::move(state);
      c.names = std::move(material_names);
      break;
    }
  }
  apply_pending();
}

void GltfLiveLink::request(job j)
{
  if(worker.request)
    worker.request(std::move(j));
  else if(auto done = worker::work(std::move(j)))
    done(*this); // No worker bound (a test, or a host without one).
}

void GltfLiveLink::on_mesh_ready(LiveLink::structural_message m)
{
  for(auto& c : m_pending)
  {
    if(c.seq == m.seq && c.kind == pending_change::native && !c.ready)
    {
      c.ready = true;
      c.message = std::move(m);
      break;
    }
  }
  apply_pending();
}

void GltfLiveLink::apply_pending()
{
  // In seq order, and only up to the first change still being parsed: a
  // removal must not overtake the patch that added what it removes.
  while(!m_pending.empty() && m_pending.front().ready)
  {
    auto c = std::move(m_pending.front());
    m_pending.erase(m_pending.begin());
    switch(c.kind)
    {
      case pending_change::scene:
        if(c.state)
          set_scene(std::move(c.state), c.seq, std::move(c.names));
        break;
      case pending_change::patch:
        if(c.state)
          add_patch(std::move(c.state), c.seq, std::move(c.names));
        break;
      case pending_change::remove:
        remove_nodes(c.names, c.seq);
        break;
      case pending_change::native:
        apply_native(c.message);
        break;
    }
  }
}

void GltfLiveLink::apply_native(LiveLink::structural_message& m)
{
  if(m.mesh)
  {
    m_native.add_mesh(*m.mesh, m.tangents);
    return;
  }
  if(m.image)
  {
    m_native.add_image(*m.image);
    return;
  }
  if(!m.scene)
    return;

  auto built = LiveLink::build_native_scene(m.scene->json, m_native, m.seq);
  if(!built.state)
  {
    qWarning() << "glTF Live Link: ignoring a native scene:"
               << QString::fromStdString(built.error);
    return;
  }
  if(m.scene->reset)
  {
    set_scene(std::move(built.state), m.seq, std::move(built.material_names));
    // What the new scene does not use is not referred to by anything left:
    // patches name the meshes they send along.
    m_native.keep_only(built.meshes, built.images);
  }
  else
  {
    add_patch(std::move(built.state), m.seq, std::move(built.material_names));
  }
}

void GltfLiveLink::rebuild_lookups()
{
  m_ids_by_name.clear();
  m_ids_by_joint.clear();
  m_max_node_id = 0;
  if(m_parsed_state && m_parsed_state->roots)
  {
    std::vector<std::string_view> ancestors;
    for(const auto& root : *m_parsed_state->roots)
    {
      collectNames(root, ancestors, m_ids_by_name, m_ids_by_joint);
      m_max_node_id = std::max(m_max_node_id, maxNodeId(root));
    }
  }

  m_material_index.clear();
  for(std::size_t i = 0; i < m_material_names.size(); ++i)
    if(!m_material_names[i].empty())
      m_material_index.emplace(m_material_names[i], i);
}

void GltfLiveLink::add_patch(
    std::shared_ptr<const ossia::scene_state> patch, uint64_t seq,
    std::vector<std::string> material_names)
{
  // Contained in a scene applied since, or with no scene to add to.
  if(!m_parsed_state || seq <= m_scene_seq || !patch)
    return;

  const uint64_t offset = m_max_node_id;
  auto st = std::make_shared<ossia::scene_state>(*m_parsed_state);

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(m_parsed_state->roots)
    *roots = *m_parsed_state->roots;
  if(patch->roots)
    for(const auto& r : *patch->roots)
      roots->push_back(offsetIds(r, offset));
  st->roots = std::move(roots);

  // Skinned meshes find their skeleton by joint node ids: they move too.
  if(patch->skeletons && !patch->skeletons->empty())
  {
    auto skels = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    if(m_parsed_state->skeletons)
      *skels = *m_parsed_state->skeletons;
    for(const auto& src : *patch->skeletons)
    {
      if(!src)
        continue;
      auto sk = std::make_shared<ossia::skeleton_component>(*src);
      for(auto& id : sk->joint_node_ids)
        if(id.value != 0)
          id.value += offset;
      skels->push_back(std::move(sk));
    }
    st->skeletons = std::move(skels);
  }

  // The patch brings its own copy of every material it uses, existing ones
  // included: streamed values go to every material of a name.
  const std::size_t old_count = m_parsed_state->materials ? m_parsed_state->materials->size() : 0;
  st->materials = concat(m_parsed_state->materials, patch->materials);
  m_material_names.resize(old_count);
  if(patch->materials)
  {
    material_names.resize(patch->materials->size());
    m_material_names.insert(
        m_material_names.end(), std::make_move_iterator(material_names.begin()),
        std::make_move_iterator(material_names.end()));
  }
  st->cameras = concat(m_parsed_state->cameras, patch->cameras);

  st->statistics.total_nodes += patch->statistics.total_nodes;
  st->statistics.total_triangles += patch->statistics.total_triangles;
  st->statistics.total_vertices += patch->statistics.total_vertices;
  st->statistics.total_lights += patch->statistics.total_lights;
  st->statistics.total_materials += patch->statistics.total_materials;
  st->version = ++m_version_counter;
  st->dirty_index = m_parsed_state->dirty_index + 1;

  m_parsed_state = std::move(st);
  rebuild_lookups();
  m_scene_dirty = true;
}

void GltfLiveLink::remove_nodes(const std::vector<std::string>& names, uint64_t seq)
{
  if(!m_parsed_state || seq <= m_scene_seq || names.empty())
    return;

  const std::unordered_set<std::string> removed(names.begin(), names.end());
  auto st = std::make_shared<ossia::scene_state>(*m_parsed_state);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(m_parsed_state->roots)
    for(const auto& r : *m_parsed_state->roots)
      if(auto kept = withoutNamed(r, removed))
        roots->push_back(std::move(kept));
  st->roots = roots;

  // Materials nothing draws with any more go too: re-exporting an object
  // alone brings new copies of its materials every time.
  if(m_parsed_state->materials)
  {
    std::unordered_set<const ossia::material_component*> used;
    for(const auto& r : *roots)
      collectMaterials(r, used);
    auto mats = std::make_shared<std::vector<ossia::material_component_ptr>>();
    std::vector<std::string> mat_names;
    const auto& old = *m_parsed_state->materials;
    for(std::size_t i = 0; i < old.size(); ++i)
    {
      if(old[i] && !used.contains(old[i].get()))
        continue;
      mats->push_back(old[i]);
      mat_names.push_back(i < m_material_names.size() ? m_material_names[i] : std::string{});
    }
    st->materials = std::move(mats);
    m_material_names = std::move(mat_names);
  }
  st->version = ++m_version_counter;
  st->dirty_index = m_parsed_state->dirty_index + 1;

  m_parsed_state = std::move(st);
  rebuild_lookups();

  // What was streamed for them. A node re-exported in a patch comes back
  // with its current values in it.
  dropNamed(m_transforms, removed);
  dropNamed(m_weights, removed);
  dropNamed(m_visibility, removed);
  dropNamed(m_lights, removed);
  dropNamed(m_cameras, removed);
  std::erase_if(m_joints, [&](const auto& kv) {
    return removed.contains(kv.second.record.armature);
  });
  m_scene_dirty = true;
}

void GltfLiveLink::set_scene(
    std::shared_ptr<const ossia::scene_state> parsed, uint64_t seq,
    std::vector<std::string> material_names)
{
  // An older parse finishing after a newer one was applied.
  if(m_parsed_state && seq < m_scene_seq)
    return;

  m_parsed_state = std::move(parsed);
  m_scene_seq = seq;

  // The scene already holds every value the sender had changed before it
  // exported it; only those that came after must survive.
  dropOlderThan(m_transforms, seq);
  dropOlderThan(m_joints, seq);
  dropOlderThan(m_weights, seq);
  dropOlderThan(m_visibility, seq);
  dropOlderThan(m_lights, seq);
  dropOlderThan(m_cameras, seq);
  dropOlderThan(m_materials, seq);

  m_material_names = std::move(material_names);
  rebuild_lookups();
  m_scene_dirty = true;
}

template <typename Record>
void GltfLiveLink::store(
    LiveLink::latest<Record>& map, std::string key, uint64_t seq, Record r,
    uint8_t dirty)
{
  if(seq <= m_scene_seq && m_parsed_state)
    return;

  auto [it, inserted] = map.try_emplace(std::move(key));
  if(!inserted && it->second.seq > seq)
    return;
  it->second.seq = seq;
  it->second.record = std::move(r);
  m_overrides_dirty = true;
  m_overrides_dirt |= dirty;
}

void GltfLiveLink::set_transform(const LiveLink::transform_record& r, uint64_t seq)
{
  store(m_transforms, r.name, seq, r, ossia::scene_port::dirty_animation);
}

void GltfLiveLink::restart_server()
{
  const bool want = inputs.listen.value;
  const int port = inputs.port.value;
  const std::string& bind = inputs.bind.value;

  if(!want)
  {
    if(m_server)
      qDebug() << "glTF Live Link: stopped listening on port" << m_server_port;
    m_server.reset();
    m_server_port = -1;
    m_server_failed = false;
    return;
  }

  const bool config_changed = port != m_server_port || bind != m_server_bind;
  if(m_server && !config_changed)
    return;

  // A failed bind is retried only now and then, not on every tick.
  if(m_server_failed && !config_changed && --m_retry_countdown > 0)
    return;

  m_server.reset();
  m_server_port = port;
  m_server_bind = bind;
  try
  {
    m_server = std::make_unique<LiveLink::server>(bind, uint16_t(port));
    if(m_server_failed)
      qDebug() << "glTF Live Link: now listening on"
               << QString::fromStdString(bind) << "port" << port;
    m_server_failed = false;
  }
  catch(const std::exception& e)
  {
    if(!m_server_failed || config_changed)
      qWarning() << "glTF Live Link: cannot listen on"
                 << QString::fromStdString(bind) << "port" << port << ":"
                 << e.what();
    m_server_failed = true;
    m_retry_countdown = bind_retry_ticks;
  }
}

void GltfLiveLink::receive()
{
  if(m_server)
    apply(m_server->take());
}

void GltfLiveLink::apply(LiveLink::inbox box)
{
  // Structural changes are queued in seq order; a scene supersedes whatever
  // was queued before it.
  std::vector<std::pair<std::string, pending_change>> to_parse;
  std::vector<mesh_job> to_mesh;
  if(box.scene)
  {
    auto& msg = *box.scene;
    m_requested_scene_seq = msg.seq;
    std::erase_if(m_pending, [&](const auto& c) { return c.seq < msg.seq && !c.is_resource(); });
    m_pending.push_back({.seq = msg.seq, .kind = pending_change::scene});
    to_parse.emplace_back(std::move(msg.glb), m_pending.back());
  }
  for(auto& msg : box.structural)
  {
    if(msg.seq < m_requested_scene_seq && !msg.is_resource())
      continue;
    if(msg.mesh || msg.image || msg.scene)
    {
      if(msg.is_reset())
      {
        m_requested_scene_seq = msg.seq;
        std::erase_if(m_pending, [&](const auto& c) {
          return c.seq < msg.seq && !c.is_resource();
        });
      }
      const auto seq = msg.seq;
      if(msg.mesh)
      {
        // Ready once the worker has its tangents; the message stays a
        // resource meanwhile, so that a reset does not drop it.
        m_pending.push_back(
            {.seq = seq, .kind = pending_change::native,
             .message = {.seq = seq, .type = msg.type, .mesh = LiveLink::mesh_message{}}});
        to_mesh.push_back(
            {std::make_shared<LiveLink::structural_message>(std::move(msg))});
        continue;
      }
      m_pending.push_back(
          {.seq = seq, .kind = pending_change::native, .ready = true,
           .message = std::move(msg)});
      continue;
    }
    if(msg.type == LiveLink::frame_type::patch)
    {
      m_pending.push_back({.seq = msg.seq, .kind = pending_change::patch});
      to_parse.emplace_back(std::move(msg.glb), m_pending.back());
    }
    else
    {
      m_pending.push_back(
          {.seq = msg.seq, .kind = pending_change::remove, .ready = true,
           .names = std::move(msg.names)});
    }
  }
  std::stable_sort(m_pending.begin(), m_pending.end(), [](const auto& a, const auto& b) {
    return a.seq < b.seq;
  });

  for(auto& [glb, c] : to_parse)
  {
    const bool patch = c.kind == pending_change::patch;
    auto path = (patch ? patchFilePath(m_server_port, c.seq)
                       : sceneFilePath(m_server_port))
                    .toStdString();
    request(glb_job{std::move(glb), std::move(path), c.seq, patch});
  }
  for(auto& m : to_mesh)
    request(std::move(m));
  apply_pending();

  // The world is not part of the glTF: its record outlives scenes. There is
  // one world at a time, whatever its name.
  for(auto& [key, v] : box.active_camera)
  {
    if(m_active_camera && m_active_camera->seq > v.seq)
      continue;
    m_active_camera = std::move(v);
    m_overrides_dirty = true;
    m_overrides_dirt |= ossia::scene_port::dirty_transform;
  }
  for(auto& [key, v] : box.environment)
  {
    if(m_environment && m_environment->seq > v.seq)
      continue;
    m_environment = std::move(v);
    m_overrides_dirty = true;
    m_overrides_dirt |= ossia::scene_port::dirty_environment;
  }

  for(auto& [key, v] : box.transforms)
    store(m_transforms, key, v.seq, std::move(v.record), ossia::scene_port::dirty_animation);
  for(auto& [key, v] : box.joints)
    store(m_joints, key, v.seq, std::move(v.record), ossia::scene_port::dirty_animation);
  for(auto& [key, v] : box.weights)
    store(m_weights, key, v.seq, std::move(v.record), ossia::scene_port::dirty_geometry);
  for(auto& [key, v] : box.visibility)
    store(m_visibility, key, v.seq, std::move(v.record), ossia::scene_port::dirty_structure);
  for(auto& [key, v] : box.lights)
    store(m_lights, key, v.seq, std::move(v.record), ossia::scene_port::dirty_lights);
  for(auto& [key, v] : box.cameras)
    store(m_cameras, key, v.seq, std::move(v.record), ossia::scene_port::dirty_transform);
  for(auto& [key, v] : box.materials)
    store(m_materials, key, v.seq, std::move(v.record), ossia::scene_port::dirty_materials);
}

void GltfLiveLink::rebuild_overridden_state()
{
  apply_overrides();
  m_overridden_state = filter_components(std::move(m_overridden_state));
}

GltfLiveLink::import_options GltfLiveLink::current_options() const noexcept
{
  return {
      inputs.cameras.value,    inputs.lights.value,     inputs.materials.value,
      inputs.skinning.value,   inputs.bone_poses.value, inputs.shape_keys.value,
      inputs.world.value,      inputs.move_cameras.value};
}

std::shared_ptr<const ossia::scene_state>
GltfLiveLink::filter_components(std::shared_ptr<const ossia::scene_state> st)
{
  const bool cameras = inputs.cameras.value;
  const bool lights = inputs.lights.value;
  const bool materials = inputs.materials.value;
  const bool skinning = inputs.skinning.value;
  if(!st || (cameras && lights && materials && skinning))
  {
    m_stripped.clear();
    return st;
  }

  // Skinning off: the meshes lose their skin as well, since the renderer
  // still skins a mesh by the skin it points at when scene_state does not
  // list it (flattenScene's FlatScene::skins).
  auto strips = [&](const ossia::mesh_component& mesh) {
    return !materials || (!skinning && mesh.skin);
  };
  std::unordered_map<const ossia::mesh_component*, ossia::mesh_component_ptr> stripped;
  auto strip = [&](const ossia::mesh_component_ptr& mesh) -> ossia::mesh_component_ptr {
    auto& out = stripped[mesh.get()];
    if(auto it = m_stripped.find(mesh.get()); it != m_stripped.end())
      return out = it->second;
    auto m = std::make_shared<ossia::mesh_component>(*mesh);
    if(!materials)
      for(auto& p : m->primitives)
        p.material.reset(); // the renderer's default material
    if(!skinning)
      m->skin.reset(); // drawn unskinned, in its bind pose
    m->dirty_index = mesh->dirty_index + 1;
    return out = std::move(m);
  };

  std::function<ossia::scene_node_ptr(const ossia::scene_node_ptr&)> filter
      = [&](const ossia::scene_node_ptr& n) -> ossia::scene_node_ptr {
    if(!n || !n->children)
      return n;
    bool changed = false;
    std::vector<ossia::scene_payload> children;
    children.reserve(n->children->size());
    for(const auto& c : *n->children)
    {
      if(!cameras && ossia::get_if<ossia::camera_component_ptr>(&c))
      {
        changed = true;
        continue;
      }
      if(!lights && ossia::get_if<ossia::light_component_ptr>(&c))
      {
        changed = true;
        continue;
      }
      if(auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&c);
         mesh && *mesh && strips(**mesh))
      {
        children.push_back(strip(*mesh));
        changed = true;
        continue;
      }
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&c))
      {
        auto f = filter(*sub);
        changed |= f != *sub;
        children.push_back(std::move(f));
        continue;
      }
      children.push_back(c);
    }
    if(!changed)
      return n;
    auto out = std::make_shared<ossia::scene_node>(*n);
    out->children
        = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(children));
    out->dirty_index = n->dirty_index + 1;
    return out;
  };

  auto out = std::make_shared<ossia::scene_state>(*st);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(st->roots)
    for(const auto& r : *st->roots)
      roots->push_back(filter(r));
  out->roots = std::move(roots);
  if(!materials)
    out->materials.reset();
  if(!skinning)
    out->skeletons.reset(); // no skeleton: the renderer draws the bind pose
  if(!cameras)
  {
    out->cameras.reset();
    out->active_camera_id = {};
  }
  out->version = ++m_version_counter;
  m_stripped = std::move(stripped);
  return out;
}

void GltfLiveLink::apply_overrides()
{
  m_overrides_dirty = false;
  m_overrides_dirt = 0;
  if(!m_parsed_state)
  {
    m_overridden_state.reset();
    return;
  }

  // Resolve every streamed value to the scene_node ids it applies to.
  NodeOverrides nodes;
  auto by_name = [&](const auto& map, auto field, auto get) {
    for(const auto& [name, v] : map)
    {
      auto [b, e] = m_ids_by_name.equal_range(name);
      for(auto it = b; it != e; ++it)
        nodes[it->second].*field = get(v);
    }
  };
  auto record = [](const auto& v) { return &v.record; };
  auto stamped = [](const auto& v) { return &v; };
  by_name(m_transforms, &NodeOverride::transform, record);
  by_name(m_visibility, &NodeOverride::visibility, record);
  by_name(m_lights, &NodeOverride::light, stamped);
  by_name(m_cameras, &NodeOverride::camera, stamped);
  if(inputs.shape_keys.value)
    by_name(m_weights, &NodeOverride::weights, stamped);

  // Whatever this rebuild derives goes to `fresh`; what it does not use any
  // more is dropped with the old cache.
  derived_cache fresh;

  // A bone is a node too, whose transform is the joint's local TRS.
  std::unordered_map<uint64_t, const LiveLink::transform_record*> joints;
  for(const auto& [key, v] : m_joints)
  {
    if(!inputs.bone_poses.value)
      break;
    if(auto it = m_ids_by_joint.find(key); it != m_ids_by_joint.end())
    {
      nodes[it->second].transform = &v.record.bone;
      joints[it->second] = &v.record.bone;
    }
  }

  // Materials are replaced wholesale, in the list and in the primitives.
  MaterialRemap remap;
  std::shared_ptr<std::vector<ossia::material_component_ptr>> materials;
  if(!m_materials.empty() && m_parsed_state->materials)
  {
    materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
        *m_parsed_state->materials);
    for(const auto& [name, v] : m_materials)
    {
      auto [b, e] = m_material_index.equal_range(name);
      for(auto it = b; it != e; ++it)
      {
        if(it->second >= materials->size())
          continue;
        auto& slot = (*materials)[it->second];
        if(!slot)
          continue;
        auto replaced = reuseOrMake(
            m_derived.materials, fresh.materials, it->second, slot.get(), v.seq,
            [&] { return overrideMaterial(*slot, v.record); });
        remap[slot.get()] = replaced;
        slot = std::move(replaced);
      }
    }
  }

  const bool world = m_environment && inputs.world.value;
  if(nodes.empty() && remap.empty() && !world && !m_active_camera)
  {
    m_derived = std::move(fresh);
    m_overridden_state = m_parsed_state;
    return;
  }

  OverrideVisitor vis{nodes, remap, m_derived, fresh};
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(m_parsed_state->roots)
  {
    roots->reserve(m_parsed_state->roots->size());
    for(const auto& r : *m_parsed_state->roots)
      roots->push_back(vis.clone(r));
  }
  m_derived = std::move(fresh);

  auto st = std::make_shared<ossia::scene_state>(*m_parsed_state);
  st->roots = std::move(roots);
  if(!remap.empty())
    st->materials = std::move(materials);
  if(world)
  {
    const auto& env = m_environment->record;
    std::memcpy(st->environment.ambient_color, env.ambient, sizeof(env.ambient));
    st->environment.ambient_intensity = env.strength;
    st->environment.params_set |= ossia::scene_environment::params_ambient;
  }
  if(m_active_camera)
  {
    // Unknown (or no) name: 0, the renderer's "first camera" default.
    auto it = m_ids_by_name.find(m_active_camera->record.name);
    st->active_camera_id.value = it != m_ids_by_name.end() ? it->second : 0;
  }
  st->version = ++m_version_counter;
  st->dirty_index = m_parsed_state->dirty_index + 1;

  // The renderer skins with skeleton_component::joints, by the node each
  // joint is backed by (joint_node_ids), as AnimationPlayer drives it.
  if(!joints.empty() && st->skeletons && !st->skeletons->empty())
  {
    auto skels = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    skels->reserve(st->skeletons->size());
    for(const auto& src : *st->skeletons)
    {
      if(!src)
      {
        skels->push_back(src);
        continue;
      }
      std::shared_ptr<ossia::skeleton_component> cloned;
      const std::size_t n = std::min(src->joints.size(), src->joint_node_ids.size());
      for(std::size_t j = 0; j < n; ++j)
      {
        auto it = joints.find(src->joint_node_ids[j].value);
        if(it == joints.end())
          continue;
        if(!cloned)
          cloned = std::make_shared<ossia::skeleton_component>(*src);
        const auto& b = *it->second;
        std::memcpy(cloned->joints[j].translation, b.translation, sizeof(b.translation));
        std::memcpy(cloned->joints[j].rotation, b.rotation, sizeof(b.rotation));
        std::memcpy(cloned->joints[j].scale, b.scale, sizeof(b.scale));
      }
      if(cloned)
      {
        cloned->dirty_index = st->version;
        skels->push_back(std::move(cloned));
      }
      else
      {
        skels->push_back(src);
      }
    }
    st->skeletons = std::move(skels);
  }

  m_overridden_state = std::move(st);
}

namespace
{
QMatrix4x4 trsMatrix(const ossia::scene_transform& t)
{
  QMatrix4x4 m;
  m.translate(t.translation[0], t.translation[1], t.translation[2]);
  m.rotate(QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
  m.scale(t.scale[0], t.scale[1], t.scale[2]);
  return m;
}

ossia::scene_transform toTRS(const QMatrix4x4& m)
{
  ossia::scene_transform t{};
  const QVector3D cols[3]{
      m.column(0).toVector3D(), m.column(1).toVector3D(), m.column(2).toVector3D()};
  QMatrix3x3 rot;
  for(int c = 0; c < 3; ++c)
  {
    const float len = cols[c].length();
    t.scale[c] = len;
    for(int r = 0; r < 3; ++r)
      rot(r, c) = len > 0.f ? cols[c][r] / len : 0.f;
  }
  const QVector4D tr = m.column(3);
  t.translation[0] = tr.x();
  t.translation[1] = tr.y();
  t.translation[2] = tr.z();
  const QQuaternion q = QQuaternion::fromRotationMatrix(rot).normalized();
  t.rotation[0] = q.x();
  t.rotation[1] = q.y();
  t.rotation[2] = q.z();
  t.rotation[3] = q.scalar();
  return t;
}

// The subtree without its camera components, which go to `cameras` as
// root nodes of their own, at their world transform (as the renderer's
// flattening composes it: a node's transforms apply, in order, to what
// follows them in the node) and with their node's name and id, which
// active_camera_id refers to. A node left with nothing but transforms is
// dropped (null), so that the name and id are the lifted camera's alone.
ossia::scene_node_ptr liftCameras(
    const ossia::scene_node_ptr& n, QMatrix4x4 world,
    std::vector<ossia::scene_node_ptr>& cameras)
{
  if(!n || !n->children)
    return n;
  bool changed = false;
  std::vector<ossia::scene_payload> children;
  children.reserve(n->children->size());
  for(const auto& c : *n->children)
  {
    if(auto* xf = ossia::get_if<ossia::scene_transform>(&c))
    {
      world = world * trsMatrix(*xf);
    }
    else if(auto* cam = ossia::get_if<ossia::camera_component_ptr>(&c))
    {
      auto lifted = std::make_shared<ossia::scene_node>();
      lifted->id = n->id;
      lifted->name = n->name;
      lifted->visible = n->visible;
      lifted->children = std::make_shared<const std::vector<ossia::scene_payload>>(
          std::vector<ossia::scene_payload>{toTRS(world), *cam});
      cameras.push_back(std::move(lifted));
      changed = true;
      continue;
    }
    else if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&c))
    {
      auto f = liftCameras(*sub, world, cameras);
      changed |= f != *sub;
      if(f)
        children.push_back(std::move(f));
      continue;
    }
    children.push_back(c);
  }
  if(!changed)
    return n;
  if(std::all_of(children.begin(), children.end(), [](const auto& c) {
       return ossia::get_if<ossia::scene_transform>(&c) != nullptr;
     }))
    return nullptr;
  auto out = std::make_shared<ossia::scene_node>(*n);
  out->children
      = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(children));
  return out;
}
}

void GltfLiveLink::rebuild_wrapped_state()
{
  if(!m_overridden_state || inputs.move_cameras.value)
  {
    m_wrapped_state = Threedim::wrapSceneWithTransform(
        m_overridden_state, inputs, m_cached_xform, m_version_counter, m_xform_ref);
    return;
  }

  // The controls place the content: the cameras it holds stay where they are
  // in the received scene, outside of the wrap, or the view would move with
  // the content and nothing would seem to change.
  std::vector<ossia::scene_node_ptr> cameras;
  auto content = std::make_shared<ossia::scene_state>(*m_overridden_state);
  if(m_overridden_state->roots)
  {
    auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    for(const auto& r : *m_overridden_state->roots)
      if(auto kept = liftCameras(r, QMatrix4x4{}, cameras))
        roots->push_back(std::move(kept));
    content->roots = std::move(roots);
  }
  auto wrapped = Threedim::wrapSceneWithTransform(
      std::shared_ptr<const ossia::scene_state>(std::move(content)), inputs,
      m_cached_xform, m_version_counter, m_xform_ref);
  if(cameras.empty())
  {
    m_wrapped_state = std::move(wrapped);
    return;
  }
  auto st = std::make_shared<ossia::scene_state>(*wrapped);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(*wrapped->roots);
  roots->insert(roots->end(), cameras.begin(), cameras.end());
  st->roots = std::move(roots);
  m_wrapped_state = std::move(st);
}

void GltfLiveLink::operator()()
{
  restart_server();
  receive();

  if(!m_parsed_state)
  {
    outputs.scene_out.scene.state = nullptr;
    outputs.scene_out.dirty = 0;
    return;
  }

  // A toggle changed: everything is built again from the received scene.
  if(const auto opts = current_options(); opts != m_options)
  {
    m_scene_dirty |= m_options.has_value();
    m_stripped.clear(); // stripped under the previous toggles
    m_options = opts;
  }

  uint8_t dirty = ossia::scene_port::dirty_transform;
  if(m_scene_dirty || !m_overridden_state)
  {
    m_scene_dirty = false;
    rebuild_overridden_state();
    rebuild_wrapped_state();
    dirty = ossia::scene_port::dirty_all;
  }
  else if(m_overrides_dirty)
  {
    dirty = m_overrides_dirt;
    rebuild_overridden_state();
    rebuild_wrapped_state();
  }
  else if(Threedim::transformChanged(inputs, m_cached_xform))
  {
    rebuild_wrapped_state();
  }

  outputs.scene_out.scene.state = m_wrapped_state;
  outputs.scene_out.dirty = dirty;
}

void GltfLiveLink::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!raw_transform_slot.valid())
  {
    raw_transform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        sizeof(score::gfx::RawLocalTransform));
    m_xform_ref = r.registry().toOssiaRef(raw_transform_slot);
    m_cached_xform.valid = false;
  }
  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform seed{};
    r.registry().updateSlot(res, raw_transform_slot, &seed, sizeof(seed));
  }
}

void GltfLiveLink::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_transform_slot.valid())
    return;

  score::gfx::RawLocalTransform xform{};
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;
  QQuaternion q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y, inputs.rotation.value.z);
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
}

void GltfLiveLink::release(score::gfx::RenderList& r)
{
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_xform_ref = {};
  // As in AssetLoader: the overridden and wrapped states embed registry refs.
  m_overridden_state.reset();
  m_wrapped_state.reset();
  m_derived = {};
}

}
