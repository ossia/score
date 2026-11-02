#include "SceneSummary.hpp"

#include <Threedim/ModelDisplay/ModelSceneDrawer.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QQuaternion>

#include <fmt/format.h>

#include <cmath>
#include <cstring>

namespace Gfx::GeometryViewer
{
namespace
{
constexpr ossia::aabb empty_box{{1.f, 1.f, 1.f}, {-1.f, -1.f, -1.f}};

QMatrix4x4 toMatrix(const ossia::scene_transform& t)
{
  QMatrix4x4 m;
  m.translate(t.translation[0], t.translation[1], t.translation[2]);
  m.rotate(QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
  m.scale(t.scale[0], t.scale[1], t.scale[2]);
  return m;
}

//! Some producers leave the bounds of a primitive at zero rather than empty
bool knownBounds(const ossia::aabb& b) noexcept
{
  return !b.empty()
         && (b.max[0] > b.min[0] || b.max[1] > b.min[1] || b.max[2] > b.min[2]);
}

//! Positions of a primitive held in memory, as float3 or float4
ossia::aabb primitiveBounds(const ossia::mesh_primitive& p) noexcept
{
  if(knownBounds(p.bounds))
    return p.bounds;
  auto box = empty_box;
  auto attr = ossia::find_if(p.attributes, [](const ossia::vertex_attribute& a) {
    return a.semantic == ossia::attribute_semantic::position;
  });
  if(attr == p.attributes.end()
     || (attr->format != ossia::vertex_format::float3
         && attr->format != ossia::vertex_format::float4)
     || attr->buffer_index >= p.vertex_buffers.size() || !p.vertex_buffers[attr->buffer_index])
    return box;
  const auto* data
      = ossia::get_if<ossia::buffer_data>(&p.vertex_buffers[attr->buffer_index]->resource);
  if(!data || !data->data)
    return box;
  const int64_t element = attr->format == ossia::vertex_format::float3 ? 12 : 16;
  const int64_t stride = attr->byte_stride > 0 ? attr->byte_stride : element;
  const auto* bytes = static_cast<const char*>(data->data.get());
  const int64_t first = int64_t(attr->byte_offset) + int64_t(p.first_vertex) * stride;
  for(int64_t i = 0; i < p.vertex_count; i++)
  {
    const int64_t at = first + i * stride;
    if(at < 0 || at + 12 > data->byte_size)
      break;
    float v[3];
    std::memcpy(v, bytes + at, sizeof(v));
    if(std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]))
      box.expand(v[0], v[1], v[2]);
  }
  return box;
}

ossia::aabb legacyBounds(const ossia::geometry& g) noexcept
{
  const ossia::aabb b{
      {g.bounds.min[0], g.bounds.min[1], g.bounds.min[2]},
      {g.bounds.max[0], g.bounds.max[1], g.bounds.max[2]}};
  if(knownBounds(b))
    return b;
  return score::gfx::geometryPositionBounds(g);
}

const char* lightTypeName(ossia::light_type t) noexcept
{
  switch(t)
  {
    case ossia::light_type::directional:
      return "directional";
    case ossia::light_type::point:
      return "point";
    case ossia::light_type::spot:
      return "spot";
    case ossia::light_type::rect_area:
      return "rectangle area";
    case ossia::light_type::disk_area:
      return "disk area";
    case ossia::light_type::sphere_area:
      return "sphere area";
    case ossia::light_type::cylinder_area:
      return "cylinder area";
    case ossia::light_type::dome:
      return "dome";
  }
  return "light";
}

std::string plural(uint64_t n, const char* one, const char* many)
{
  return fmt::format("{} {}", n, n == 1 ? one : many);
}

struct Walker
{
  SceneDescription& out;
  const ossia::scene_state& state;
  ossia::hash_set<const void*> materials;

  int32_t addRow(int32_t parent, SceneRow::Kind kind, std::string name, std::string details)
  {
    out.summary.rows.push_back(SceneRow{
        .parent = parent, .kind = kind, .name = std::move(name),
        .details = std::move(details)});
    return int32_t(out.summary.rows.size() - 1);
  }

  //! The row and the ones it is under
  void grow(int32_t row, const ossia::aabb& box)
  {
    if(box.empty())
      return;
    for(; row >= 0; row = out.summary.rows[row].parent)
      unite(out.summary.rows[row].bounds, box);
  }

  void material(const ossia::material_component_ptr& m)
  {
    if(m && materials.insert(m.get()).second)
      out.summary.counts.materials++;
  }

