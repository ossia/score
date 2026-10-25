#include "NativeScene.hpp"

#include <Threedim/LiveLink/Components.hpp>
#include <Threedim/MorphTargets.hpp>

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <ossia/detail/hash.hpp>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Threedim::LiveLink
{

namespace
{
template <typename T>
ossia::buffer_resource_ptr make_buffer(
    const std::shared_ptr<std::vector<T>>& v, ossia::buffer_data::usage usage)
{
  if(!v || v->empty())
    return {};
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(v, v->data());
  bd.byte_size = int64_t(v->size() * sizeof(T));
  bd.usage_hint = usage;
  br->resource = std::move(bd);
  br->dirty_index = 1;
  return br;
}

// Same layout as GltfParser's part_to_primitive: one buffer per attribute.
ossia::mesh_primitive
to_primitive(const native_primitive& p, const std::shared_ptr<std::vector<float>>& tangents)
{
  ossia::mesh_primitive mp;
  mp.stable_id = ossia::mint_stable_id();
  mp.topology = ossia::primitive_topology::triangles;
  mp.index_type = p.indices ? ossia::index_format::uint32 : ossia::index_format::none;
  mp.vertex_count = p.vertex_count;
  mp.index_count = p.indices ? uint32_t(p.indices->size()) : 0;
  if(p.positions)
    mp.bounds = ossia::compute_aabb_from_positions(p.positions->data(), p.vertex_count);

  uint32_t bi = 0;
  auto add = [&](const std::shared_ptr<std::vector<float>>& buf, int floats,
                 ossia::attribute_semantic sem, ossia::vertex_format fmt) {
    if(!buf || buf->empty())
      return;
    mp.vertex_buffers.push_back(make_buffer(buf, ossia::buffer_data::usage::vertex_buffer));
    ossia::vertex_attribute attr;
    attr.semantic = sem;
    attr.format = fmt;
    attr.buffer_index = bi++;
    attr.byte_offset = 0;
    attr.byte_stride = uint32_t(floats) * sizeof(float);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
  };
  add(p.positions, 3, ossia::attribute_semantic::position, ossia::vertex_format::float3);
  add(p.normals, 3, ossia::attribute_semantic::normal, ossia::vertex_format::float3);
  add(p.texcoords, 2, ossia::attribute_semantic::texcoord0, ossia::vertex_format::float2);
  add(p.texcoords1, 2, ossia::attribute_semantic::texcoord1, ossia::vertex_format::float2);
  add(p.colors, 4, ossia::attribute_semantic::color0, ossia::vertex_format::float4);
  add(tangents, 4, ossia::attribute_semantic::tangent, ossia::vertex_format::float4);
  if(p.joints && p.weights)
  {
    // As GltfParser: uvec4 joints, vec4 weights.
    auto joints = std::make_shared<std::vector<uint32_t>>(p.joints->begin(), p.joints->end());
    mp.vertex_buffers.push_back(make_buffer(joints, ossia::buffer_data::usage::vertex_buffer));
    ossia::vertex_attribute attr;
    attr.semantic = ossia::attribute_semantic::joints0;
    attr.format = ossia::vertex_format::uint32x4;
    attr.buffer_index = bi++;
    attr.byte_offset = 0;
    attr.byte_stride = 4 * sizeof(uint32_t);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
    add(p.weights, 4, ossia::attribute_semantic::weights0, ossia::vertex_format::float4);
  }
  if(p.indices)
    mp.index_buffer = make_buffer(p.indices, ossia::buffer_data::usage::index_buffer);
  return mp;
}

float num(const QJsonValue& v, float def)
{
  return v.isDouble() ? float(v.toDouble()) : def;
}

template <std::size_t N>
void nums(const QJsonValue& v, float (&out)[N])
{
  const auto a = v.toArray();
  for(std::size_t i = 0; i < N && i < std::size_t(a.size()); ++i)
    out[i] = float(a[qsizetype(i)].toDouble());
}

std::string str(const QJsonValue& v)
{
  return v.toString().toStdString();
}

QMatrix4x4 trs_matrix(const ossia::scene_transform& t)
{
  QMatrix4x4 m;
  m.translate(t.translation[0], t.translation[1], t.translation[2]);
  m.rotate(QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
  m.scale(t.scale[0], t.scale[1], t.scale[2]);
  return m;
}

void decompose(const QMatrix4x4& m, float (&t)[3], float (&r)[4], float (&s)[3])
{
  QMatrix3x3 rot;
  for(int c = 0; c < 3; ++c)
  {
    const auto col = m.column(c).toVector3D();
    s[c] = col.length();
    for(int k = 0; k < 3; ++k)
      rot(k, c) = s[c] > 0.f ? col[k] / s[c] : 0.f;
  }
  const auto tr = m.column(3);
  t[0] = tr.x(), t[1] = tr.y(), t[2] = tr.z();
  const auto q = QQuaternion::fromRotationMatrix(rot).normalized();
  r[0] = q.x(), r[1] = q.y(), r[2] = q.z(), r[3] = q.scalar();
}

std::shared_ptr<ossia::material_component> to_material(
    const std::string& name, const QJsonObject& m, const native_resources& res,
    std::unordered_set<std::string>& images_used, uint64_t seq)
{
  auto mc = std::make_shared<ossia::material_component>();
  mc->tag = name;
  nums(m["base_color"], mc->base_color_factor);
  mc->metallic_factor = num(m["metallic"], 1.f);
  mc->roughness_factor = num(m["roughness"], 1.f);
  nums(m["emissive"], mc->emissive_factor);
  mc->emissive_strength = num(m["emissive_strength"], 1.f);
  const auto alpha = m["alpha"].toString();
  mc->alpha = alpha == "blend"  ? ossia::alpha_mode::blend
              : alpha == "mask" ? ossia::alpha_mode::mask
                                : ossia::alpha_mode::opaque_;
  mc->alpha_cutoff = num(m["alpha_cutoff"], 0.5f);
  mc->double_sided = m["double_sided"].toBool(false);
  // As GltfParser's: unique per file there, per native scene here.
  mc->stable_id = ossia::scene_node_id::from_parent(
                      ossia::scene_node_id::from_path("livelink-native"),
                      name + "/" + std::to_string(seq))
                      .value;

  const auto textures = m["textures"].toObject();
  // The slot's texture info when it names a received image, else empty.
  auto fill = [&](ossia::texture_ref& tr, const char* slot) -> QJsonObject {
    const auto t = textures[slot].toObject();
    if(t.isEmpty())
      return {};
    const auto key = str(t["image"]);
    auto it = res.images.find(key);
    if(it == res.images.end())
      return {};
    images_used.insert(key);
    tr.source = it->second;
    tr.texcoord_set = uint32_t(t["uv"].toInt(0));
    const auto wrap = t["wrap"].toString();
    const auto mode = wrap == "clamp"    ? ossia::CLAMP_TO_EDGE
                      : wrap == "mirror" ? ossia::MIRROR
                                         : ossia::REPEAT;
    tr.sampler.wrap_s = mode;
    tr.sampler.wrap_t = mode;
    if(t["filter"].toString() == "nearest")
    {
      tr.sampler.mag_filter = ossia::NEAREST;
      tr.sampler.min_filter = ossia::NEAREST;
    }
    // KHR_texture_transform, as GltfParser reads it.
    if(const auto x = t["transform"].toObject(); !x.isEmpty())
    {
      nums(x["offset"], tr.uv_transform.offset);
      nums(x["scale"], tr.uv_transform.scale);
      tr.uv_transform.rotation = num(x["rotation"], 0.f);
    }
    return t;
  };
  fill(mc->base_color_texture, "base_color");
  fill(mc->metallic_roughness_texture, "metallic_roughness");
  if(const auto t = fill(mc->normal_texture, "normal"); !t.isEmpty())
    mc->normal_scale = num(t["scale"], 1.f);
  if(const auto t = fill(mc->occlusion_texture, "occlusion"); !t.isEmpty())
    mc->occlusion_strength = num(t["strength"], 1.f);
  fill(mc->emissive_texture, "emissive");

  // Extensions: the fields GltfParser fills from KHR_materials_*.
  mc->unlit = m["unlit"].toBool(false);
  mc->ior = num(m["ior"], 1.5f);
  mc->transmission.factor = num(m["transmission"], 0.f);
  fill(mc->transmission.texture, "transmission");
  mc->specular.factor = num(m["specular"], 1.f);
  if(m.contains("specular_color"))
    nums(m["specular_color"], mc->specular.color_factor);
  fill(mc->specular.texture, "specular");
  fill(mc->specular.color_texture, "specular_color");
  mc->clearcoat.factor = num(m["clearcoat"], 0.f);
  mc->clearcoat.roughness_factor = num(m["clearcoat_roughness"], 0.f);
  fill(mc->clearcoat.texture, "clearcoat");
  fill(mc->clearcoat.roughness_texture, "clearcoat_roughness");
  if(const auto t = fill(mc->clearcoat.normal_texture, "clearcoat_normal"); !t.isEmpty())
    mc->clearcoat.normal_scale = num(t["scale"], 1.f);
  if(m.contains("sheen_color"))
    nums(m["sheen_color"], mc->sheen.color_factor);
  mc->sheen.roughness_factor = num(m["sheen_roughness"], 0.f);
  fill(mc->sheen.color_texture, "sheen_color");
  fill(mc->sheen.roughness_texture, "sheen_roughness");
  mc->anisotropy.strength = num(m["anisotropy"], 0.f);
  mc->anisotropy.rotation = num(m["anisotropy_rotation"], 0.f);
  mc->iridescence.factor = num(m["iridescence"], 0.f);
  mc->iridescence.ior = num(m["iridescence_ior"], 1.3f);
  if(const auto th = m["iridescence_thickness"].toArray(); th.size() == 2)
  {
    mc->iridescence.thickness_min = float(th[0].toDouble());
    mc->iridescence.thickness_max = float(th[1].toDouble());
  }
  return mc;
}
}

void native_resources::add_mesh(
    const mesh_message& m, const std::vector<std::shared_ptr<std::vector<float>>>& tangents)
{
  mesh out;
  out.primitives.reserve(m.primitives.size());
  std::size_t vertices = 0, targets = 0;
  for(std::size_t i = 0; i < m.primitives.size(); ++i)
  {
    out.primitives.push_back(to_primitive(m.primitives[i], i < tangents.size() ? tangents[i] : nullptr));
    vertices += m.primitives[i].vertex_count;
    targets = std::max(targets, m.primitives[i].targets.size());
  }

  // Morph targets: each target's deltas for every primitive, one after the
  // other (MorphTargets.hpp's layout, as GltfParser's attach_morph_targets).
  out.morph_targets.resize(targets);
  for(std::size_t t = 0; t < targets; ++t)
  {
    for(bool positions : {true, false})
    {
      bool any = false;
      for(const auto& p : m.primitives)
        any |= t < p.targets.size() && (positions ? p.targets[t].positions : p.targets[t].normals);
      if(!any)
        continue;
      auto all = std::make_shared<std::vector<float>>(vertices * 3, 0.f);
      std::size_t base = 0;
      for(const auto& p : m.primitives)
      {
        if(t < p.targets.size())
          if(const auto& d = positions ? p.targets[t].positions : p.targets[t].normals)
            std::copy(d->begin(), d->end(), all->begin() + base * 3);
        base += p.vertex_count;
      }
      (positions ? out.morph_targets[t].position_deltas : out.morph_targets[t].normal_deltas)
          = make_buffer(all, ossia::buffer_data::usage::vertex_buffer);
    }
  }
  meshes[m.key] = std::move(out);
}

void native_resources::add_image(const image_message& m)
{
  auto src = std::make_shared<ossia::texture_source>();
  src->embedded_data = m.bytes;
  src->mime_type = m.mime_type;
  if(m.bytes && !m.bytes->empty())
    src->content_hash = ossia::hash_bytes(m.bytes->data(), m.bytes->size());
  images[m.key] = std::move(src);
}

void native_resources::keep_only(
    const std::unordered_set<std::string>& used_meshes,
    const std::unordered_set<std::string>& used_images)
{
  std::erase_if(meshes, [&](const auto& kv) { return !used_meshes.contains(kv.first); });
  std::erase_if(images, [&](const auto& kv) { return !used_images.contains(kv.first); });
}

native_scene build_native_scene(
    std::string_view json, const native_resources& res, uint64_t seq)
{
  native_scene out;
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(
      QByteArray::fromRawData(json.data(), qsizetype(json.size())), &err);
  if(!doc.isObject())
  {
    out.error = err.errorString().toStdString();
    return out;
  }
  const auto root = doc.object();

  auto st = std::make_shared<ossia::scene_state>();
  auto materials = std::make_shared<std::vector<ossia::material_component_ptr>>();
  std::unordered_map<std::string, ossia::material_component_ptr> by_name;
  const auto mats = root["materials"].toObject();
  for(auto it = mats.begin(); it != mats.end(); ++it)
  {
    // Nodes refer to the key; values stream by "name", the material's own
    // (several keys can share one, for different texture coordinates).
    const auto key = it.key().toStdString();
    const auto obj = it.value().toObject();
    const auto name = obj.contains("name") ? str(obj["name"]) : key;
    auto mc = to_material(name, obj, res, out.images, seq);
    by_name[key] = mc;
    materials->push_back(std::move(mc));
    out.material_names.push_back(name);
  }

  // Parents come before their children: each node's payload is gathered in
  // a first pass, and the nodes are made, children first, in a second.
  const auto nodes = root["nodes"].toArray();
  const auto n = std::size_t(nodes.size());
  std::vector<std::vector<ossia::scene_payload>> payloads(n);
  std::vector<std::size_t> skinned;
  std::vector<std::size_t> mesh_slot(n, std::size_t(-1)); // index in payloads[i]
  std::vector<std::vector<std::size_t>> children(n);
  std::vector<int> parent(n, -1);
  for(std::size_t i = 0; i < n; ++i)
  {
    const auto node = nodes[qsizetype(i)].toObject();
    // The parent: an index into "nodes", earlier in it (names repeat: bones).
    const int p = node["parent"].toInt(-1) < int(i) ? node["parent"].toInt(-1) : -1;
    if(p >= 0)
    {
      parent[i] = p;
      children[std::size_t(p)].push_back(i);
    }

    ossia::scene_transform xf{};
    nums(node["t"], xf.translation);
    nums(node["r"], xf.rotation);
    nums(node["s"], xf.scale);
    auto& pl = payloads[i];
    pl.push_back(xf);

    if(node.contains("mesh"))
    {
      const auto key = str(node["mesh"]);
      if(auto m = res.meshes.find(key); m != res.meshes.end())
      {
        out.meshes.insert(key);
        auto mc = std::make_shared<ossia::mesh_component>();
        mc->morph_targets = m->second.morph_targets;
        mc->morph_weights.assign(mc->morph_targets.size(), 0.f);
        // Empty (inverted) until the first primitive's bounds.
        mc->bounds.min[0] = mc->bounds.min[1] = mc->bounds.min[2] = 1.f;
        mc->bounds.max[0] = mc->bounds.max[1] = mc->bounds.max[2] = -1.f;
        const auto slots = node["materials"].toArray();
        for(std::size_t k = 0; k < m->second.primitives.size(); ++k)
        {
          auto prim = m->second.primitives[k];
          if(k < std::size_t(slots.size()))
            if(auto mat = by_name.find(str(slots[qsizetype(k)])); mat != by_name.end())
              prim.material = mat->second;
          mc->bounds.expand(prim.bounds.min[0], prim.bounds.min[1], prim.bounds.min[2]);
          mc->bounds.expand(prim.bounds.max[0], prim.bounds.max[1], prim.bounds.max[2]);
          st->statistics.total_vertices += prim.vertex_count;
          st->statistics.total_triangles
              += (prim.index_count ? prim.index_count : prim.vertex_count) / 3;
          mc->primitives.push_back(std::move(prim));
        }
        mc->dirty_index = 1;
        if(node.contains("skin"))
          skinned.push_back(i);
        // The mesh holds the basis; the node's weights blend it once, as
        // GltfParser bakes a file's default weights.
        std::vector<float> weights;
        for(const auto& w : node["weights"].toArray())
          weights.push_back(float(w.toDouble()));
        mesh_slot[i] = pl.size();
        if(std::ranges::any_of(weights, [](float w) { return w != 0.f; }))
          pl.push_back(ossia::mesh_component_ptr(Threedim::blendMorphTargets(*mc, weights)));
        else
          pl.push_back(ossia::mesh_component_ptr(std::move(mc)));
      }
    }
    if(const auto l = node["light"].toArray(); l.size() >= 8)
    {
      light_record r;
      r.type = uint8_t(l[0].toInt());
      for(int c = 0; c < 3; ++c)
        r.color[c] = float(l[1 + c].toDouble());
      r.intensity = float(l[4].toDouble());
      r.range = float(l[5].toDouble());
      r.inner_cone = float(l[6].toDouble());
      r.outer_cone = float(l[7].toDouble());
      auto lc = std::make_shared<ossia::light_component>();
      apply_light(*lc, r);
      pl.push_back(ossia::light_component_ptr(std::move(lc)));
      st->statistics.total_lights++;
    }
    if(const auto c = node["camera"].toArray(); c.size() >= 7)
    {
      camera_record r;
      r.projection = uint8_t(c[0].toInt());
      r.yfov = float(c[1].toDouble());
      r.aspect = float(c[2].toDouble());
      r.znear = float(c[3].toDouble());
      r.zfar = float(c[4].toDouble());
      r.xmag = float(c[5].toDouble());
      r.ymag = float(c[6].toDouble());
      auto cc = std::make_shared<ossia::camera_component>();
      apply_camera(*cc, r);
      pl.push_back(ossia::camera_component_ptr(std::move(cc)));
    }
  }

  // Skins, as GltfParser builds them: a joint per bone node named by the
  // skin (looked up under its armature's node), with the local transform of
  // that node and the inverse bind matrix sent; a root joint's parent, when
  // not the mesh node's frame, as an extra joint carrying the difference.
  auto local = [&](std::size_t i) {
    const auto* xf = ossia::get_if<ossia::scene_transform>(&payloads[i].front());
    return xf ? trs_matrix(*xf) : QMatrix4x4{};
  };
  auto world = [&](int i) {
    QMatrix4x4 m;
    for(int d = 0; i >= 0 && d < 256; ++d, i = parent[i])
      m = local(std::size_t(i)) * m;
    return m;
  };
  auto skeletons = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
  for(auto i : skinned)
  {
    const auto skin = nodes[qsizetype(i)].toObject()["skin"].toObject();
    const int av = skin["armature"].toInt(-1);
    if(av < 0 || std::size_t(av) >= n)
      continue;
    const auto arm = std::size_t(av);
    // The armature's descendants, by name: bone names are unique there.
    std::unordered_map<std::string, std::size_t> bones;
    std::vector<std::size_t> todo{arm};
    while(!todo.empty())
    {
      const auto k = todo.back();
      todo.pop_back();
      for(auto c : children[k])
      {
        bones.emplace(str(nodes[qsizetype(c)].toObject()["name"]), c);
        todo.push_back(c);
      }
    }

    auto skel = std::make_shared<ossia::skeleton_component>();
    const auto joints = skin["joints"].toArray();
    const auto ibms = skin["inverse_bind"].toArray();
    std::unordered_map<std::size_t, int> joint_of;
    for(qsizetype j = 0; j < joints.size(); ++j)
    {
      const auto b = bones.find(str(joints[j]));
      if(b == bones.end())
        break;
      ossia::skeleton_joint sj;
      sj.name = b->first;
      decompose(local(b->second), sj.translation, sj.rotation, sj.scale);
      for(int k = 0; k < 16 && j * 16 + k < ibms.size(); ++k)
        sj.inverse_bind_matrix[k] = float(ibms[j * 16 + k].toDouble());
      joint_of.emplace(b->second, int(skel->joints.size()));
      skel->joints.push_back(std::move(sj));
      skel->joint_node_ids.push_back(ossia::scene_node_id{b->second + 1});
    }
    if(skel->joints.size() != std::size_t(joints.size()))
      continue; // a joint missing: no skin rather than a wrong one

    const QMatrix4x4 mesh_world_inv = world(int(i)).inverted();
    std::unordered_map<int, int> root_parent_of;
    const auto count = skel->joints.size();
    for(std::size_t j = 0; j < count; ++j)
    {
      const auto node = skel->joint_node_ids[j].value - 1;
      const int p = parent[node];
      if(auto it = p >= 0 ? joint_of.find(std::size_t(p)) : joint_of.end(); it != joint_of.end())
      {
        skel->joints[j].parent_index = it->second;
        continue;
      }
      auto rp = root_parent_of.find(p);
      if(rp == root_parent_of.end())
      {
        const QMatrix4x4 m = mesh_world_inv * world(p);
        int extra = -1;
        if(!qFuzzyCompare(m, QMatrix4x4{}))
        {
          ossia::skeleton_joint sj;
          decompose(m, sj.translation, sj.rotation, sj.scale);
          extra = int(skel->joints.size());
          skel->joints.push_back(std::move(sj));
          skel->joint_node_ids.push_back(ossia::scene_node_id{});
        }
        rp = root_parent_of.emplace(p, extra).first;
      }
      skel->joints[j].parent_index = rp->second;
    }
    skel->dirty_index = 1;

    // The node's mesh points at its skeleton, as GltfParser's do.
    if(mesh_slot[i] < payloads[i].size())
      if(auto* mp = ossia::get_if<ossia::mesh_component_ptr>(&payloads[i][mesh_slot[i]]); mp && *mp)
      {
        auto mc = std::make_shared<ossia::mesh_component>(**mp);
        mc->skin = skel;
        *mp = std::move(mc);
      }
    skeletons->push_back(std::move(skel));
  }
  if(!skeletons->empty())
    st->skeletons = std::move(skeletons);

  std::vector<ossia::scene_node_ptr> made(n);
  for(std::size_t i = n; i-- > 0;)
  {
    const auto node = nodes[qsizetype(i)].toObject();
    auto sn = std::make_shared<ossia::scene_node>();
    sn->id.value = i + 1;
    sn->name = str(node["name"]);
    sn->visible = node["visible"].toBool(true);
    auto pl = std::move(payloads[i]);
    for(auto c : children[i])
      if(made[c])
        pl.push_back(made[c]);
    sn->children = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(pl));
    made[i] = std::move(sn);
  }

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  for(std::size_t i = 0; i < n; ++i)
    if(parent[i] < 0)
      roots->push_back(made[i]);
  st->roots = std::move(roots);
  st->statistics.total_nodes = n;
  st->statistics.total_materials = materials->size();
  st->materials = std::move(materials);
  out.state = std::move(st);
  return out;
}

}
