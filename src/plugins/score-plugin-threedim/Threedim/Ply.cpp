#include "Ply.hpp"

#include <miniply.h>

#include <ossia/dataflow/geometry_port.hpp>

#include <cmath>
namespace Threedim
{

struct TriMesh
{
  float_vec storage;
  float* pos = nullptr;
  float* uv = nullptr;
  float* norm = nullptr;
  float* color = nullptr;
  uint32_t numVerts = 0;
};

static bool print_ply_header(const char* filename)
{
  static const char* kPropertyTypes[] = {
      "char",
      "uchar",
      "short",
      "ushort",
      "int",
      "uint",
      "float",
      "double",
  };

  miniply::PLYReader reader(filename);
  if (!reader.valid())
  {
    return false;
  }
  using namespace miniply;
  for (uint32_t i = 0, endI = reader.num_elements(); i < endI; i++)
  {
    const miniply::PLYElement* elem = reader.get_element(i);
    fprintf(stderr, "element %s %u\n", elem->name.c_str(), elem->count);
    for (const miniply::PLYProperty& prop : elem->properties)
    {
      if (prop.countType != miniply::PLYPropertyType::None)
      {
        fprintf(
            stderr,
            "property list %s %s %s\n",
            kPropertyTypes[uint32_t(prop.countType)],
            kPropertyTypes[uint32_t(prop.type)],
            prop.name.c_str());
      }
      else
      {
        fprintf(
            stderr,
            "property %s %s\n",
            kPropertyTypes[uint32_t(prop.type)],
            prop.name.c_str());
      }
    }
  }
  fprintf(stderr, "end_header\n");

  return true;
}

// Known PLY multi-component property groups that map to a single semantic.
// These are tried before individual scalar property matching.
struct PlyPropertyGroup
{
  const char* names[4];
  int count;
  halp::attribute_semantic semantic;
  halp::attribute_format format;
};

static constexpr PlyPropertyGroup known_groups[] = {
    // Normals (alternate naming from PCL/PDAL — miniply's find_normal only checks nx/ny/nz)
    {{"normal_x", "normal_y", "normal_z"}, 3,
     halp::attribute_semantic::normal, halp::attribute_format::float3},

    // Velocity (Houdini, simulation exports)
    {{"velocity_x", "velocity_y", "velocity_z"}, 3,
     halp::attribute_semantic::velocity, halp::attribute_format::float3},
    {{"vx", "vy", "vz"}, 3,
     halp::attribute_semantic::velocity, halp::attribute_format::float3},

    // Scale (3DGS, instancing)
    {{"scale_x", "scale_y", "scale_z"}, 3,
     halp::attribute_semantic::scale, halp::attribute_format::float3},

    // Rotation quaternion
    {{"rot_x", "rot_y", "rot_z", "rot_w"}, 4,
     halp::attribute_semantic::rotation, halp::attribute_format::float4},

    // Angular velocity
    {{"angular_velocity_x", "angular_velocity_y", "angular_velocity_z"}, 3,
     halp::attribute_semantic::angular_velocity, halp::attribute_format::float3},

    // Force
    {{"force_x", "force_y", "force_z"}, 3,
     halp::attribute_semantic::force, halp::attribute_format::float3},

    // Acceleration
    {{"acceleration_x", "acceleration_y", "acceleration_z"}, 3,
     halp::attribute_semantic::acceleration, halp::attribute_format::float3},

    // Previous position (Verlet)
    {{"previous_position_x", "previous_position_y", "previous_position_z"}, 3,
     halp::attribute_semantic::previous_position, halp::attribute_format::float3},

    // Previous velocity
    {{"previous_velocity_x", "previous_velocity_y", "previous_velocity_z"}, 3,
     halp::attribute_semantic::previous_velocity, halp::attribute_format::float3},

    // Target position
    {{"target_position_x", "target_position_y", "target_position_z"}, 3,
     halp::attribute_semantic::target_position, halp::attribute_format::float3},

    // Rest position
    {{"rest_position_x", "rest_position_y", "rest_position_z"}, 3,
     halp::attribute_semantic::rest_position, halp::attribute_format::float3},

    // Collision normal
    {{"collision_normal_x", "collision_normal_y", "collision_normal_z"}, 3,
     halp::attribute_semantic::collision_normal, halp::attribute_format::float3},

    // Tangent
    {{"tangent_x", "tangent_y", "tangent_z"}, 3,
     halp::attribute_semantic::tangent, halp::attribute_format::float3},
    {{"tx", "ty", "tz"}, 3,
     halp::attribute_semantic::tangent, halp::attribute_format::float3},

    // Bitangent
    {{"bitangent_x", "bitangent_y", "bitangent_z"}, 3,
     halp::attribute_semantic::bitangent, halp::attribute_format::float3},

    // Emissive color
    {{"emissive_r", "emissive_g", "emissive_b"}, 3,
     halp::attribute_semantic::emissive, halp::attribute_format::float3},

    // Anisotropy direction
    {{"anisotropy_direction_x", "anisotropy_direction_y", "anisotropy_direction_z"}, 3,
     halp::attribute_semantic::anisotropy_direction, halp::attribute_format::float3},

    // Sprite size
    {{"sprite_width", "sprite_height"}, 2,
     halp::attribute_semantic::sprite_size, halp::attribute_format::float2},
};

// PLY property names that don't match the semantic table directly
// but should map to known semantics. These are checked in the scalar
// property scan phase (Phase 2) before falling through to name_to_semantic().
struct PlyNameAlias {
  const char* ply_name;
  halp::attribute_semantic semantic;
};

static constexpr PlyNameAlias ply_aliases[] = {
    // MeshLab/VCG: "radius" means point radius (maps to width semantic)
    {"radius", halp::attribute_semantic::width},
    // MeshLab/VCG: "quality" is a general scalar (maps to fx0)
    {"quality", halp::attribute_semantic::fx0},
    // MeshLab/VCG: "confidence" (maps to fx1)
    {"confidence", halp::attribute_semantic::fx1},
    // PCL/LiDAR: "intensity" (maps to fx2)
    {"intensity", halp::attribute_semantic::fx2},
    // PCL: "curvature" (maps to fx3)
    {"curvature", halp::attribute_semantic::fx3},
    // PLY spec / COLMAP: standalone alpha channel
    {"alpha", halp::attribute_semantic::opacity},
    {"diffuse_alpha", halp::attribute_semantic::opacity},
    // CloudCompare scalar_ prefix variants
    {"scalar_intensity", halp::attribute_semantic::fx2},
};

static halp::attribute_format format_for_components(int n)
{
  switch(n)
  {
    case 1: return halp::attribute_format::float1;
    case 2: return halp::attribute_format::float2;
    case 3: return halp::attribute_format::float3;
    default: return halp::attribute_format::float4;
  }
}

static bool
load_vert_from_ply(miniply::PLYReader& reader, TriMesh* trimesh, float_vec& buf,
                   std::vector<extra_attribute>& extras)
{
  uint32_t pos_indices[3];
  uint32_t uv_indices[3];
  uint32_t n_indices[3];
  uint32_t col_indices[3];

  if (reader.element_is(miniply::kPLYVertexElement) && reader.load_element())
  {
    const auto N = reader.num_rows();
    if (N <= 0)
      return false;

    trimesh->numVerts = N;

    bool pos = reader.find_pos(pos_indices);
    bool uv = reader.find_texcoord(uv_indices);
    bool norms = reader.find_normal(n_indices);
    bool col = reader.find_color(col_indices);
    if (!col)
    {
      col = reader.find_properties(
          col_indices, 3, "diffuse_red", "diffuse_green", "diffuse_blue");
    }

    if (!pos)
      return false;

    // Track which property indices are already consumed by standard extraction
    const auto* elem = reader.element();
    std::vector<bool> consumed(elem->properties.size(), false);

    auto mark_consumed = [&](const uint32_t* indices, int count) {
      for(int i = 0; i < count; i++)
        if(indices[i] < consumed.size())
          consumed[indices[i]] = true;
    };

    if(pos) mark_consumed(pos_indices, 3);
    if(uv) mark_consumed(uv_indices, 2);
    if(norms) mark_consumed(n_indices, 3);
    if(col) mark_consumed(col_indices, 3);

    // Phase 1: Try known multi-component property groups
    struct PendingExtra {
      uint32_t indices[4];
      int count;
      halp::attribute_semantic semantic;
      halp::attribute_format format;
    };
    std::vector<PendingExtra> pending;

    for(const auto& group : known_groups)
    {
      uint32_t indices[4];
      bool all_found = true;
      bool any_consumed = false;

      for(int i = 0; i < group.count; i++)
      {
        indices[i] = elem->find_property(group.names[i]);
        if(indices[i] == miniply::kInvalidIndex)
        {
          all_found = false;
          break;
        }
        if(consumed[indices[i]])
          any_consumed = true;
      }

      if(all_found && !any_consumed)
      {
        for(int i = 0; i < group.count; i++)
          consumed[indices[i]] = true;

        PendingExtra pe;
        std::copy_n(indices, group.count, pe.indices);
        pe.count = group.count;
        pe.semantic = group.semantic;
        pe.format = group.format;
        pending.push_back(pe);
      }
    }

    // Phase 2: Scan remaining scalar properties for known semantic names.
    // First check PLY-specific aliases (radius→width, quality→fx0, etc.),
    // then try the general name_to_semantic() lookup.
    for(uint32_t pi = 0; pi < elem->properties.size(); pi++)
    {
      if(consumed[pi])
        continue;

      const auto& prop = elem->properties[pi];

      // Skip list properties
      if(prop.countType != miniply::PLYPropertyType::None)
        continue;

      // Check PLY-specific name aliases first
      halp::attribute_semantic matched_sem = halp::attribute_semantic::custom;
      for(const auto& alias : ply_aliases)
      {
        if(prop.name == alias.ply_name)
        {
          matched_sem = alias.semantic;
          break;
        }
      }

      // Fall back to the general semantic name table
      if(matched_sem == halp::attribute_semantic::custom)
      {
        auto sem = ossia::name_to_semantic(prop.name);
        if(sem != ossia::attribute_semantic::custom)
          matched_sem = static_cast<halp::attribute_semantic>(sem);
      }

      if(matched_sem != halp::attribute_semantic::custom)
      {
        consumed[pi] = true;

        PendingExtra pe;
        pe.indices[0] = pi;
        pe.count = 1;
        pe.semantic = matched_sem;
        pe.format = halp::attribute_format::float1;
        pending.push_back(pe);
      }
    }

    // Calculate total buffer size
    int num_elements = 0;
    if (pos) num_elements += 3;
    if (uv)  num_elements += 2;
    if (norms) num_elements += 3;
    if (col) num_elements += 3;
    for(auto& pe : pending)
      num_elements += pe.count;

    buf.resize(num_elements * trimesh->numVerts, boost::container::default_init);

    // Extract standard attributes
    float* cur = buf.data();
    if (pos)
    {
      trimesh->pos = cur;
      reader.extract_properties(
          pos_indices, 3, miniply::PLYPropertyType::Float, trimesh->pos);
      cur += 3 * N;
    }

    if (uv)
    {
      trimesh->uv = cur;
      reader.extract_properties(
          uv_indices, 2, miniply::PLYPropertyType::Float, trimesh->uv);
      cur += 2 * N;
    }

    if (norms)
    {
      trimesh->norm = cur;
      reader.extract_properties(
          n_indices, 3, miniply::PLYPropertyType::Float, trimesh->norm);
      cur += 3 * N;
    }

    if (col)
    {
      const auto t = elem->properties[col_indices[0]].type;
      trimesh->color = cur;
      reader.extract_properties(
          col_indices, 3, miniply::PLYPropertyType::Float, trimesh->color);
      switch (t)
      {
        case miniply::PLYPropertyType::Float:
          break;
        case miniply::PLYPropertyType::Char:
          break;
        case miniply::PLYPropertyType::UChar:
        {
          for (float *begin = trimesh->color, *end = trimesh->color + 3 * N;
               begin != end;
               ++begin)
            *begin /= 255.f;
          break;
        }
        default:
          break;
      }
      cur += 3 * N;
    }

    // Extract extra attributes
    float* begin = buf.data();
    for(auto& pe : pending)
    {
      extra_attribute ea;
      ea.offset = cur - begin;
      ea.semantic = pe.semantic;
      ea.format = pe.format;
      ea.components = pe.count;

      // Normalize UChar properties (colors stored as 0-255) to 0-1
      const auto t = elem->properties[pe.indices[0]].type;

      reader.extract_properties(
          pe.indices, pe.count, miniply::PLYPropertyType::Float, cur);

      if(t == miniply::PLYPropertyType::UChar)
      {
        for(float *p = cur, *end = cur + pe.count * N; p != end; ++p)
          *p /= 255.f;
      }

      cur += pe.count * N;
      extras.push_back(ea);
    }

    return true;
  }
  return false;
}

static TriMesh load_vertices_from_ply(miniply::PLYReader& reader, float_vec& buf,
                                      std::vector<extra_attribute>& extras)
{
  TriMesh mesh;

  while (reader.has_element())
  {
    if (load_vert_from_ply(reader, &mesh, buf, extras))
      return mesh;
    reader.next_element();
  }

  return {};
}

std::vector<mesh> PlyFromFile(std::string_view filename, float_vec& buf)
{
  print_ply_header(filename.data());

  std::vector<mesh> meshes;
  miniply::PLYReader reader{filename.data()};
  if (!reader.valid())
    return {};

  std::vector<extra_attribute> extras;
  auto res = load_vertices_from_ply(reader, buf, extras);
  if (!res.pos)
    return {};

  auto begin = buf.data();
  mesh m{};
  m.vertices = res.numVerts;
  m.points = true;
  m.pos_offset = 0;
  if (res.uv)
  {
    m.texcoord_offset = res.uv - begin;
    m.texcoord = true;
  }
  if (res.norm)
  {
    m.normal_offset = res.norm - begin;
    m.normals = true;
  }
  if (res.color)
  {
    m.color_offset = res.color - begin;
    m.colors = true;
  }
  m.extras = std::move(extras);

  meshes.push_back(std::move(m));
  return meshes;
}

GaussianSplatData GaussianSplatsFromPly(std::string_view filename)
{
  using miniply::PLYPropertyType;

  GaussianSplatData result;

  miniply::PLYReader reader(filename.data());
  if(!reader.valid())
    return result;

  while(reader.has_element())
  {
    if(!reader.element_is(miniply::kPLYVertexElement))
    {
      reader.next_element();
      continue;
    }

    if(!reader.load_element())
      return result;

    const uint32_t N = reader.num_rows();
    if(N == 0)
      return result;

    result.splatCount = N;
    qDebug() << "Loader: " << N << "splat";
    result.buffer.resize(
        std::size_t(N) * GaussianSplatData::floatsPerSplat,
        boost::container::default_init);
    // Zero-fill: ensures padding and missing properties are 0
    std::memset(result.buffer.data(), 0, result.buffer.size() * sizeof(float));

    char* base = reinterpret_cast<char*>(result.buffer.data());
    constexpr uint32_t stride = GaussianSplatData::bytesPerSplat; // 256 bytes

    // Position: floats 0-2, byte offset 0
    {
      uint32_t idx[3];
      if(reader.find_properties(idx, 3, "x", "y", "z"))
        reader.extract_properties_with_stride(
            idx, 3, PLYPropertyType::Float, base, stride);
    }

    // Normals: floats 3-5, byte offset 12
    {
      uint32_t idx[3];
      if(reader.find_properties(idx, 3, "nx", "ny", "nz"))
        reader.extract_properties_with_stride(
            idx, 3, PLYPropertyType::Float, base + 12, stride);
    }

    // SH DC coefficients: floats 6-8, byte offset 24
    {
      uint32_t idx[3];
      if(reader.find_properties(idx, 3, "f_dc_0", "f_dc_1", "f_dc_2"))
        reader.extract_properties_with_stride(
            idx, 3, PLYPropertyType::Float, base + 24, stride);
    }

    // SH rest coefficients: floats 9-53, byte offset 36
    // Find how many f_rest_N properties exist (0, 9, 24, or 45 typically)
    {
      uint32_t idx[45];
      uint32_t numRest = 0;
      for(uint32_t i = 0; i < 45; ++i)
      {
        char name[16];
        std::snprintf(name, sizeof(name), "f_rest_%u", i);
        uint32_t propIdx = reader.find_property(name);
        if(propIdx == miniply::kInvalidIndex)
          break;
        idx[i] = propIdx;
        numRest++;
      }
      result.shRestCount = numRest;
      if(numRest > 0)
        reader.extract_properties_with_stride(
            idx, numRest, PLYPropertyType::Float, base + 36, stride);
    }

    // Opacity: float 54, byte offset 216
    {
      uint32_t idx[1];
      if(reader.find_properties(idx, 1, "opacity"))
        reader.extract_properties_with_stride(
            idx, 1, PLYPropertyType::Float, base + 216, stride);
    }

    // Scale (log-space): floats 55-57, byte offset 220
    {
      uint32_t idx[3];
      if(reader.find_properties(idx, 3, "scale_0", "scale_1", "scale_2"))
        reader.extract_properties_with_stride(
            idx, 3, PLYPropertyType::Float, base + 220, stride);
    }

    // Rotation quaternion (w,x,y,z): floats 58-61, byte offset 232
    {
      uint32_t idx[4];
      if(reader.find_properties(idx, 4, "rot_0", "rot_1", "rot_2", "rot_3"))
        reader.extract_properties_with_stride(
            idx, 4, PLYPropertyType::Float, base + 232, stride);
    }

    // Debug: log a few sample splats to verify loaded data
    {
      const float* data = result.buffer.data();
      constexpr uint32_t FPS = GaussianSplatData::floatsPerSplat; // 64

      // Log indices: first 3, middle, last
      uint32_t samplesToLog[] = {0, 1, 2, N / 2, N - 1};
      int numSamples = (N >= 5) ? 5 : N;

      for(int s = 0; s < numSamples; ++s)
      {
        uint32_t i = samplesToLog[s];
        if(i >= N) continue;
        const float* splat = data + std::size_t(i) * FPS;

        qDebug() << "=== Splat" << i << "/" << N << "===";
        qDebug() << "  pos:     " << splat[0] << splat[1] << splat[2];
        qDebug() << "  normal:  " << splat[3] << splat[4] << splat[5];
        qDebug() << "  SH DC:   " << splat[6] << splat[7] << splat[8];

        // Print all SH rest coefficients (floats 9..53)
        {
          QString shLine;
          for(uint32_t c = 0; c < result.shRestCount; ++c)
            shLine += QString::number(splat[9 + c], 'g', 6) + " ";
          qDebug() << "  SH rest (" << result.shRestCount << "):" << shLine;
        }

        qDebug() << "  opacity: " << splat[54]
                 << " -> sigmoid:" << (1.0f / (1.0f + std::exp(-splat[54])));
        qDebug() << "  scale:   " << splat[55] << splat[56] << splat[57]
                 << " -> exp:" << std::exp(splat[55]) << std::exp(splat[56]) << std::exp(splat[57]);
        qDebug() << "  rot(wxyz):" << splat[58] << splat[59] << splat[60] << splat[61];
        float qlen = std::sqrt(splat[58]*splat[58] + splat[59]*splat[59]
                              + splat[60]*splat[60] + splat[61]*splat[61]);
        qDebug() << "  rot norm:" << qlen;

        // Print all 64 floats as raw dump
        {
          QString rawLine;
          for(int f = 0; f < 64; ++f)
          {
            if(f > 0 && f % 8 == 0) rawLine += "\n                ";
            rawLine += QString("[%1]=%2 ").arg(f).arg(splat[f], 0, 'g', 6);
          }
          qDebug().noquote() << "  raw:" << rawLine;
        }
      }

      // Verify: are the found f_rest property indices correct?
      {
        const auto* elem = reader.element();
        qDebug() << "=== PLY property layout (element has" << elem->properties.size()
                 << "properties, rowStride=" << elem->rowStride << ") ===";
        for(uint32_t p = 0; p < elem->properties.size(); ++p)
        {
          const auto& prop = elem->properties[p];
          qDebug() << "  [" << p << "] name=" << prop.name.c_str()
                   << " type=" << int(prop.type)
                   << " offset=" << prop.offset;
        }
      }

      // Summary stats: check for NaN/Inf and value ranges
      uint32_t nanCount = 0, infCount = 0, zeroPos = 0;
      float minOpacity = 1e30f, maxOpacity = -1e30f;
      float minScale = 1e30f, maxScale = -1e30f;
      for(uint32_t i = 0; i < N; ++i)
      {
        const float* splat = data + std::size_t(i) * FPS;
        for(int f = 0; f < 62; ++f)
        {
          if(std::isnan(splat[f])) nanCount++;
          if(std::isinf(splat[f])) infCount++;
        }
        if(splat[0] == 0.f && splat[1] == 0.f && splat[2] == 0.f)
          zeroPos++;
        if(splat[54] < minOpacity) minOpacity = splat[54];
        if(splat[54] > maxOpacity) maxOpacity = splat[54];
        for(int j = 55; j <= 57; ++j)
        {
          if(splat[j] < minScale) minScale = splat[j];
          if(splat[j] > maxScale) maxScale = splat[j];
        }
      }
      qDebug() << "=== Summary ===" ;
      qDebug() << "  Total splats:" << N;
      qDebug() << "  NaN values:" << nanCount << " Inf values:" << infCount;
      qDebug() << "  Zero-position splats:" << zeroPos;
      qDebug() << "  Opacity range (raw):" << minOpacity << "to" << maxOpacity;
      qDebug() << "  Scale range (log):" << minScale << "to" << maxScale;
      qDebug() << "  SH rest coeffs found:" << result.shRestCount;
    }

    return result;
  }

  return result;
}

}