  //! The primitives of a mesh where `world` puts them
  ossia::aabb meshBounds(const ossia::mesh_component& mc, const QMatrix4x4& world)
  {
    auto box = empty_box;
    for(const auto& p : mc.primitives)
      unite(box, transformBounds(primitiveBounds(p), world));
    if(mc.legacy_geometry.meshes)
      for(const auto& g : mc.legacy_geometry.meshes->meshes)
        unite(box, transformBounds(legacyBounds(g), world));
    if(box.empty() && knownBounds(mc.bounds))
      box = transformBounds(mc.bounds, world);
    return box;
  }

  //! The material a primitive is drawn with, in the active variant
  ossia::material_component_ptr materialOf(const ossia::mesh_primitive& p) const
  {
    const int v = state.active_variant_index;
    if(v >= 0 && std::size_t(v) < p.material_variants.size() && p.material_variants[v])
      return p.material_variants[v];
    return p.material;
  }

  //! Returns false once there is no room left
  bool addDraws(const ossia::mesh_component& mc, const QMatrix4x4& world)
  {
    auto& draws = out.draws;
    for(const auto& p : mc.primitives)
    {
      if(p.vertex_buffers.empty() || p.vertex_count == 0)
        continue;
      if(draws.size() >= SceneDescription::max_draws)
        return false;
      draws.push_back({.primitive = &p, .world = world, .material = materialOf(p)});
    }
    if(mc.legacy_geometry.meshes)
    {
      const auto& list = mc.legacy_geometry.meshes->meshes;
      for(std::size_t i = 0; i < list.size(); i++)
      {
        if(draws.size() >= SceneDescription::max_draws)
          return false;
        draws.push_back(
            {.legacy = &mc.legacy_geometry, .legacy_index = i, .world = world,
             .material = mc.primitives.empty() ? nullptr : mc.primitives[0].material});
      }
    }
    return true;
  }

  void mesh(const ossia::mesh_component& mc, int32_t parent, const QMatrix4x4& world)
  {
    addDraws(mc, world);
    auto& c = out.summary.counts;
    c.meshes++;
    uint64_t vertices = 0;
    uint32_t primitives = 0;
    const ossia::material_component* first_material{};
    for(const auto& p : mc.primitives)
    {
      if(p.vertex_buffers.empty() || p.vertex_count == 0)
        continue;
      primitives++;
      vertices += p.vertex_count;
      material(p.material);
      if(!first_material && p.material)
        first_material = p.material.get();
    }
    if(mc.legacy_geometry.meshes)
      for(const auto& g : mc.legacy_geometry.meshes->meshes)
      {
        primitives++;
        vertices += std::max(0, g.vertices);
      }
    c.primitives += primitives;
    c.vertices += vertices;

    std::string details = plural(primitives, "primitive", "primitives") + ", "
                          + plural(vertices, "vertex", "vertices");
    if(first_material)
      details += ", material " + materialName(*first_material, &state);
    if(!mc.morph_targets.empty())
      details += ", " + plural(mc.morph_targets.size(), "morph target", "morph targets");
    if(mc.skin)
      details += ", skinned";
    const int32_t row = addRow(parent, SceneRow::Kind::Mesh, "Mesh", std::move(details));

    const auto box = meshBounds(mc, world);
    grow(row, box);
    unite(out.bounds, box);
  }

  void light(const ossia::light_component& l, int32_t parent, const QMatrix4x4& world)
  {
    out.summary.counts.lights++;
    const int32_t row = addRow(
        parent, SceneRow::Kind::Light, "Light",
        fmt::format(
            "{}, colour {:.2f} {:.2f} {:.2f}, intensity {:.3g}", lightTypeName(l.type),
            l.color[0], l.color[1], l.color[2], l.intensity));
    SceneGizmo g{.kind = SceneGizmo::Light, .light = l.type, .world = world, .row = row};
    std::copy_n(l.color, 3, g.color);
    out.gizmos.push_back(g);
    point(row, world);
  }

  void camera(const ossia::camera_component& cam, int32_t parent, const QMatrix4x4& world)
  {
    out.summary.counts.cameras++;
    const bool ortho = cam.projection == ossia::camera_projection::orthographic;
    std::string details
        = ortho ? std::string{"orthographic"}
                : fmt::format(
                      "{}, field of view {:.0f} degrees",
                      cam.projection == ossia::camera_projection::fulldome ? "fulldome"
                                                                            : "perspective",
                      cam.yfov * 180. / 3.14159265358979);
    const int32_t row
        = addRow(parent, SceneRow::Kind::Camera, "Camera", std::move(details));
    SceneGizmo g{
        .kind = SceneGizmo::Camera, .yfov = ortho ? 0.f : cam.yfov,
        .aspect = cam.aspect_ratio_set && cam.aspect_ratio > 0.f ? cam.aspect_ratio : 1.5f,
        .world = world, .row = row};
    g.color[0] = 0.95f, g.color[1] = 0.85f, g.color[2] = 0.45f;
    out.gizmos.push_back(g);
    point(row, world);
  }

