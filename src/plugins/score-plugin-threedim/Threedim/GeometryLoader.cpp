#include "GeometryLoader.hpp"

#include <QMatrix4x4>
#include <cmath>
#include <QString>

#include <Threedim/Debug.hpp>
#include <Threedim/Ply.hpp>
#include <Threedim/VcgImporters.hpp>

namespace Threedim
{

void GeometryLoader::rebuild_geometry()
{
  std::vector<mesh>& new_meshes = this->meshinfo;

  if(!outputs.geometry.mesh.empty())
  {
    outputs.geometry.mesh.clear();
  }

  for(auto& m : new_meshes)
  {
    if(m.vertices <= 0)
      continue;

    halp::dynamic_geometry geom;

    geom.buffers.clear();
    geom.bindings.clear();
    geom.attributes.clear();
    geom.input.clear();
    if(m.points)
    {
      geom.topology = halp::primitive_topology::points;
      geom.cull_mode = halp::cull_mode::none;
      geom.front_face = halp::front_face::counter_clockwise;
    }
    else
    {
      geom.topology = halp::primitive_topology::triangles;
      geom.cull_mode = halp::cull_mode::back;
      geom.front_face = halp::front_face::counter_clockwise;
    }
    geom.index = {};

    geom.vertices = m.vertices;

    geom.buffers.push_back(
        halp::geometry_cpu_buffer{
            .raw_data = this->complete.data(),
            .byte_size = int64_t(this->complete.size() * sizeof(float)),
            .dirty = true});

    // Bindings
    geom.bindings.push_back(
        halp::geometry_binding{
            .stride = 3 * sizeof(float),
            .step_rate = 1,
            .classification = halp::binding_classification::per_vertex});

    if(m.texcoord)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = 2 * sizeof(float),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});
    }

    if(m.normals)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = 3 * sizeof(float),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});
    }

    if(m.colors)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = int(m.color_components * sizeof(float)),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});
    }

    if(m.tangents)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = 4 * sizeof(float),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});
    }

    // Attributes
    geom.attributes.push_back(
        halp::geometry_attribute{
            .binding = 0,
            .semantic = halp::attribute_semantic::position,
            .format = halp::attribute_format::float3,
            .byte_offset = 0});

    if(m.texcoord)
    {
      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = halp::attribute_semantic::texcoord0,
              .format = halp::attribute_format::float2,
              .byte_offset = 0});
    }

    if(m.normals)
    {
      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = halp::attribute_semantic::normal,
              .format = halp::attribute_format::float3,
              .byte_offset = 0});
    }

    if(m.colors)
    {
      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = halp::attribute_semantic::color0,
              .format = m.color_components == 4 ? halp::attribute_format::float4
                                                : halp::attribute_format::float3,
              .byte_offset = 0});
    }

    if(m.tangents)
    {
      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = halp::attribute_semantic::tangent,
              .format = halp::attribute_format::float4,
              .byte_offset = 0});
    }

    // Vertex input;
    geom.input.push_back(
        halp::geometry_input{
            .buffer = 0, .byte_offset = m.pos_offset * (int)sizeof(float)});

    if(m.texcoord)
    {
      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = m.texcoord_offset * (int)sizeof(float)});
    }

    if(m.normals)
    {
      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = m.normal_offset * (int)sizeof(float)});
    }

    if(m.colors)
    {
      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = m.color_offset * (int)sizeof(float)});
    }

    if(m.tangents)
    {
      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = m.tangent_offset * (int)sizeof(float)});
    }

    for(auto& extra : m.extras)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = extra.components * (int)sizeof(float),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});

      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = extra.semantic,
              .format = extra.format,
              .byte_offset = 0});

      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = extra.offset * (int)sizeof(float)});
    }

    outputs.geometry.mesh.push_back(std::move(geom));
    outputs.geometry.dirty_mesh = true;
  }
}

