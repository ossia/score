#pragma once

#include <Threedim/TinyObj.hpp>
#include <boost/container/vector.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <halp/texture.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace halp
{
struct gpu_buffer
{
  void* handle{};
  int bytesize{};
};

template <static_string lit>
struct gpu_buffer_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  operator const halp::gpu_buffer&() const noexcept { return buffer; }
  operator halp::gpu_buffer&() noexcept { return buffer; }

  halp::gpu_buffer buffer{};
};

struct position_gpu_geometry
{
  struct buffers
  {
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      void* handle{};
      int size{};
      bool dirty{};
    } main_buffer;
  } buffers;

  struct bindings
  {
    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 3 * sizeof(float);
      int step_rate = 1;
    } position_binding;
  };

  struct attributes
  {
    struct
    {
      enum
      {
        position
      };
      using datatype = float[3];
      int32_t offset = 0;
      int32_t binding = 0;
    } position;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input0;
  } input;

  int vertices = 0;
  enum
  {
    triangles,
    counter_clockwise,
    cull_back
  };
};

}
namespace Threedim
{

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
    halp::buffer_input<"Buffer"> in;
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

  PCLToMesh();
  void create_mesh(std::span<float> v);
  void operator()();

  std::vector<float> complete;
};


class PCLToMesh2
{
public:
  halp_meta(name, "Pointcloud to mesh")
  halp_meta(category, "Visuals/3D")
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
  void create_mesh(std::span<float> v);
  void operator()();

  std::vector<float> complete;
};
}
