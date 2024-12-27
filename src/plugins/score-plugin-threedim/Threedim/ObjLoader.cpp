#include "ObjLoader.hpp"

#include "Ply.hpp"

#include <QMatrix4x4>
#include <QString>

namespace Threedim
{

void ObjLoader::operator()() { }

void ObjLoader::rebuild_geometry()
{
  std::vector<mesh>& new_meshes = this->meshinfo;

  if (!outputs.geometry.mesh.empty())
  {
    outputs.geometry.mesh.clear();
  }

  for (auto& m : new_meshes)
  {
    if (m.vertices <= 0)
      continue;

    halp::dynamic_geometry geom;

    geom.buffers.clear();
    geom.bindings.clear();
    geom.attributes.clear();
    geom.input.clear();
    if (m.points)
    {
      geom.topology = halp::dynamic_geometry::points;
      geom.cull_mode = halp::dynamic_geometry::none;
      geom.front_face = halp::dynamic_geometry::counter_clockwise;
    }
    else
    {
      geom.topology = halp::dynamic_geometry::triangles;
      geom.cull_mode = halp::dynamic_geometry::back;
      geom.front_face = halp::dynamic_geometry::counter_clockwise;
    }
    geom.index = {};

    geom.vertices = m.vertices;

    geom.buffers.push_back(halp::dynamic_geometry::buffer{
        .data = this->complete.data(),
        .size = int64_t(this->complete.size() * sizeof(float)),
        .dirty = true});

    // Bindings
    geom.bindings.push_back(halp::dynamic_geometry::binding{
        .stride = 3 * sizeof(float),
        .step_rate = 1,
        .classification = halp::dynamic_geometry::binding::per_vertex});

    if (m.texcoord)
    {
      geom.bindings.push_back(halp::dynamic_geometry::binding{
          .stride = 2 * sizeof(float),
          .step_rate = 1,
          .classification = halp::dynamic_geometry::binding::per_vertex});
    }

    if (m.normals)
    {
      geom.bindings.push_back(halp::dynamic_geometry::binding{
          .stride = 3 * sizeof(float),
          .step_rate = 1,
          .classification = halp::dynamic_geometry::binding::per_vertex});
    }

    if (m.colors)
    {
      geom.bindings.push_back(halp::dynamic_geometry::binding{
          .stride = 3 * sizeof(float),
          .step_rate = 1,
          .classification = halp::dynamic_geometry::binding::per_vertex});
    }

    // Attributes
    geom.attributes.push_back(halp::dynamic_geometry::attribute{
        .binding = 0,
        .location = halp::dynamic_geometry::attribute::position,
        .format = halp::dynamic_geometry::attribute::float3,
        .offset = 0});

    if (m.texcoord)
    {
      geom.attributes.push_back(halp::dynamic_geometry::attribute{
          .binding = geom.attributes.back().binding + 1,
          .location = halp::dynamic_geometry::attribute::tex_coord,
          .format = halp::dynamic_geometry::attribute::float2,
          .offset = 0});
    }

    if (m.normals)
    {
      geom.attributes.push_back(halp::dynamic_geometry::attribute{
          .binding = geom.attributes.back().binding + 1,
          .location = halp::dynamic_geometry::attribute::normal,
          .format = halp::dynamic_geometry::attribute::float3,
          .offset = 0});
    }

    if (m.colors)
    {
      geom.attributes.push_back(halp::dynamic_geometry::attribute{
          .binding = geom.attributes.back().binding + 1,
          .location = halp::dynamic_geometry::attribute::color,
          .format = halp::dynamic_geometry::attribute::float3,
          .offset = 0});
    }

    // Vertex input
    using input_t = struct halp::dynamic_geometry::input;
    geom.input.push_back(
        input_t{.buffer = 0, .offset = m.pos_offset * (int)sizeof(float)});

    if (m.texcoord)
    {
      geom.input.push_back(
          input_t{.buffer = 0, .offset = m.texcoord_offset * (int)sizeof(float)});
    }

    if (m.normals)
    {
      geom.input.push_back(
          input_t{.buffer = 0, .offset = m.normal_offset * (int)sizeof(float)});
    }

    if (m.colors)
    {
      geom.input.push_back(
          input_t{.buffer = 0, .offset = m.color_offset * (int)sizeof(float)});
    }
    outputs.geometry.mesh.push_back(std::move(geom));
    outputs.geometry.dirty_mesh = true;
  }
}

static bool check_file_extension(std::string_view filename, std::string_view expected)
{
  if (filename.size() < expected.size())
    return false;
  auto ext = filename.substr(filename.size() - expected.size(), expected.size());
  for (int i = 0; i < expected.size(); i++)
    if (std::tolower(ext[i]) != std::tolower(expected[i]))
      return false;
  return true;
}

std::function<void(ObjLoader&)> ObjLoader::ins::obj_t::process(file_type tv)
{
  auto upload = [](auto&& mesh, auto&& buf)
  {
    return [mesh = std::move(mesh), buf = std::move(buf)](ObjLoader& o) mutable
    {
      // This part happens in the execution thread
      std::swap(o.meshinfo, mesh);
      std::swap(o.complete, buf);

      o.rebuild_geometry();
    };
  };

  Threedim::float_vec buf;
  if (check_file_extension(tv.filename, "obj"))
  {
    // This part happens in a separate thread
    if (auto mesh = Threedim::ObjFromString(tv.bytes, buf); !mesh.empty())
    {
      return upload(std::move(mesh), std::move(buf));
    }
  }
  else if (check_file_extension(tv.filename, "ply"))
  {
    // This part happens in a separate thread
    if (auto mesh = Threedim::PlyFromFile(tv.filename, buf); !mesh.empty())
    {
      return upload(std::move(mesh), std::move(buf));
    }
  }
  return {};
}
}
