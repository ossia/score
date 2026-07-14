#pragma once
#include "TransformHelper.hpp"

#include <Threedim/TinyObj.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/mutex.hpp>

namespace Threedim
{

// Geometry-only file loader. Dispatches by extension to the right parser
// and emits a halp::dynamic_geometry output — one draw-ready mesh per
// file part, no scene graph, no materials, no lights. Use AssetLoader
// for the full-scene variant (FBX / glTF also go through a
// geometry+materials+hierarchy scene_spec pipeline there).
//
// Supported extensions: .obj, .ply, .stl, .off. STL + OFF go through
// the vcglib importers; OBJ + PLY through tinyobj / miniply. All four
// funnel into the same `Threedim::mesh` + `float_vec` representation
// so `rebuild_geometry` sees one uniform input format.
//
// This is the TD-equivalent of a geometry-specific SOP-style loader —
// simpler output, no material / skeleton / animation carry-along. When
// users want the full content (PBR materials, skeletons, anim clips)
// they reach for AssetLoader instead.
class GeometryLoader
{
public:
  halp_meta(name, "Geometry Loader")
  halp_meta(category, "Visuals/Meshes")
  halp_meta(c_name, "geometry_loader")
  halp_meta(
      authors,
      "Jean-Michaël Celerier, TinyOBJ authors, miniPLY authors, vcglib authors, Eigen authors")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/meshes.html#geometry-loader")
  halp_meta(uuid, "5df71765-505f-4ab7-98c1-f305d10a01ef")

  struct ins
  {
    struct geom_t : halp::file_port<"3D file">
    {
      halp_meta(extensions, "3D files (*.obj *.ply *.stl *.off)");
      static std::function<void(GeometryLoader&)> process(file_type data);
    } geom;
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
  void operator()();

  std::vector<mesh> meshinfo{};
  float_vec complete;

  // Per-frame TRS matrix cache (see TransformHelper.hpp).
  CachedTRS m_cachedTRS{};
};

}
