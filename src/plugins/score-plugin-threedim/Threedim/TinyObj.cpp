#include "TinyObj.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
// #define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "../3rdparty/tiny_obj_loader.h"

#include <mikktspace.h>

#include <QDebug>
#include <QElapsedTimer>
namespace Threedim
{

// MikkTSpace callback data: operates on a single flat buffer
// where positions, texcoords, normals, and tangents are stored
// as separate SoA arrays for a given mesh shape.
struct MikkTSpaceUserData
{
  const float* positions; // 3 floats per vertex
  const float* texcoords; // 2 floats per vertex
  const float* normals;   // 3 floats per vertex
  float* tangents;        // 4 floats per vertex (output)
  int num_faces;
};

static int mts_getNumFaces(const SMikkTSpaceContext* ctx)
{
  auto* ud = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
  return ud->num_faces;
}

static int mts_getNumVerticesOfFace(const SMikkTSpaceContext*, int)
{
  return 3;
}

static void mts_getPosition(const SMikkTSpaceContext* ctx, float out[], int iFace, int iVert)
{
  auto* ud = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
  int idx = iFace * 3 + iVert;
  out[0] = ud->positions[idx * 3 + 0];
  out[1] = ud->positions[idx * 3 + 1];
  out[2] = ud->positions[idx * 3 + 2];
}

static void mts_getNormal(const SMikkTSpaceContext* ctx, float out[], int iFace, int iVert)
{
  auto* ud = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
  int idx = iFace * 3 + iVert;
  out[0] = ud->normals[idx * 3 + 0];
  out[1] = ud->normals[idx * 3 + 1];
  out[2] = ud->normals[idx * 3 + 2];
}

static void mts_getTexCoord(const SMikkTSpaceContext* ctx, float out[], int iFace, int iVert)
{
  auto* ud = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
  int idx = iFace * 3 + iVert;
  out[0] = ud->texcoords[idx * 2 + 0];
  out[1] = ud->texcoords[idx * 2 + 1];
}

static void mts_setTSpaceBasic(
    const SMikkTSpaceContext* ctx, const float tangent[], float fSign,
    int iFace, int iVert)
{
  auto* ud = static_cast<MikkTSpaceUserData*>(ctx->m_pUserData);
  int idx = iFace * 3 + iVert;
  ud->tangents[idx * 4 + 0] = tangent[0];
  ud->tangents[idx * 4 + 1] = tangent[1];
  ud->tangents[idx * 4 + 2] = tangent[2];
  ud->tangents[idx * 4 + 3] = fSign;
}

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
  const bool gen_tangents = texcoords && normals;

  std::size_t float_count = total_vertices * 3 + (normals ? total_vertices * 3 : 0)
                            + (texcoords ? total_vertices * 2 : 0)
                            + (gen_tangents ? total_vertices * 4 : 0);

  // 3 float per vertex for position, 3 per vertex for normal
  buf.clear();
  buf.resize(float_count, 0.); // boost::container::default_init);

  int64_t pos_offset = 0;
  int64_t texcoord_offset = 0;
  int64_t normal_offset = 0;
  int64_t tangent_offset = 0;
  if (texcoords)
  {
    texcoord_offset = total_vertices * 3;
    if (normals)
    {
      normal_offset = texcoord_offset + total_vertices * 2;
      tangent_offset = normal_offset + total_vertices * 3;
    }
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
         .tangent_offset = tangent_offset,
         .texcoord = texcoords,
         .normals = normals,
         .tangents = gen_tangents});

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
      tangent_offset += vertices * 4;
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

  // Generate tangents via MikkTSpace for shapes that have both texcoords and normals
  if (gen_tangents)
  {
    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces = mts_getNumFaces;
    iface.m_getNumVerticesOfFace = mts_getNumVerticesOfFace;
    iface.m_getPosition = mts_getPosition;
    iface.m_getNormal = mts_getNormal;
    iface.m_getTexCoord = mts_getTexCoord;
    iface.m_setTSpaceBasic = mts_setTSpaceBasic;

    for (auto& m : res)
    {
      if (!m.tangents)
        continue;

      MikkTSpaceUserData ud;
      ud.positions = buf.data() + m.pos_offset;
      ud.texcoords = buf.data() + m.texcoord_offset;
      ud.normals = buf.data() + m.normal_offset;
      ud.tangents = buf.data() + m.tangent_offset;
      ud.num_faces = m.vertices / 3;

      SMikkTSpaceContext ctx;
      ctx.m_pInterface = &iface;
      ctx.m_pUserData = &ud;

      genTangSpaceDefault(&ctx);
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
