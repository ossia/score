#pragma once
#include <boost/container/vector.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <QMatrix4x4>

#include <cstring>
#include <vector>

#include <string_view>
namespace Threedim
{
using float_vec = boost::container::vector<float, ossia::pod_allocator<float>>;

struct mesh {
  int64_t vertices{};
  // offset are in "elements", not bytes
  int64_t pos_offset{}, texcoord_offset{}, normal_offset{}, color_offset{};
  bool texcoord{};
  bool normals{};
  bool colors{};
  bool points{};
};

std::vector<mesh> ObjFromString(
    std::string_view obj_data
    , std::string_view mtl_data
    , float_vec& data);

std::vector<mesh> ObjFromString(
    std::string_view obj_data
    , float_vec& data);

template <std::size_t N>
static void fromGL(float (&from)[N], auto& to)
{
  memcpy(to.data(), from, sizeof(float[N]));
}
template <std::size_t N>
static void toGL(auto& from, float (&to)[N])
{
  memcpy(to, from.data(), sizeof(float[N]));
}

inline void rebuild_transform(auto& inputs, auto& outputs)
{
  QMatrix4x4 model{};
  auto& pos = inputs.position;
  auto& rot = inputs.rotation;
  auto& sc = inputs.scale;

  model.translate(pos.value.x, pos.value.y, pos.value.z);
  model.rotate(QQuaternion::fromEulerAngles(rot.value.x, rot.value.y, rot.value.z));
  model.scale(sc.value.x, sc.value.y, sc.value.z);

  toGL(model, outputs.geometry.transform);
  outputs.geometry.dirty_transform = true;
}
struct PositionControl : halp::xyz_spinboxes_f32<"Position", halp::free_range_min<>>
{
  void update(auto& o) { rebuild_transform(o.inputs, o.outputs); }
};
struct RotationControl
    : halp::xyz_spinboxes_f32<"Rotation", halp::range{0., 359.9999999, 0.}>
{
  void update(auto& o) { rebuild_transform(o.inputs, o.outputs); }
};
struct ScaleControl : halp::xyz_spinboxes_f32<"Scale", halp::range{0.00001, 1000., 1.}>
{
  void update(auto& o) { rebuild_transform(o.inputs, o.outputs); }
};

struct Update
{
  void update(auto& obj) { obj.update(); }
};

struct PrimitiveOutputs
{
  struct
  {
    halp_meta(name, "Geometry");
    halp::position_normals_texcoords_geometry_volume mesh;
    float transform[16]{};
    bool dirty_mesh = false;
    bool dirty_transform = false;
  } geometry;
};
}
