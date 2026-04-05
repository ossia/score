#pragma once
#include <Threedim/TinyObj.hpp>
#include <Threedim/Vox.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{

class VoxelLoader
{
public:
  halp_meta(name, "Voxel loader")
  halp_meta(category, "Visuals/Meshes")
  halp_meta(c_name, "voxel_loader")
  halp_meta(authors, "Jean-Michaël Celerier, opengametools authors")
  halp_meta(manual_url, "")
  halp_meta(uuid, "a7c3e1b4-9f2d-4e8a-b6c5-1d3f7e9a2b4c")

  enum VoxelMode
  {
    PointCloud,
    Mesh_Simple,
    Mesh_Greedy
  };
  struct ins
  {
    struct vox_t : halp::file_port<"Voxel file">
    {
      halp_meta(extensions, "Voxel files (*.vox)");
      static std::function<void(VoxelLoader&)> process(file_type data);
    } file;

    struct : halp::combobox_t<"Mode", VoxelMode>
    {
      struct range
      {
        std::string_view values[3]{"Point Cloud", "Mesh (Simple)", "Mesh (Greedy)"};
        int init{1};
      };

      void update(VoxelLoader& self) { self.reload(); }
    } mode;

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

  void reload();
  void rebuild_geometry();

  std::vector<mesh> meshinfo{};
  float_vec complete;
  float_vec palette;

  // Cache the file data so mode changes can re-process
  std::string cached_filename;
};

}