static bool check_file_extension(std::string_view filename, std::string_view expected)
{
  if(filename.size() < expected.size())
    return false;
  auto ext = filename.substr(filename.size() - expected.size(), expected.size());
  for(std::size_t i = 0; i < expected.size(); i++)
    if(std::tolower(ext[i]) != std::tolower(expected[i]))
      return false;
  return true;
}

std::function<void(GeometryLoader&)> GeometryLoader::ins::geom_t::process(file_type tv)
{
  // Dispatch by extension. Each branch returns a pair of
  // (vector<Threedim::mesh>, float_vec). Empty pair = unsupported / failed
  // parse → we return {} so the halp runtime leaves the current geometry
  // intact rather than wiping it.
  //
  // The returned lambda (captured mesh list + flat float buffer) runs on
  // the execution thread and swaps into the loader instance's members,
  // then triggers rebuild_geometry to populate the dynamic_geometry
  // output.
  // Derive flat per-face normals for any triangle mesh a loader returned
  // without them. A mesh with no normals renders BLACK under any lit
  // material (the Light projection has nothing to dot against): STL got
  // its per-face normals in 1a02c5cabf, but the TinyObj path leaves an OBJ
  // that carries no `vn` records normal-less. Deriving them here — at the
  // loader that feeds the renderer, not in ObjFromString whose documented
  // contract is to report normals as absent — keeps a loaded mesh visible.
  auto deriveMissingNormals = [](std::vector<mesh>& meshes,
                                 Threedim::float_vec& buf) {
    for(auto& m : meshes)
    {
      if(m.normals || m.points || m.vertices <= 0)
        continue;

      const int64_t nrm_offset = int64_t(buf.size());
      buf.resize(buf.size() + m.vertices * 3, 0.f);
      const float* pos = buf.data() + m.pos_offset;
      float* nrm = buf.data() + nrm_offset;
      for(int64_t t = 0; t < m.vertices / 3; ++t)
      {
        const float* p0 = pos + 9 * t;
        const float* p1 = p0 + 3;
        const float* p2 = p0 + 6;
        const float ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2];
        const float vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2];
        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if(len > 0.f)
        {
          nx /= len;
          ny /= len;
          nz /= len;
        }
        for(int c = 0; c < 3; ++c)
        {
          *nrm++ = nx;
          *nrm++ = ny;
          *nrm++ = nz;
        }
      }
      m.normal_offset = nrm_offset;
      m.normals = true;
    }
  };

  auto upload = [&](auto&& mesh, auto&& buf) {
    deriveMissingNormals(mesh, buf);
    return [mesh = std::move(mesh), buf = std::move(buf)](GeometryLoader& o) mutable {
      std::swap(o.meshinfo, mesh);
      std::swap(o.complete, buf);
      o.rebuild_geometry();
    };
  };

  Threedim::float_vec buf;
  if(check_file_extension(tv.filename, "obj"))
  {
    if(auto mesh = Threedim::ObjFromString(tv.bytes, buf); !mesh.empty())
      return upload(std::move(mesh), std::move(buf));
  }
  else if(check_file_extension(tv.filename, "ply"))
  {
    if(auto mesh = Threedim::PlyFromFile(tv.filename, buf); !mesh.empty())
      return upload(std::move(mesh), std::move(buf));
  }
  else if(check_file_extension(tv.filename, "stl"))
  {
    if(auto mesh = Threedim::StlFromFile(tv.filename, buf); !mesh.empty())
      return upload(std::move(mesh), std::move(buf));
  }
  else if(check_file_extension(tv.filename, "off"))
  {
    if(auto mesh = Threedim::OffFromFile(tv.filename, buf); !mesh.empty())
      return upload(std::move(mesh), std::move(buf));
  }
  return {};
}

void GeometryLoader::operator()()
{
  // Compute TRS matrix from position/rotation/scale into
  // halp::mesh::transform[16]. dirty_transform fires only on actual
  // change so downstream's transform binding rebuild is skipped on
  // idle frames.
  outputs.geometry.dirty_transform
      = computeTRSMatrix(inputs, outputs.geometry.transform, m_cachedTRS);
}

}
