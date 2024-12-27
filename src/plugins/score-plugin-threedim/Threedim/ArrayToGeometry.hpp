#pragma once

#include <Threedim/TinyObj.hpp>
#include <boost/container/vector.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace Threedim
{

class ArrayToMesh
{
public:
  halp_meta(name, "Array to mesh")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "array_to_mesh")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array-to-mesh.html")
  halp_meta(uuid, "dfc5bae9-c75c-4180-b4e8-be3063c8d8f2")

  struct ins
  {
    struct : halp::val_port<"Input", std::vector<float>>
    {
      void update(ArrayToMesh& self) { self.create_mesh(value); }
    } in;
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;

    halp::toggle<"Triangulate"> triangulate;
  } inputs;

  PrimitiveOutputs outputs;
  void create_mesh(std::span<float> v);

  std::vector<float> complete;
};

}
