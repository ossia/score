#pragma once
#include <Threedim/TinyObj.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/mutex.hpp>

namespace Threedim
{

class ObjLoader
{
public:
  halp_meta(name, "Object loader")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "obj_loader")
  halp_meta(
      authors,
      "Jean-Michaël Celerier, TinyOBJ authors, miniPLY authors, Eigen authors")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/meshes.html#obj-loader")
  halp_meta(uuid, "5df71765-505f-4ab7-98c1-f305d10a01ef")

  struct ins
  {
    struct obj_t : halp::file_port<"3D file">
    {
      halp_meta(extensions, "3D files (*.obj *.ply)");
      static std::function<void(ObjLoader&)> process(file_type data);
    } obj;
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
  } inputs;

  struct
  {
    struct : halp::mesh
    {
      halp_meta(name, "Geometry");
      std::vector<halp::dynamic_geometry> mesh;
    } geometry;
  } outputs;

  void rebuild_geometry();

  std::vector<mesh> meshinfo{};
  float_vec complete;
};

}
