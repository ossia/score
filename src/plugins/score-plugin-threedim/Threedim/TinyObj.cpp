#include "TinyObj.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
// #define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "../3rdparty/tiny_obj_loader.h"

#include <QDebug>
#include <QElapsedTimer>
namespace Threedim
{

std::vector<mesh>
ObjFromString(std::string_view obj_data, std::string_view mtl_data, float_vec& buf)
{
  tinyobj::ObjReaderConfig reader_config;

  tinyobj::ObjReader reader;

  QElapsedTimer e;
  e.start();
  if (!reader.ParseFromString(obj_data, mtl_data, reader_config))
  {
    if (!reader.Error().empty())
    {
      qDebug() << "TinyObjReader: " << reader.Error().c_str();
    }
    return {};
  }

  if (!reader.Warning().empty())
  {
    qDebug() << "TinyObjReader: " << reader.Warning().c_str();
  }

  auto& attrib = reader.GetAttrib();
  auto& shapes = reader.GetShapes();
  if (shapes.empty())
    return {};

  int64_t total_vertices = 0;
  for (auto& shape : shapes)
  {
    total_vertices += shape.mesh.num_face_vertices.size() * 3;
  }

  const bool texcoords = !attrib.texcoords.empty();
  const bool normals = !attrib.normals.empty();

  std::size_t float_count = total_vertices * 3 + (normals ? total_vertices * 3 : 0)
                            + (texcoords ? total_vertices * 2 : 0);

  // 3 float per vertex for position, 3 per vertex for normal
  buf.clear();
  buf.resize(float_count, 0.); // boost::container::default_init);

  int64_t pos_offset = 0;
  int64_t texcoord_offset = 0;
  int64_t normal_offset = 0;
  if (texcoords)
  {
    texcoord_offset = total_vertices * 3;
    if (normals)
      normal_offset = texcoord_offset + total_vertices * 2;
  }
  else if (normals)
  {
    normal_offset = total_vertices * 3;
  }
  float* pos = buf.data() + pos_offset;
  float* tc = buf.data() + texcoord_offset;
  float* norm = buf.data() + normal_offset;

  std::vector<mesh> res;
  for (auto& shape : shapes)
  {
    const auto faces = shape.mesh.num_face_vertices.size();
    const auto vertices = faces * 3;

    res.push_back(
        {.vertices = int64_t(vertices),
         .pos_offset = pos_offset,
         .texcoord_offset = texcoord_offset,
         .normal_offset = normal_offset,
         .texcoord = texcoords,
         .normals = normals});

    size_t index_offset = 0;

    if (texcoords && normals)
    {
      for (auto fv : shape.mesh.num_face_vertices)
      {
        if (fv != 3)
          return {};
        for (auto v = 0; v < 3; v++)
        {
          const auto idx = shape.mesh.indices[index_offset + v];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

          *tc++ = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
          *tc++ = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

          *norm++ = attrib.normals[3 * size_t(idx.normal_index) + 0];
          *norm++ = attrib.normals[3 * size_t(idx.normal_index) + 1];
          *norm++ = attrib.normals[3 * size_t(idx.normal_index) + 2];
        }
        index_offset += 3;
      }

      pos_offset += vertices * 3;
      texcoord_offset += vertices * 2;
      normal_offset += vertices * 3;
    }
    else if (normals)
    {
      for (size_t fv : shape.mesh.num_face_vertices)
      {
        if (fv != 3)
          return {};
        for (size_t v = 0; v < 3; v++)
        {
          const auto idx = shape.mesh.indices[index_offset + v];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

          *norm++ = attrib.normals[3 * size_t(idx.normal_index) + 0];
          *norm++ = attrib.normals[3 * size_t(idx.normal_index) + 1];
          *norm++ = attrib.normals[3 * size_t(idx.normal_index) + 2];
        }
        index_offset += 3;
      }
      pos_offset += vertices * 3;
      normal_offset += vertices * 3;
    }
    else if (texcoords)
    {
      for (size_t fv : shape.mesh.num_face_vertices)
      {
        if (fv != 3)
          return {};
        for (size_t v = 0; v < 3; v++)
        {
          const auto idx = shape.mesh.indices[index_offset + v];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

          *tc++ = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
          *tc++ = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
        }
        index_offset += 3;
      }
      pos_offset += vertices * 3;
      texcoord_offset += vertices * 2;
    }
    else
    {
      for (size_t fv : shape.mesh.num_face_vertices)
      {
        if (fv != 3)
          return {};
        for (size_t v = 0; v < 3; v++)
        {
          const auto idx = shape.mesh.indices[index_offset + v];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
          *pos++ = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
        }
        index_offset += 3;
      }
      pos_offset += vertices * 3;
    }
  }

  return res;
}

std::vector<mesh> ObjFromString(std::string_view obj_data, float_vec& data)
{
  std::string default_mtl = R"(newmtl default
Ka  0.1986  0.0000  0.0000
Kd  0.5922  0.0166  0.0000
Ks  0.5974  0.2084  0.2084
illum 2
Ns 100.2237
)";

  return ObjFromString(obj_data, default_mtl, data);
}

}
