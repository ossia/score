#include "Primitive.hpp"

#include <Threedim/MeshHelpers.hpp>
#include <Threedim/TinyObj.hpp>

#include <QDebug>

#include <cmath>
#include <QString>

namespace Threedim
{
static auto createMesh(TMesh& mesh, std::vector<float>& complete)
{
  vcg::tri::Clean<TMesh>::RemoveUnreferencedVertex(mesh);
  vcg::tri::Clean<TMesh>::RemoveZeroAreaFace(mesh);
  vcg::tri::UpdateTopology<TMesh>::FaceFace(mesh);
  vcg::tri::Clean<TMesh>::RemoveNonManifoldFace(mesh);
  // vcglib deletes lazily — a flag on the element, no compaction — so
  // without this the fill loop below would expand deleted faces into the
  // published buffer as if the three cleanup calls above had never run.
  vcg::tri::Allocator<TMesh>::CompactEveryVector(mesh);
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

    // Per-face parameterization: project each corner onto the plane
    // orthogonal to the face normal's dominant axis (box mapping).
    // A global uv = pos.xy would collapse every face parallel to the Z
    // axis (e.g. 8 of the cube's 12 triangles) to zero UV area; for a
    // Z-facing surface such as the Plane this reduces to (x, y), its
    // natural parameterization.
    const auto fnrm = (p1 - p0) ^ (p2 - p0);
    const float ax = std::abs(fnrm.X());
    const float ay = std::abs(fnrm.Y());
    const float az = std::abs(fnrm.Z());
    int u_axis = 0, v_axis = 1; // normal mostly Z -> (x, y)
    if(ax >= ay && ax >= az)
    {
      u_axis = 1; // normal mostly X -> (y, z)
      v_axis = 2;
    }
    else if(ay >= ax && ay >= az)
    {
      u_axis = 0; // normal mostly Y -> (x, z)
      v_axis = 2;
    }

    (*uv_start++) = p0[u_axis];
    (*uv_start++) = p0[v_axis];

    (*uv_start++) = p1[u_axis];
    (*uv_start++) = p1[v_axis];

    (*uv_start++) = p2[u_axis];
    (*uv_start++) = p2[v_axis];
  }

  return std::make_tuple(vertices, pos_start, norm_start, uv_start);
}

void loadTriMesh(TMesh& mesh, std::vector<float>& complete, PrimitiveOutputs& outputs)
{
  auto [vertices, pos_start, norm_start, uv_start] = createMesh(mesh, complete);
  outputs.geometry.mesh.buffers.main_buffer.elements = complete.data();
  outputs.geometry.mesh.buffers.main_buffer.element_count = complete.size();
  outputs.geometry.mesh.buffers.main_buffer.dirty = true;

  outputs.geometry.mesh.input.input0.byte_offset = 0;
  outputs.geometry.mesh.input.input1.byte_offset = sizeof(float) * vertices * 3;
  outputs.geometry.mesh.input.input2.byte_offset = sizeof(float) * vertices * (3 + 3);
  outputs.geometry.mesh.vertices = vertices;
  outputs.geometry.dirty_mesh = true;
}

static thread_local TMesh g_tmpMesh;
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
  g_tmpMesh.Clear();

  const int hdivs = std::max(2, (int)inputs.hdivs);
  const int vdivs = std::max(2, (int)inputs.vdivs);
  vcg::tri::Grid(g_tmpMesh, hdivs, vdivs, 1., 1.);
  auto [vertices, pos_start, norm_start, uv_start] = createMesh(g_tmpMesh, complete);
  outputs.geometry.mesh.buffers.main_buffer.elements = complete.data();
  outputs.geometry.mesh.buffers.main_buffer.element_count = complete.size();
  outputs.geometry.mesh.buffers.main_buffer.dirty = true;

  outputs.geometry.mesh.input.input0.byte_offset = 0;
  outputs.geometry.mesh.input.input1.byte_offset = sizeof(float) * vertices * 3;
  outputs.geometry.mesh.input.input2.byte_offset = sizeof(float) * vertices * (3 + 3);
  outputs.geometry.mesh.vertices = vertices;
  outputs.geometry.dirty_mesh = true;
}

void Cube::update()
{
  g_tmpMesh.Clear();
  vcg::Box3<float> box;
  box.min = {0, 0, 0};
  box.max = {1, 1, 1};
  vcg::tri::Box(g_tmpMesh, box);
  loadTriMesh(g_tmpMesh, complete, outputs);
}

void Sphere::update()
{
  g_tmpMesh.Clear();
  vcg::tri::Sphere(g_tmpMesh, inputs.subdiv);
  loadTriMesh(g_tmpMesh, complete, outputs);
}

void Icosahedron::update()
{
  g_tmpMesh.Clear();
  vcg::tri::Icosahedron(g_tmpMesh);
  loadTriMesh(g_tmpMesh, complete, outputs);
}

void Cone::update()
{
  g_tmpMesh.Clear();
  vcg::tri::Cone(g_tmpMesh, inputs.r1, inputs.r2, inputs.h, inputs.subdiv);
  loadTriMesh(g_tmpMesh, complete, outputs);
}

void Cylinder::update()
{
  g_tmpMesh.Clear();
  vcg::tri::Cylinder(inputs.slices, inputs.stacks, g_tmpMesh, true);
  loadTriMesh(g_tmpMesh, complete, outputs);
}

void Torus::update()
{
  g_tmpMesh.Clear();
  vcg::tri::Torus(g_tmpMesh, inputs.r1, inputs.r2, inputs.hdiv, inputs.vdiv);
  loadTriMesh(g_tmpMesh, complete, outputs);
}

}
