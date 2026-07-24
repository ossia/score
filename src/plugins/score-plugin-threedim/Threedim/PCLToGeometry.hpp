#pragma once

#include <Threedim/TinyObj.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{

class PCLToMesh2
{
public:
  halp_meta(name, "Pointcloud to mesh")
  halp_meta(category, "Visuals/Meshes")
  halp_meta(c_name, "pointcloud_to_mesh")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pointcloud-to-mesh.html")
  halp_meta(uuid, "2450ffbf-04ed-4b42-8848-69f200d2742a")

  enum BufferType
  {
    XYZ,
    XYZ_RGB,
    XYZW,
    XYZW_RGBA
  };
  struct ins
  {
    halp::gpu_buffer_input<"Buffer"> in;
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    halp::enum_t<BufferType, "Buffer type"> type;
  } inputs;

  struct
  {
    struct
    {
      // Use Noiuse::dynamic_geometry
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  PCLToMesh2();
  void operator()();
};
}
