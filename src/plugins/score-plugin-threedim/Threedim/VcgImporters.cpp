#include "VcgImporters.hpp"

// vcglib pulls Qt / GL through its utility headers; we only need the
// header-only trimesh + io_trimesh subset. Isolate these includes here so
// the rest of the plugin isn't exposed to vcglib's macro soup.
#include <vcg/complex/complex.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <wrap/io_trimesh/import_off.h>
#include <wrap/io_trimesh/import_stl.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

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
  m.color_components = 4;
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
  const int err = vcg::tri::io::ImporterSTL<ImpMesh>::Open(m, p, mask, cb);
  // STL defines one normal per facet, but vcglib's importer discards the
  // stored value and never sets IOM_FACENORMAL. Recompute from the winding,
  // which the STL spec requires to agree with the stored normal.
  if(err == 0 && !m.face.empty())
  {
    vcg::tri::UpdateNormal<ImpMesh>::PerFaceNormalized(m);
    mask |= vcg::tri::io::Mask::IOM_FACENORMAL;
  }
  return err;
}
// vcglib's ImporterOFF indexes tokens[] without bounds checks in its face
// section (import_off.h:440 reads tokens[0] of a possibly-empty EOF line;
// :451-460 read tokens[1..3] of a face line shorter than announced; :472-474
// dereference mesh.vert[idx] BEFORE validating idx for polygons). A file
// truncated mid-face-list is therefore an out-of-bounds vector read: a
// libstdc++ assertion abort in debug, silent UB in release — found by
// tests/unit/AssetLoaderFailure.cpp's truncation matrix. vcglib is a vendored
// submodule, so the guard lives here: for a plain "OFF" header, walk the file
// with the exact tokenization the importer uses and refuse it when the face
// section would index past a line's tokens. Variant headers (NOFF/COFF/STOFF/
// 4OFF) have data-dependent per-vertex token counts; they pass through
// unvalidated rather than risking rejection of a valid file.
static void offTokenizeNextLine(
    std::istream& stream, std::vector<std::string>& tokens)
{
  // Byte-for-byte the semantics of ImporterOFF::TokenizeNextLine.
  std::string line;
  do
    std::getline(stream, line, '\n');
  while((line.empty() || line[0] == '#' || line[0] == '\r') && !stream.eof());

  tokens.clear();
  std::size_t from = 0;
  const std::size_t length = line.size();
  while(from < length)
  {
    while(from != length
          && (line[from] == ' ' || line[from] == '\t' || line[from] == '\r'))
      from++;
    if(from != length)
    {
      std::size_t to = from + 1;
      while(to != length
            && (((line[to] != ' ') && (line[to] != '\t')) || (line[to] == '\r')))
        to++;
      tokens.push_back(line.substr(from, to - from));
      from = to;
    }
  }
}

static bool offStructureIsSane(const char* path)
{
  std::ifstream stream{path};
  if(!stream)
    return true; // let vcglib report the open failure itself

  std::vector<std::string> tokens;
  offTokenizeNextLine(stream, tokens);
  if(tokens.empty())
    return true; // InvalidFile_MissingOFF, handled safely by vcglib
  if(tokens[0] != "OFF")
    return true; // variant header: pass through (see the comment above)

  // Counts, possibly on the header line ("OFF 3 1 0").
  if(tokens.size() == 1)
    offTokenizeNextLine(stream, tokens);
  else
    tokens.erase(tokens.begin());
  if(tokens.size() < 3)
    return true; // vcglib returns InvalidFile safely
  const long nVertices = std::atol(tokens[0].c_str());
  const long nFaces = std::atol(tokens[1].c_str());
  if(nVertices < 0 || nFaces < 0)
    return true;

  // Vertex section: a flat token stream, 3 coordinates per vertex; vcglib's
  // own EOL/EOF handling there is safe (returns InvalidFile), so we only
  // advance the cursor the same way it does.
  offTokenizeNextLine(stream, tokens);
  std::size_t k = 0;
  for(long i = 0; i < nVertices; i++)
  {
    for(int j = 0; j < 3; j++)
    {
      if(k == tokens.size())
      {
        offTokenizeNextLine(stream, tokens);
        if(tokens.empty())
          return true; // vcglib returns InvalidFile safely
        k = 0;
      }
      k++;
    }
  }

  // Face section: one TokenizeNextLine per face, then unchecked indexing.
  for(long f = 0; f < nFaces; f++)
  {
    offTokenizeNextLine(stream, tokens);
    if(tokens.empty())
      return false; // import_off.h:440 would read tokens[0] out of bounds
    const long n = std::atol(tokens[0].c_str());
    if(n < 2)
      return true; // ErrorDegenerateFace, handled safely by vcglib
    if(tokens.size() == 1)
    {
      // Count on its own line: vcglib appends the next line's tokens and
      // only checks tokens.size() + 1 >= n before indexing tokens[1..n].
      std::vector<std::string> more;
      offTokenizeNextLine(stream, more);
      for(auto& t : more)
        tokens.push_back(std::move(t));
    }
    if(std::size_t(n) + 1 > tokens.size())
      return false; // tokens[1..n] would be out of bounds
    if(n > 3)
    {
      // Polygon path dereferences mesh.vert[index] before validating it.
      for(long j = 1; j <= n; j++)
      {
        const long idx = std::atol(tokens[std::size_t(j)].c_str());
        if(idx < 0 || idx >= nVertices)
          return false;
      }
    }
  }
  return true;
}

static int openOff(ImpMesh& m, const char* p, int& mask, vcg::CallBackPos* cb)
{
  if(!offStructureIsSane(p))
    return vcg::tri::io::ImporterOFF<ImpMesh>::InvalidFile;
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