  //! A row with a position and no extent
  void point(int32_t row, const QMatrix4x4& world)
  {
    const QVector3D p = world.map(QVector3D{});
    out.summary.rows[row].bounds.expand(p.x(), p.y(), p.z());
  }

  void instances(const ossia::instance_component& inst, int32_t parent, const QMatrix4x4& world)
  {
    auto& c = out.summary.counts;
    c.instances += inst.instance_count;
    const int32_t row = addRow(
        parent, SceneRow::Kind::Instance, "Instances",
        plural(inst.instance_count, "instance", "instances"));
    if(!inst.prototype)
      return;
    const uint32_t n = instancesInMemory(inst);
    auto box = empty_box;
    uint32_t drawn = 0;
    for(uint32_t i = 0; i < n; i++)
    {
      QMatrix4x4 m;
      if(!instanceMatrix(inst, i, m))
        continue;
      unite(box, meshBounds(*inst.prototype, world * m));
      if(!addDraws(*inst.prototype, world * m))
        break;
      drawn++;
    }
    out.skipped_instances += inst.instance_count - drawn;
    for(const auto& p : inst.prototype->primitives)
      material(p.material);
    grow(row, box);
    unite(out.bounds, box);
  }

  void payload(const ossia::scene_payload& p, int32_t parent, QMatrix4x4& world)
  {
    using K = SceneRow::Kind;
    if(auto* n = ossia::get_if<ossia::scene_node_ptr>(&p))
    {
      if(*n)
        node(**n, parent, world);
    }
    else if(auto* m = ossia::get_if<ossia::mesh_component_ptr>(&p))
    {
      if(*m)
        mesh(**m, parent, world);
    }
    else if(auto* mat = ossia::get_if<ossia::material_component_ptr>(&p))
    {
      if(*mat)
      {
        material(*mat);
        addRow(parent, K::Material, "Material", materialName(**mat, &state));
      }
    }
    else if(auto* l = ossia::get_if<ossia::light_component_ptr>(&p))
    {
      if(*l)
        light(**l, parent, world);
    }
    else if(auto* cam = ossia::get_if<ossia::camera_component_ptr>(&p))
    {
      if(*cam)
        camera(**cam, parent, world);
    }
    else if(auto* xf = ossia::get_if<ossia::scene_transform>(&p))
    {
      world = world * toMatrix(*xf);
    }
    else if(auto* sk = ossia::get_if<ossia::skeleton_component_ptr>(&p))
    {
      if(*sk)
        addRow(
            parent, K::Skeleton, "Skeleton",
            plural((*sk)->joints.size(), "joint", "joints"));
    }
    else if(auto* an = ossia::get_if<ossia::animation_component_ptr>(&p))
    {
      if(*an)
        addRow(
            parent, K::Animation, "Animation",
            fmt::format(
                "{}, {:.2f} seconds", plural((*an)->channels.size(), "channel", "channels"),
                (*an)->duration));
    }
    else if(auto* inst = ossia::get_if<ossia::instance_component_ptr>(&p))
    {
      if(*inst)
        instances(**inst, parent, world);
    }
    else if(auto* sd = ossia::get_if<ossia::scene_data_ptr>(&p))
    {
      if(*sd)
        addRow(parent, K::Data, "Data", (*sd)->name);
    }
    else if(auto* gs = ossia::get_if<ossia::gaussian_splat_component_ptr>(&p))
    {
      if(*gs)
        addRow(parent, K::Splat, "Gaussian splats", plural((*gs)->splat_count, "splat", "splats"));
    }
    else if(auto* vf = ossia::get_if<ossia::voxel_field_component_ptr>(&p))
    {
      if(*vf)
        addRow(parent, K::Voxels, "Voxels", plural((*vf)->voxel_count, "voxel", "voxels"));
    }
    else if(auto* pc = ossia::get_if<ossia::point_cloud_component_ptr>(&p))
    {
      if(*pc)
        addRow(
            parent, K::PointCloud, "Point cloud",
            plural((*pc)->point_count, "point", "points"));
    }
    else if(auto* vol = ossia::get_if<ossia::volume_component_ptr>(&p))
    {
      if(*vol)
        addRow(
            parent, K::Volume, "Volume",
            fmt::format(
                "{} x {} x {}", (*vol)->resolution[0], (*vol)->resolution[1],
                (*vol)->resolution[2]));
    }
    else if(auto* prc = ossia::get_if<ossia::primitive_cloud_component_ptr>(&p))
    {
      if(*prc)
        addRow(
            parent, K::PrimitiveCloud, "Primitive cloud",
            plural((*prc)->primitive_count, "primitive", "primitives"));
    }
  }

