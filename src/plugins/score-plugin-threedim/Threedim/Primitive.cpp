#include "Primitive.hpp"

#include <Threedim/MeshHelpers.hpp>
#include <Threedim/TinyObj.hpp>

#include <QDebug>
#include <QString>

namespace Threedim
{
static auto createMesh(TMesh& mesh, std::vector<float>& complete)
{
  vcg::tri::Clean<TMesh>::RemoveUnreferencedVertex(mesh);
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
  const auto vertices = mesh.face.size() * 3;
  const auto floats
      = vertices
        * (3 + 3
           + 2); // 3 float for pos, 3 float for normal, 2 float for UV. Times 3 as three vertices per face.
  complete.resize(floats);
  float* pos_start = complete.data();
  float* norm_start = complete.data() + vertices * 3;
  float* uv_start = complete.data() + vertices * 3 + vertices * 3;

  for(auto& fi : mesh.face)
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

  return std::make_tuple(vertices, pos_start, norm_start, uv_start);
}

void loadTriMesh(TMesh& mesh, std::vector<float>& complete, PrimitiveOutputs& outputs)
{
  auto [vertices, pos_start, norm_start, uv_start] = createMesh(mesh, complete);
  outputs.geometry.mesh.buffers.main_buffer.data = complete.data();
  outputs.geometry.mesh.buffers.main_buffer.size = complete.size();
  outputs.geometry.mesh.buffers.main_buffer.dirty = true;

  outputs.geometry.mesh.input.input0.offset = 0;
  outputs.geometry.mesh.input.input1.offset = sizeof(float) * vertices * 3;
  outputs.geometry.mesh.input.input2.offset = sizeof(float) * vertices * (3 + 3);
  outputs.geometry.mesh.vertices = vertices;
  outputs.geometry.dirty_mesh = true;
}

static thread_local TMesh mesh;
void Plane::update()
{
  /*
  // clang-format off
  static const constexpr float data[] = {
    // positions
    -1, -1, 0,
    +1, -1, 0,
    -1, +1, 0,
    +1, +1, 0,
    // tex coords
    0, 0,
    1, 0,
    0, 1,
    1, 1
  };
  // clang-format on

  outputs.geometry.mesh.buffers.main_buffer.data = (float*)data;
  outputs.geometry.mesh.buffers.main_buffer.size = std::ssize(data);
  outputs.geometry.mesh.buffers.main_buffer.dirty = true;

  outputs.geometry.mesh.input.input1.offset = 12 * sizeof(float);
  outputs.geometry.mesh.vertices = 4;
  outputs.geometry.dirty_mesh = true;
  */
  mesh.Clear();
  vcg::tri::Grid(mesh, inputs.hdivs, inputs.vdivs, 1., 1.);
  auto [vertices, pos_start, norm_start, uv_start] = createMesh(mesh, complete);
  outputs.geometry.mesh.buffers.main_buffer.data = complete.data();
  outputs.geometry.mesh.buffers.main_buffer.size = complete.size();
  outputs.geometry.mesh.buffers.main_buffer.dirty = true;

  outputs.geometry.mesh.input.input0.offset = 0;
  outputs.geometry.mesh.input.input1.offset = sizeof(float) * vertices * 3;
  outputs.geometry.mesh.input.input2.offset = sizeof(float) * vertices * (3 + 3);
  outputs.geometry.mesh.vertices = vertices;
  outputs.geometry.dirty_mesh = true;
}

void Cube::update()
{
  mesh.Clear();
  vcg::Box3<float> box;
  box.min = {-1, -1, -1};
  box.max = {1, 1, 1};
  vcg::tri::Box(mesh, box);
  loadTriMesh(mesh, complete, outputs);
}

void Sphere::update()
{
  mesh.Clear();
  vcg::tri::Sphere(mesh, inputs.subdiv);
  loadTriMesh(mesh, complete, outputs);
}

void Icosahedron::update()
{
  mesh.Clear();
  vcg::tri::Icosahedron(mesh);
  loadTriMesh(mesh, complete, outputs);
}

void Cone::update()
{
  mesh.Clear();
  vcg::tri::Cone(mesh, inputs.r1, inputs.r2, inputs.h, inputs.subdiv);
  loadTriMesh(mesh, complete, outputs);
}

void Cylinder::update()
{
  mesh.Clear();
  vcg::tri::Cylinder(inputs.slices, inputs.stacks, mesh, true);
  loadTriMesh(mesh, complete, outputs);
}

void Torus::update()
{
  mesh.Clear();
  vcg::tri::Torus(mesh, inputs.r1, inputs.r2, inputs.hdiv, inputs.vdiv);
  loadTriMesh(mesh, complete, outputs);
}

}
