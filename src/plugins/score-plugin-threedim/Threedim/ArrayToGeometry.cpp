#include "ArrayToGeometry.hpp"

#include <Threedim/MeshHelpers.hpp>
#include <Threedim/TinyObj.hpp>
#include <vcg/complex/algorithms/create/ball_pivoting.h>
#include <vcg/complex/complex.h>

#include <QDebug>
#include <QString>

namespace Threedim
{
static thread_local TMesh array_to_mesh;

void loadPoints(TMesh& mesh, std::vector<float>& complete, PrimitiveOutputs& outputs)
{
  vcg::tri::Clean<TMesh>::RemoveZeroAreaFace(mesh);
  vcg::tri::UpdateTopology<TMesh>::FaceFace(mesh);
  vcg::tri::Clean<TMesh>::RemoveNonManifoldFace(mesh);
  vcg::tri::UpdateTopology<TMesh>::FaceFace(mesh);
  vcg::tri::UpdateNormal<TMesh>::PerVertexNormalized(mesh);
  vcg::tri::UpdateTexture<TMesh>::WedgeTexFromPlane(
      mesh, vcg::Point3f{0., 0., 0.}, vcg::Point3f{1., 1., 1.}, 1.);

  vcg::tri::RequirePerVertexNormal(mesh);
  vcg::tri::RequirePerVertexTexCoord(mesh);

  complete.clear();
  const auto vertices = mesh.vert.size();
  const auto floats
      = vertices * (3 + 3 + 2); // 3 float for pos, 3 float for normal, 2 float for UV
  complete.resize(floats);
  float* pos_start = complete.data();
  float* norm_start = complete.data() + vertices * 3;
  float* uv_start = complete.data() + vertices * 3 + vertices * 3;

  for (auto& fi : mesh.face)
  { // iterate each faces

    auto v0 = fi.V(0);
    auto v1 = fi.V(1);
    auto v2 = fi.V(2);

    auto p0 = v0->P();
    (*pos_start++) = p0.X();
    (*pos_start++) = p0.Y();
    (*pos_start++) = p0.Z();

    auto p1 = v1->P();
    (*pos_start++) = p1.X();
    (*pos_start++) = p1.Y();
    (*pos_start++) = p1.Z();

    auto p2 = v2->P();
    (*pos_start++) = p2.X();
    (*pos_start++) = p2.Y();
    (*pos_start++) = p2.Z();

    auto n0 = v0->N();
    (*norm_start++) = n0.X();
    (*norm_start++) = n0.Y();
    (*norm_start++) = n0.Z();

    auto n1 = v1->N();
    (*norm_start++) = n1.X();
    (*norm_start++) = n1.Y();
    (*norm_start++) = n1.Z();

    auto n2 = v2->N();
    (*norm_start++) = n2.X();
    (*norm_start++) = n2.Y();
    (*norm_start++) = n2.Z();

#if 0
    auto uv0 = fi.WT(0);
    (*uv_start++) = uv0.U();
    (*uv_start++) = uv0.V();

    auto uv1 = fi.WT(1);
    (*uv_start++) = uv1.U();
    (*uv_start++) = uv1.V();

    auto uv2 = fi.WT(2);
    (*uv_start++) = uv2.U();
    (*uv_start++) = uv2.V();
#endif
    (*uv_start++) = p0.X();
    (*uv_start++) = p0.Y();

    (*uv_start++) = p1.X();
    (*uv_start++) = p1.Y();

    (*uv_start++) = p2.X();
    (*uv_start++) = p2.Y();
  }
  outputs.geometry.mesh.buffers.main_buffer.elements = complete.data();
  outputs.geometry.mesh.buffers.main_buffer.element_count = complete.size();
  outputs.geometry.mesh.buffers.main_buffer.dirty = true;

  outputs.geometry.mesh.input.input0.byte_offset = 0;
  outputs.geometry.mesh.input.input1.byte_offset = sizeof(float) * vertices * 3;
  outputs.geometry.mesh.input.input2.byte_offset = sizeof(float) * vertices * (3 + 3);
  outputs.geometry.mesh.vertices = vertices;
  outputs.geometry.dirty_mesh = true;
}
void ArrayToMesh::create_mesh(std::span<float> v)
{
  if (v.size() < 3)
    return;

  if (inputs.triangulate)
  {
    auto& m = array_to_mesh;
    m.Clear();
    vcg::tri::Allocator<TMesh>::AddVertices(m, v.size() / 3);

    for (std::size_t i = 0; i < v.size(); i += 3)
    {
      auto& vtx = m.vert[i / 3].P();
      vtx.X() = v[i + 0];
      vtx.Y() = v[i + 1];
      vtx.Z() = v[i + 2];
    }
    vcg::tri::UpdateBounding<TMesh>::Box(m);
    vcg::tri::UpdateNormal<TMesh>::PerFace(m);

    vcg::tri::BallPivoting<TMesh> pivot(m, 0.01, 0.05);
    pivot.BuildMesh();
    loadTriMesh(m, complete, outputs);
  }
  else
  {
    std::size_t vertices = v.size() / 3;

    this->complete.clear();
    this->complete.resize(std::ceil((v.size() / 3.) * (3 + 3 + 2)));
    std::copy_n(v.begin(), v.size(), complete.begin());

    outputs.geometry.mesh.buffers.main_buffer.elements = complete.data();
    outputs.geometry.mesh.buffers.main_buffer.element_count = complete.size();
    outputs.geometry.mesh.buffers.main_buffer.dirty = true;

    outputs.geometry.mesh.input.input0.byte_offset = 0;
    outputs.geometry.mesh.input.input1.byte_offset = sizeof(float) * vertices * 3;
    outputs.geometry.mesh.input.input2.byte_offset = sizeof(float) * vertices * (3 + 3);
    outputs.geometry.mesh.vertices = vertices;
    outputs.geometry.dirty_mesh = true;
  }
}
}
