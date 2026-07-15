#pragma once
#include <mikktspace.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Threedim
{

// Generate glTF-compatible float4 tangents (xyz = unit tangent, w =
// handedness ±1) using mikktspace from a mesh's position / normal /
// texcoord_0 streams and an optional uint32 index buffer. Returns a
// shared buffer of `vertex_count * 4` floats, or nullptr on failure
// (missing streams, degenerate mesh, etc).
//
// For indexed meshes: mikktspace's contract is unindexed ("DO NOT use
// an already existing index list"), but we're constrained to keep
// indexed data. We call the mikktspace callbacks against the EXPANDED
// (unindexed) triangle list via the index buffer, and write the
// generated tangent back through the same index lookup. When two
// triangles share a vertex with the same tangent (smooth surface),
// successive writes produce the same value. At UV seams they disagree
// and the last write wins — a known small artifact compared to
// un-indexing the whole mesh. Vertex duplication on import is a
// future enhancement tracked in docs/3d-pipeline-tasks.md.
inline std::shared_ptr<std::vector<float>> generate_tangents_mikktspace(
    const std::shared_ptr<std::vector<float>>& positions,
    const std::shared_ptr<std::vector<float>>& normals,
    const std::shared_ptr<std::vector<float>>& texcoords,
    const std::shared_ptr<std::vector<uint32_t>>& indices,
    uint32_t vertex_count)
{
  if(!positions || !normals || !texcoords || vertex_count == 0)
    return {};
  if(positions->size() < vertex_count * 3
     || normals->size() < vertex_count * 3
     || texcoords->size() < vertex_count * 2)
    return {};

  // Triangle count: indexed → indices/3, non-indexed → vertex_count/3.
  const uint32_t num_faces
      = indices ? uint32_t(indices->size() / 3)
                : uint32_t(vertex_count / 3);
  if(num_faces == 0)
    return {};

  // Index values come straight from the asset file. Reject any that
  // exceed the vertex streams: the accessor callbacks below read them
  // unchecked and m_setTSpaceBasic writes through them into the
  // vertex_count*4 tangent buffer.
  if(indices)
    for(const uint32_t v : *indices)
      if(v >= vertex_count)
        return {};

  auto tangents = std::make_shared<std::vector<float>>(vertex_count * 4, 0.f);

  struct UserData
  {
    const float* positions;
    const float* normals;
    const float* texcoords;
    const uint32_t* indices; // null when un-indexed
    uint32_t num_faces;
    std::vector<float>* tangents;
  };
  UserData ud{positions->data(),
              normals->data(),
              texcoords->data(),
              indices ? indices->data() : nullptr,
              num_faces,
              tangents.get()};

  auto vertexIndex
      = [](const UserData& u, int iFace, int iVert) -> uint32_t {
    const uint32_t flat = uint32_t(iFace) * 3u + uint32_t(iVert);
    return u.indices ? u.indices[flat] : flat;
  };

  SMikkTSpaceInterface iface{};
  iface.m_getNumFaces = [](const SMikkTSpaceContext* ctx) {
    return int(static_cast<const UserData*>(ctx->m_pUserData)->num_faces);
  };
  iface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) {
    return 3;
  };
  iface.m_getPosition = [](const SMikkTSpaceContext* ctx, float out[],
                           int iFace, int iVert) {
    auto& u = *static_cast<const UserData*>(ctx->m_pUserData);
    auto vi = uint32_t(iFace) * 3u + uint32_t(iVert);
    auto v = u.indices ? u.indices[vi] : vi;
    out[0] = u.positions[v * 3 + 0];
    out[1] = u.positions[v * 3 + 1];
    out[2] = u.positions[v * 3 + 2];
  };
  iface.m_getNormal = [](const SMikkTSpaceContext* ctx, float out[],
                         int iFace, int iVert) {
    auto& u = *static_cast<const UserData*>(ctx->m_pUserData);
    auto vi = uint32_t(iFace) * 3u + uint32_t(iVert);
    auto v = u.indices ? u.indices[vi] : vi;
    out[0] = u.normals[v * 3 + 0];
    out[1] = u.normals[v * 3 + 1];
    out[2] = u.normals[v * 3 + 2];
  };
  iface.m_getTexCoord = [](const SMikkTSpaceContext* ctx, float out[],
                           int iFace, int iVert) {
    auto& u = *static_cast<const UserData*>(ctx->m_pUserData);
    auto vi = uint32_t(iFace) * 3u + uint32_t(iVert);
    auto v = u.indices ? u.indices[vi] : vi;
    out[0] = u.texcoords[v * 2 + 0];
    out[1] = u.texcoords[v * 2 + 1];
  };
  iface.m_setTSpaceBasic = [](const SMikkTSpaceContext* ctx,
                              const float tangent[], float sign,
                              int iFace, int iVert) {
    auto& u = *static_cast<const UserData*>(ctx->m_pUserData);
    auto vi = uint32_t(iFace) * 3u + uint32_t(iVert);
    auto v = u.indices ? u.indices[vi] : vi;
    auto& t = *u.tangents;
    t[v * 4 + 0] = tangent[0];
    t[v * 4 + 1] = tangent[1];
    t[v * 4 + 2] = tangent[2];
    t[v * 4 + 3] = sign;
  };
  (void)vertexIndex;

  SMikkTSpaceContext ctx{&iface, &ud};
  if(!genTangSpaceDefault(&ctx))
    return {};

  // Fallback for vertices never touched (rare; mostly for non-manifold
  // meshes): orient any zero tangent along X so shader doesn't divide
  // by zero when reconstructing the TBN.
  for(uint32_t v = 0; v < vertex_count; ++v)
  {
    float* t = tangents->data() + v * 4;
    const float len2 = t[0] * t[0] + t[1] * t[1] + t[2] * t[2];
    if(len2 < 1e-10f)
    {
      t[0] = 1.f; t[1] = 0.f; t[2] = 0.f; t[3] = 1.f;
    }
  }
  return tangents;
}

}
