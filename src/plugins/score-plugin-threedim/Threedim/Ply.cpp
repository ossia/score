#include "Ply.hpp"

#include <miniply.h>
namespace Threedim
{

struct TriMesh
{
  float_vec storage;
  float* pos = nullptr;
  float* uv = nullptr;
  float* norm = nullptr;
  float* color = nullptr;
  uint32_t numVerts = 0;

  int* indices = nullptr;
  uint32_t numIndices = 0;
};

static bool print_ply_header(const char* filename)
{
  static const char* kPropertyTypes[] = {
      "char",
      "uchar",
      "short",
      "ushort",
      "int",
      "uint",
      "float",
      "double",
  };

  miniply::PLYReader reader(filename);
  if (!reader.valid())
  {
    return false;
  }
  using namespace miniply;
  for (uint32_t i = 0, endI = reader.num_elements(); i < endI; i++)
  {
    const miniply::PLYElement* elem = reader.get_element(i);
    fprintf(stderr, "element %s %u\n", elem->name.c_str(), elem->count);
    for (const miniply::PLYProperty& prop : elem->properties)
    {
      if (prop.countType != miniply::PLYPropertyType::None)
      {
        fprintf(
            stderr,
            "property list %s %s %s\n",
            kPropertyTypes[uint32_t(prop.countType)],
            kPropertyTypes[uint32_t(prop.type)],
            prop.name.c_str());
      }
      else
      {
        fprintf(
            stderr,
            "property %s %s\n",
            kPropertyTypes[uint32_t(prop.type)],
            prop.name.c_str());
      }
    }
  }
  fprintf(stderr, "end_header\n");

  return true;
}

static bool
load_vert_from_ply(miniply::PLYReader& reader, TriMesh* trimesh, float_vec& buf)
{
  uint32_t pos_indices[3];
  uint32_t uv_indices[3];
  uint32_t n_indices[3];
  uint32_t col_indices[3];

  if (reader.element_is(miniply::kPLYVertexElement) && reader.load_element())
  {
    const auto N = reader.num_rows();
    if (N <= 0)
      return false;

    trimesh->numVerts = N;

    bool pos = reader.find_pos(pos_indices);
    bool uv = reader.find_texcoord(uv_indices);
    bool norms = reader.find_normal(n_indices);
    bool col = reader.find_color(col_indices);
    if (!col)
    {
      col = reader.find_properties(
          col_indices, 3, "diffuse_red", "diffuse_green", "diffuse_blue");
    }

    int num_elements = 0;
    if (pos)
      num_elements += 3;
    if (uv)
      num_elements += 2;
    if (norms)
      num_elements += 3;
    if (col)
      num_elements += 3;

    if (!pos)
      return false;

    buf.resize(num_elements * trimesh->numVerts, boost::container::default_init);

    float* cur = buf.data();
    if (pos)
    {
      trimesh->pos = cur;
      reader.extract_properties(
          pos_indices, 3, miniply::PLYPropertyType::Float, trimesh->pos);
      cur += 3 * N;
    }

    if (uv)
    {
      trimesh->uv = cur;
      reader.extract_properties(
          uv_indices, 2, miniply::PLYPropertyType::Float, trimesh->uv);
      cur += 2 * N;
    }

    if (norms)
    {
      trimesh->norm = cur;
      reader.extract_properties(
          n_indices, 3, miniply::PLYPropertyType::Float, trimesh->norm);
      cur += 3 * N;
    }

    if (col)
    {
      const auto t = reader.element()->properties[col_indices[0]].type;
      trimesh->color = cur;
      reader.extract_properties(
          col_indices, 3, miniply::PLYPropertyType::Float, trimesh->color);
      switch (t)
      {
        case miniply::PLYPropertyType::Float:
          break;
        case miniply::PLYPropertyType::Char:
          break;
        case miniply::PLYPropertyType::UChar:
        {
          for (float *begin = trimesh->color, *end = trimesh->color + 3 * N;
               begin != end;
               ++begin)
            *begin /= 255.f;
          break;
        }
        default:
          break;
      }
    }
    return true;
  }
  return false;
}

static TriMesh load_vertices_from_ply(miniply::PLYReader& reader, float_vec& buf)
{
  TriMesh mesh;

  while (reader.has_element())
  {
    if (load_vert_from_ply(reader, &mesh, buf))
      return mesh;
    reader.next_element();
  }

  return {};
}

std::vector<mesh> PlyFromFile(std::string_view filename, float_vec& buf)
{
  print_ply_header(filename.data());

  std::vector<mesh> meshes;
  miniply::PLYReader reader{filename.data()};
  if (!reader.valid())
    return {};

  auto res = load_vertices_from_ply(reader, buf);
  if (!res.pos)
    return {};

  auto begin = buf.data();
  mesh m{};
  m.vertices = res.numVerts;
  m.points = true;
  m.pos_offset = 0;
  if (res.uv)
  {
    m.texcoord_offset = res.uv - begin;
    m.texcoord = true;
  }
  if (res.norm)
  {
    m.normal_offset = res.norm - begin;
    m.normals = true;
  }
  if (res.color)
  {
    m.color_offset = res.color - begin;
    m.colors = true;
  }

  meshes.push_back(m);
  return meshes;
}

}
