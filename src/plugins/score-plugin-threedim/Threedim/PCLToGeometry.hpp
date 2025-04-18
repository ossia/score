#pragma once

#include <Threedim/TinyObj.hpp>
#include <boost/container/vector.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace Threedim
{
struct raw_texture
{
  unsigned char* bytes{};
  std::size_t bytesize{};

  enum format
  {
    Raw
  };
  bool changed{};
};

class PCLToMesh
{
public:
  halp_meta(name, "Pointcloud to mesh")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "pointcloud_to_mesh")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pointcloud-to-mesh.html")
  halp_meta(uuid, "2450ffbf-04ed-4b42-8848-69f200d2742a")

  struct ins
  {
    struct pcl_in
    {
      static constexpr auto name() { return "Texture"; }
      raw_texture texture;
    } in;
    // halp::texture_input<"Texture"> inx;
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::position_color_packed_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  PCLToMesh() { rebuild_transform(inputs, outputs); }
  void create_mesh(std::span<float> v);
  void operator()();

  std::vector<float> complete;
};

}
