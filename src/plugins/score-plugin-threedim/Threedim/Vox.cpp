#include "Vox.hpp"

#include <QDebug>
#include <QFile>

#define OGT_VOX_IMPLEMENTATION
#include <ogt_vox.h>

#define OGT_VOXEL_MESHIFY_IMPLEMENTATION
#include <ogt_voxel_meshify.h>

namespace Threedim
{

// Read .vox file into memory and parse the scene. Caller must destroy with ogt_vox_destroy_scene.
static const ogt_vox_scene* load_vox_scene(std::string_view filename)
{
  QFile f(QString::fromUtf8(filename.data(), filename.size()));
  if(!f.open(QIODevice::ReadOnly))
    return nullptr;

  const QByteArray bytes = f.readAll();
  f.close();

  if(bytes.isEmpty())
    return nullptr;

  return ogt_vox_read_scene(
      reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
}

// Build the palette buffer: 256 entries × 8 floats
static void build_palette(const ogt_vox_scene* scene, float_vec& palette)
{
  palette.resize(vox_palette_total_floats, boost::container::default_init);
  std::memset(palette.data(), 0, vox_palette_byte_size);

  for(int i = 0; i < 256; i++)
  {
    const ogt_vox_rgba c = scene->palette.color[i];
    const ogt_vox_matl& m = scene->materials.matl[i];
    float* entry = palette.data() + i * vox_palette_floats_per_entry;

    entry[0] = c.r / 255.f;
    entry[1] = c.g / 255.f;
    entry[2] = c.b / 255.f;
    entry[3] = c.a / 255.f;
    entry[4] = (m.content_flags & k_ogt_vox_matl_have_metal) ? m.metal : 0.f;
    entry[5] = (m.content_flags & k_ogt_vox_matl_have_rough) ? m.rough : 0.5f;
    entry[6] = (m.content_flags & k_ogt_vox_matl_have_emit) ? m.emit : 0.f;
    entry[7] = (m.content_flags & k_ogt_vox_matl_have_ior) ? m.ior : 1.5f;
  }
}

std::vector<mesh> VoxPointCloudFromFile(
    std::string_view filename, float_vec& buf, float_vec& palette)
{
  const ogt_vox_scene* scene = load_vox_scene(filename);
  if(!scene)
    return {};

  build_palette(scene, palette);

  // Count total solid voxels
  uint64_t totalVoxels = 0;
  for(uint32_t i = 0; i < scene->num_instances; i++)
  {
    const auto& inst = scene->instances[i];
    if(inst.hidden)
      continue;
    const ogt_vox_model* model = scene->models[inst.model_index];
    const uint32_t count = model->size_x * model->size_y * model->size_z;
    for(uint32_t v = 0; v < count; v++)
      if(model->voxel_data[v] != 0)
        totalVoxels++;
  }

  if(totalVoxels == 0)
  {
    ogt_vox_destroy_scene(scene);
    return {};
  }

  // 3 floats position + 1 float material_id per voxel
  buf.resize(totalVoxels * 4, boost::container::default_init);
  float* posOut = buf.data();
  float* matIdOut = buf.data() + totalVoxels * 3;

  uint64_t written = 0;
  for(uint32_t i = 0; i < scene->num_instances; i++)
  {
    const auto& inst = scene->instances[i];
    if(inst.hidden)
      continue;

    const ogt_vox_model* model = scene->models[inst.model_index];
    const auto& t = inst.transform;
    const float px = float(model->size_x / 2);
    const float py = float(model->size_y / 2);
    const float pz = float(model->size_z / 2);

    for(uint32_t z = 0; z < model->size_z; z++)
    {
      for(uint32_t y = 0; y < model->size_y; y++)
      {
        for(uint32_t x = 0; x < model->size_x; x++)
        {
          const uint32_t idx
              = x + y * model->size_x + z * model->size_x * model->size_y;
          const uint8_t ci = model->voxel_data[idx];
          if(ci == 0)
            continue;

          const float lx = float(x) + 0.5f - px;
          const float ly = float(y) + 0.5f - py;
          const float lz = float(z) + 0.5f - pz;

          posOut[written * 3 + 0] = t.m00 * lx + t.m10 * ly + t.m20 * lz + t.m30;
          posOut[written * 3 + 1] = t.m01 * lx + t.m11 * ly + t.m21 * lz + t.m31;
          posOut[written * 3 + 2] = t.m02 * lx + t.m12 * ly + t.m22 * lz + t.m32;

          matIdOut[written] = float(ci);
          written++;
        }
      }
    }
  }

  ogt_vox_destroy_scene(scene);

  mesh m{};
  m.vertices = written;
  m.points = true;
  m.pos_offset = 0;

  extra_attribute matid_attr;
  matid_attr.offset = written * 3;
  matid_attr.semantic = halp::attribute_semantic::material_id;
  matid_attr.format = halp::attribute_format::float1;
  matid_attr.components = 1;
  m.extras.push_back(matid_attr);

  std::vector<mesh> meshes;
  meshes.push_back(std::move(m));
  return meshes;
}

std::vector<mesh> VoxMeshFromFile(
    std::string_view filename, float_vec& buf, float_vec& palette, int mode)
{
  const ogt_vox_scene* scene = load_vox_scene(filename);
  if(!scene)
    return {};

  build_palette(scene, palette);

  // Collect all meshified geometry across instances
  // Per vertex: position (3) + normal (3) + material_id (1) = 7 floats
  static constexpr int floats_per_vert = 7;

  std::vector<mesh> meshes;
  uint64_t total_offset = 0;

  // First pass: count total vertices/indices across all instances
  struct InstanceMesh
  {
    ogt_mesh* mesh;
    ogt_vox_transform transform;
    float px, py, pz;
  };
  std::vector<InstanceMesh> instance_meshes;

  ogt_voxel_meshify_context ctx{};
  uint64_t total_indices = 0;

  for(uint32_t i = 0; i < scene->num_instances; i++)
  {
    const auto& inst = scene->instances[i];
    if(inst.hidden)
      continue;

    const ogt_vox_model* model = scene->models[inst.model_index];

    ogt_mesh* ogt_m = nullptr;
    if(mode == 1)
    {
      ogt_m = ogt_mesh_from_paletted_voxels_greedy(
          &ctx, model->voxel_data,
          model->size_x, model->size_y, model->size_z,
          (const ogt_mesh_rgba*)scene->palette.color);
    }
    else
    {
      ogt_m = ogt_mesh_from_paletted_voxels_simple(
          &ctx, model->voxel_data,
          model->size_x, model->size_y, model->size_z,
          (const ogt_mesh_rgba*)scene->palette.color);
    }

    if(!ogt_m || ogt_m->index_count == 0)
    {
      if(ogt_m)
        ogt_mesh_destroy(&ctx, ogt_m);
      continue;
    }

    total_indices += ogt_m->index_count;
    instance_meshes.push_back({
        ogt_m, inst.transform,
        float(model->size_x / 2), float(model->size_y / 2), float(model->size_z / 2)});
  }

  if(instance_meshes.empty())
  {
    ogt_vox_destroy_scene(scene);
    return {};
  }

  // Allocate: total_indices vertices × 7 floats
  // We de-index the mesh to get flat triangles with per-face normals
  buf.resize(total_indices * floats_per_vert, boost::container::default_init);

  float* posOut = buf.data();
  float* nrmOut = buf.data() + total_indices * 3;
  float* matOut = buf.data() + total_indices * 6;

  uint64_t written = 0;

  for(auto& im : instance_meshes)
  {
    const auto& t = im.transform;
    const ogt_mesh* ogt_m = im.mesh;

    for(uint32_t idx = 0; idx < ogt_m->index_count; idx++)
    {
      const ogt_mesh_vertex& v = ogt_m->vertices[ogt_m->indices[idx]];

      // Position: subtract pivot, apply instance transform
      const float lx = v.pos.x - im.px;
      const float ly = v.pos.y - im.py;
      const float lz = v.pos.z - im.pz;

      posOut[written * 3 + 0] = t.m00 * lx + t.m10 * ly + t.m20 * lz + t.m30;
      posOut[written * 3 + 1] = t.m01 * lx + t.m11 * ly + t.m21 * lz + t.m31;
      posOut[written * 3 + 2] = t.m02 * lx + t.m12 * ly + t.m22 * lz + t.m32;

      // Normal: apply rotation part of instance transform (no translation)
      nrmOut[written * 3 + 0] = t.m00 * v.normal.x + t.m10 * v.normal.y + t.m20 * v.normal.z;
      nrmOut[written * 3 + 1] = t.m01 * v.normal.x + t.m11 * v.normal.y + t.m21 * v.normal.z;
      nrmOut[written * 3 + 2] = t.m02 * v.normal.x + t.m12 * v.normal.y + t.m22 * v.normal.z;

      matOut[written] = float(v.palette_index);
      written++;
    }

    ogt_mesh_destroy(&ctx, im.mesh);
  }

  ogt_vox_destroy_scene(scene);

  mesh m{};
  m.vertices = written;
  m.points = false;
  m.pos_offset = 0;
  m.normal_offset = written * 3;
  m.normals = true;

  extra_attribute matid_attr;
  matid_attr.offset = written * 6;
  matid_attr.semantic = halp::attribute_semantic::material_id;
  matid_attr.format = halp::attribute_format::float1;
  matid_attr.components = 1;
  m.extras.push_back(matid_attr);

  meshes.push_back(std::move(m));
  return meshes;
}

}