  void node(const ossia::scene_node& n, int32_t parent, QMatrix4x4 world)
  {
    if(!n.active)
      return;
    out.summary.counts.nodes++;
    std::string details;
    if(!n.visible)
      details = "hidden";
    const int32_t row = addRow(
        parent, SceneRow::Kind::Node, n.name.empty() ? std::string{"Node"} : n.name,
        std::move(details));
    if(!n.has_children())
      return;
    for(const auto& child : *n.children)
      payload(child, row, world);
  }
};
}

void unite(ossia::aabb& into, const ossia::aabb& box) noexcept
{
  if(box.empty())
    return;
  into.expand(box.min[0], box.min[1], box.min[2]);
  into.expand(box.max[0], box.max[1], box.max[2]);
}

ossia::aabb transformBounds(const ossia::aabb& box, const QMatrix4x4& m) noexcept
{
  if(box.empty())
    return box;
  auto out = empty_box;
  for(int i = 0; i < 8; i++)
  {
    const QVector3D c = m.map(QVector3D{
        (i & 1) ? box.max[0] : box.min[0], (i & 2) ? box.max[1] : box.min[1],
        (i & 4) ? box.max[2] : box.min[2]});
    out.expand(c.x(), c.y(), c.z());
  }
  return out;
}

uint32_t instancesInMemory(const ossia::instance_component& inst) noexcept
{
  if(!inst.instance_transforms)
    return 0;
  const auto* data = ossia::get_if<ossia::buffer_data>(&inst.instance_transforms->resource);
  if(!data || !data->data)
    return 0;
  using F = ossia::instance_component::transform_format;
  const int64_t floats = inst.transform_type == F::mat4  ? 16
                         : inst.transform_type == F::trs ? 10
                                                         : 3;
  const int64_t available = data->byte_size / int64_t(floats * sizeof(float));
  return uint32_t(std::clamp<int64_t>(available, 0, inst.instance_count));
}

bool instanceMatrix(
    const ossia::instance_component& inst, uint32_t i, QMatrix4x4& out) noexcept
{
  if(i >= instancesInMemory(inst))
    return false;
  const auto* data = ossia::get_if<ossia::buffer_data>(&inst.instance_transforms->resource);
  const auto* f = static_cast<const float*>(data->data.get());
  using F = ossia::instance_component::transform_format;
  switch(inst.transform_type)
  {
    case F::mat4:
      // Column-major, as transform3d
      out = QMatrix4x4(f + 16 * i, 4, 4);
      break;
    case F::trs:
    {
      const float* t = f + 10 * i;
      out.setToIdentity();
      out.translate(t[0], t[1], t[2]);
      out.rotate(QQuaternion(t[6], t[3], t[4], t[5]));
      out.scale(t[7], t[8], t[9]);
      break;
    }
    case F::translation:
    {
      const float* t = f + 3 * i;
      out.setToIdentity();
      out.translate(t[0], t[1], t[2]);
      break;
    }
  }
  return true;
}

std::string materialName(
    const ossia::material_component& m, const ossia::scene_state* state)
{
  if(!m.tag.empty())
    return m.tag;
  if(auto it = m.properties.find("name"); it != m.properties.end())
    if(auto s = it->second.target<std::string>(); s && !s->empty())
      return *s;
  if(state && state->materials)
  {
    const auto& all = *state->materials;
    for(std::size_t i = 0; i < all.size(); i++)
      if(all[i].get() == &m)
        return fmt::format("{}", i + 1);
  }
  return "unnamed";
}

void describeScene(const ossia::scene_spec& scene, SceneDescription& out)
{
  out.clear();
  if(!scene.state || !scene.state->roots)
    return;
  Walker w{out, *scene.state, {}};
  for(const auto& root : *scene.state->roots)
    if(root)
      w.node(*root, -1, QMatrix4x4{});
}
}
