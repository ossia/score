#include "VcgImporters.hpp"

// vcglib pulls Qt / GL through its utility headers; we only need the
// header-only trimesh + io_trimesh subset. Isolate these includes here so
// the rest of the plugin isn't exposed to vcglib's macro soup.
#include <vcg/complex/complex.h>
#include <wrap/io_trimesh/import_off.h>
#include <wrap/io_trimesh/import_stl.h>

#include <string>

namespace Threedim
{

namespace
{

// Minimal vcglib mesh type for STL / OFF import: per-vertex position +
// normal + colour + bit flags, per-face vertex refs + normal + colour.
// STL contributes position + per-face normal; OFF can contribute per-vertex
// and per-face colours. We always request normals + colours; vcglib zero-
// inits any it doesn't fill.
class ImpVertex;
class ImpFace;
struct ImpTypes : public vcg::UsedTypes<
                      vcg::Use<ImpVertex>::AsVertexType,
                      vcg::Use<ImpFace>::AsFaceType>
{};
class ImpVertex : public vcg::Vertex<
                      ImpTypes, vcg::vertex::Coord3f, vcg::vertex::Normal3f,
                      vcg::vertex::Color4b, vcg::vertex::BitFlags>
{};
class ImpFace : public vcg::Face<
                    ImpTypes, vcg::face::VertexRef, vcg::face::Normal3f,
                    vcg::face::Color4b, vcg::face::BitFlags>
{};
class ImpMesh : public vcg::tri::TriMesh<
                    std::vector<ImpVertex>, std::vector<ImpFace>>
{};

// Expand the loaded vcglib mesh into the flat, non-interleaved float_vec
// layout Threedim::mesh expects: all positions, then all normals, then
// all colours. De-indexed (one output vertex per triangle corner) because
// STL doesn't carry per-vertex normals shared across triangles, and OFF
// often has smooth normals but STL's "one normal per face" forces the
// per-corner expansion anyway.
static std::vector<Threedim::mesh>
convertVcgToMeshes(const ImpMesh& vm, Threedim::float_vec& out, int loadmask)
{
  std::vector<Threedim::mesh> result;
  if(vm.face.empty() && vm.vert.empty())
    return result;

  // Count output vertices — one per triangle corner (de-indexed).
  const bool has_faces = !vm.face.empty();
  const bool has_normal = (loadmask & vcg::tri::io::Mask::IOM_VERTNORMAL)
                          || (loadmask & vcg::tri::io::Mask::IOM_FACENORMAL);
  const bool has_color = (loadmask & vcg::tri::io::Mask::IOM_VERTCOLOR)
                         || (loadmask & vcg::tri::io::Mask::IOM_FACECOLOR);

  Threedim::mesh m{};
  m.texcoord = false;
  m.normals  = has_normal;
  m.colors   = has_color;
  m.tangents = false;
  m.points   = !has_faces;
  m.extras.clear();

  if(has_faces)
  {
    const size_t corners = vm.face.size() * 3;
    m.vertices = (int64_t)corners;

    // Allocate contiguous attribute blocks. Layout matches Threedim::mesh's
    // convention: offsets stored in elements (floats), not bytes.
    const int64_t pos_count    = 3 * corners;
    const int64_t nor_count    = has_normal ? 3 * corners : 0;
    const int64_t col_count    = has_color  ? 4 * corners : 0;
    const int64_t total_floats = pos_count + nor_count + col_count;

    const int64_t pos_offset = (int64_t)out.size();
    const int64_t nor_offset = pos_offset + pos_count;
    const int64_t col_offset = nor_offset + nor_count;

    out.resize(pos_offset + total_floats);

    m.pos_offset     = pos_offset;
    m.normal_offset  = has_normal ? nor_offset : 0;
    m.color_offset   = has_color  ? col_offset : 0;

    // Fill buffer by walking faces.
    for(size_t fi = 0; fi < vm.face.size(); ++fi)
    {
      const auto& f = vm.face[fi];

      // Use face normal as per-corner normal if per-vertex is unavailable
      // (STL case). vcglib's ImporterSTL computes per-face normals.
      const bool have_face_normal
          = loadmask & vcg::tri::io::Mask::IOM_FACENORMAL;

      for(int c = 0; c < 3; ++c)
      {
        const auto* v = f.cV(c);
        const int64_t base_p = pos_offset + (fi * 3 + c) * 3;
        out[base_p + 0] = (float)v->cP()[0];
        out[base_p + 1] = (float)v->cP()[1];
        out[base_p + 2] = (float)v->cP()[2];

        if(has_normal)
        {
          const int64_t base_n = nor_offset + (fi * 3 + c) * 3;
          const auto& n = have_face_normal ? f.cN() : v->cN();
          out[base_n + 0] = (float)n[0];
          out[base_n + 1] = (float)n[1];
          out[base_n + 2] = (float)n[2];
        }

        if(has_color)
        {
          const int64_t base_c = col_offset + (fi * 3 + c) * 4;
          const bool have_face_color
              = loadmask & vcg::tri::io::Mask::IOM_FACECOLOR;
          const auto& cc = have_face_color ? f.cC() : v->cC();
          out[base_c + 0] = cc[0] / 255.0f;
          out[base_c + 1] = cc[1] / 255.0f;
          out[base_c + 2] = cc[2] / 255.0f;
          out[base_c + 3] = cc[3] / 255.0f;
        }
      }
    }
  }
  else
  {
    // Point cloud (no faces). Emit one vertex per input vertex.
    const size_t nv = vm.vert.size();
    m.vertices = (int64_t)nv;
    const int64_t pos_count    = 3 * nv;
    const int64_t nor_count    = has_normal ? 3 * nv : 0;
    const int64_t col_count    = has_color  ? 4 * nv : 0;
    const int64_t total_floats = pos_count + nor_count + col_count;

    const int64_t pos_offset = (int64_t)out.size();
    const int64_t nor_offset = pos_offset + pos_count;
    const int64_t col_offset = nor_offset + nor_count;
    out.resize(pos_offset + total_floats);
    m.pos_offset     = pos_offset;
    m.normal_offset  = has_normal ? nor_offset : 0;
    m.color_offset   = has_color  ? col_offset : 0;

    for(size_t i = 0; i < nv; ++i)
    {
      const auto& v = vm.vert[i];
      out[pos_offset + i * 3 + 0] = (float)v.cP()[0];
      out[pos_offset + i * 3 + 1] = (float)v.cP()[1];
      out[pos_offset + i * 3 + 2] = (float)v.cP()[2];

      if(has_normal)
      {
        out[nor_offset + i * 3 + 0] = (float)v.cN()[0];
        out[nor_offset + i * 3 + 1] = (float)v.cN()[1];
        out[nor_offset + i * 3 + 2] = (float)v.cN()[2];
      }

      if(has_color)
      {
        out[col_offset + i * 4 + 0] = v.cC()[0] / 255.0f;
        out[col_offset + i * 4 + 1] = v.cC()[1] / 255.0f;
        out[col_offset + i * 4 + 2] = v.cC()[2] / 255.0f;
        out[col_offset + i * 4 + 3] = v.cC()[3] / 255.0f;
      }
    }
  }

  result.push_back(std::move(m));
  return result;
}

template <int (*OpenFn)(ImpMesh&, const char*, int&, vcg::CallBackPos*)>
std::vector<Threedim::mesh>
importVcgGeneric(std::string_view filename, Threedim::float_vec& out)
{
  ImpMesh vm;
  int loadmask = 0;
  const std::string path{filename};
  const int err = OpenFn(vm, path.c_str(), loadmask, nullptr);
  if(err != 0)
    return {};
  return convertVcgToMeshes(vm, out, loadmask);
}

// Wrappers to pin the importer function pointer signature.
static int openStl(ImpMesh& m, const char* p, int& mask, vcg::CallBackPos* cb)
{
  return vcg::tri::io::ImporterSTL<ImpMesh>::Open(m, p, mask, cb);
}
static int openOff(ImpMesh& m, const char* p, int& mask, vcg::CallBackPos* cb)
{
  return vcg::tri::io::ImporterOFF<ImpMesh>::Open(m, p, mask, cb);
}

} // namespace

std::vector<Threedim::mesh>
StlFromFile(std::string_view filename, Threedim::float_vec& buffer)
{
  return importVcgGeneric<&openStl>(filename, buffer);
}

std::vector<Threedim::mesh>
OffFromFile(std::string_view filename, Threedim::float_vec& buffer)
{
  return importVcgGeneric<&openOff>(filename, buffer);
}

} // namespace Threedim
