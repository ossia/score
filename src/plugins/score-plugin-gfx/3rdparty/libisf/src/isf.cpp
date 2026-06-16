#include "isf.hpp"

#include "sajson.h"

#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/string_map.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <ctre.hpp>
#include <fmt/format.h>
#include <array>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>
extern "C" {
#include <glsl-parser/glsl_ast.h>
#include <glsl-parser/glsl_parser.h>
}
#include <glsl-parser/glsl_tokens.hpp>

namespace isf
{
namespace
{
static constexpr struct glsl45_t
{
  static constexpr auto versionPrelude = R"_(#version 450
)_";

  static constexpr auto vertexPrelude = R"_(
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 isf_FragNormCoord;

)_";
  static constexpr auto vertexInitFunc = R"_(
void isf_vertShaderInit()
{
  gl_Position = clipSpaceCorrMatrix * vec4(position, 0.0, 1.0);
  isf_FragNormCoord = vec2((gl_Position.x+1.0)/2.0, (gl_Position.y+1.0)/2.0);
}

void isf_vertShaderFinish()
{
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = -gl_Position.y;
#endif
}
)_";

  static constexpr auto vertexDefaultMain = R"_(
void main()
{
  isf_vertShaderInit();
  isf_vertShaderFinish();
}
)_";

  static constexpr auto fragmentPrelude = R"_(
layout(location = 0) in vec2 isf_FragNormCoord;
layout(location = 0) out vec4 isf_FragColor;
)_";

  static constexpr auto defaultUniforms = R"_(
// Shared uniform buffer for the whole render window
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix_;

  vec2 RENDERSIZE_;
  // MSAA sample count of the active output target (1 when MSAA is off).
  // Mirrors RenderList::samples(); needed because glslang strips
  // gl_NumSamples under SPIR-V. _pad0 keeps the struct vec4-aligned.
  int MSAA_SAMPLES_;
  int _renderer_pad0_;
} isf_renderer_uniforms;

// This dance is needed because otherwise
// spirv-cross may generate different struct names in the vertex & fragment, causing crashes..
// but we have to keep compat with ISF
#define clipSpaceCorrMatrix isf_renderer_uniforms.clipSpaceCorrMatrix_
#define MSAA_SAMPLES isf_renderer_uniforms.MSAA_SAMPLES_

// Time-dependent uniforms, only relevant during execution
layout(std140, binding = 1) uniform process_t {
  float TIME_;
  float TIMEDELTA_;
  float PROGRESS_;
  float SAMPLERATE_;

  int PASSINDEX_;
  int FRAMEINDEX_;

  vec2 RENDERSIZE_;
  vec4 DATE_;
  // Mirrors gl_NumWorkGroups for compute shaders. SPIRV-Cross's HLSL
  // backend refuses to emit code for the NumWorkgroups built-in unless
  // remap_num_workgroups_builtin() is set up on both the cross-compiler
  // and the QRhi side; QShaderBaker exposes neither, so any compute
  // shader using gl_NumWorkGroups silently fails to bake to HLSL on
  // D3D11/D3D12. We sidestep that by routing references through this
  // uniform — populated host-side just before each dispatch — and
  // textually shadowing the built-in via the #define below.
  uvec3 NUMWORKGROUPS_;
} isf_process_uniforms;

#define TIME isf_process_uniforms.TIME_
#define TIMEDELTA isf_process_uniforms.TIMEDELTA_
#define PROGRESS isf_process_uniforms.PROGRESS_
#define PASSINDEX isf_process_uniforms.PASSINDEX_
#define FRAMEINDEX isf_process_uniforms.FRAMEINDEX_
#define RENDERSIZE isf_process_uniforms.RENDERSIZE_
#define DATE isf_process_uniforms.DATE_
#define SAMPLERATE isf_process_uniforms.SAMPLERATE_
#define gl_NumWorkGroups isf_process_uniforms.NUMWORKGROUPS_
#define isf_NumWorkGroups isf_process_uniforms.NUMWORKGROUPS_
)_";

  static constexpr auto defaultFunctions =
      R"_(
// GLSL's textureSize is overloaded by sampler dimensionality — sampler2D
// returns ivec2, sampler3D returns ivec3. Authors typically reach for
// TEX_DIMENSIONS regardless of 2D/3D; the *_2D / *_3D aliases below make
// the intended dimensionality explicit in shader source.
#define TEX_DIMENSIONS(tex) textureSize(tex, 0)
#define TEX_DIMENSIONS_2D(tex) textureSize(tex, 0)
#define TEX_DIMENSIONS_3D(tex) textureSize(tex, 0)
#define IMG_SIZE(tex) textureSize(tex, 0)
#define IMG_SIZE_3D(tex) textureSize(tex, 0)

// IMG_CUBE(tex, dir) — canonical colour-cube read; same in both coord systems
// since a direction vector has no Y-flip. IMG_CUBE_DEPTH(tex, dir) —
// canonical depth-cube read for inputs declared DEPTH: true on a cubemap,
// hides the internal `_depth` companion binding.
#define IMG_CUBE(tex, dir) texture(tex, dir)
#define IMG_CUBE_DEPTH(tex, dir) texture(tex##_depth, dir).r

#if defined(QSHADER_SPIRV)
#define isf_FragCoord vec4(gl_FragCoord.x, RENDERSIZE.y - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w)
#define ISF_FIXUP_TEXCOORD(coord) vec2((coord).x, 1. - (coord).y)
#define IMG_THIS_PIXEL(tex) texture(tex, ISF_FIXUP_TEXCOORD(isf_FragNormCoord))
#define IMG_THIS_NORM_PIXEL(tex) texture(tex, ISF_FIXUP_TEXCOORD(isf_FragNormCoord))
#define IMG_PIXEL(tex, coord) texture(tex, ISF_FIXUP_TEXCOORD(coord / RENDERSIZE))
#define IMG_NORM_PIXEL(tex, coord) texture(tex, ISF_FIXUP_TEXCOORD(coord))
#define IMG_THIS_DEPTH(tex) texture(tex##_depth, ISF_FIXUP_TEXCOORD(isf_FragNormCoord)).r
#define IMG_DEPTH_PIXEL(tex, coord) texture(tex##_depth, ISF_FIXUP_TEXCOORD(coord / RENDERSIZE)).r
#define IMG_DEPTH_NORM_PIXEL(tex, coord) texture(tex##_depth, ISF_FIXUP_TEXCOORD(coord)).r
#else
#define isf_FragCoord gl_FragCoord
#define IMG_THIS_PIXEL(tex) texture(tex, isf_FragNormCoord)
#define IMG_THIS_NORM_PIXEL(tex) texture(tex, isf_FragNormCoord)
#define IMG_PIXEL(tex, coord) texture(tex, (coord) / RENDERSIZE)
#define IMG_NORM_PIXEL(tex, coord) texture(tex, coord)
#define IMG_THIS_DEPTH(tex) texture(tex##_depth, isf_FragNormCoord).r
#define IMG_DEPTH_PIXEL(tex, coord) texture(tex##_depth, (coord) / RENDERSIZE).r
#define IMG_DEPTH_NORM_PIXEL(tex, coord) texture(tex##_depth, coord).r
#endif
)_";

} GLSL45;

static const ossia::hash_map<std::string, attribute_type>& attribute_type_parse{[] {
  static const ossia::hash_map<std::string, attribute_type> i{
      {"float", attribute_type::Float},     {"vec2", attribute_type::Vec2},
      {"vec3", attribute_type::Vec3},       {"vec4", attribute_type::Vec4},
      {"mat2", attribute_type::Mat2},       {"mat2x3", attribute_type::Mat2x3},
      {"mat2x4", attribute_type::Mat2x4},   {"mat3", attribute_type::Mat3},
      {"mat3x2", attribute_type::Mat3x2},   {"mat3x4", attribute_type::Mat3x4},
      {"mat4", attribute_type::Mat4},       {"mat4x2", attribute_type::Mat4x2},
      {"mat4x3", attribute_type::Mat4x3},   {"int", attribute_type::Int},
      {"ivec2", attribute_type::Int2},      {"ivec3", attribute_type::Int3},
      {"ivec4", attribute_type::Int4},      {"uint", attribute_type::Uint},
      {"uvec2", attribute_type::Uint2},     {"uvec3", attribute_type::Uint3},
      {"uvec4", attribute_type::Uint4},     {"bool", attribute_type::Bool},
      {"bvec2", attribute_type::Bool2},     {"bvec3", attribute_type::Bool3},
      {"bvec4", attribute_type::Bool4},     {"double", attribute_type::Double},
      {"dvec2", attribute_type::Double2},   {"dvec3", attribute_type::Double3},
      {"dvec4", attribute_type::Double4},   {"dmat2", attribute_type::DMat2},
      {"dmat2x3", attribute_type::DMat2x3}, {"dmat2x4", attribute_type::DMat2x4},
      {"dmat3", attribute_type::DMat3},     {"dmat3x2", attribute_type::DMat3x2},
      {"dmat3x4", attribute_type::DMat3x4}, {"dmat4", attribute_type::DMat4},
      {"dmat4x2", attribute_type::DMat4x2}, {"dmat4x3", attribute_type::DMat4x3}};
  return i;
}()};
static const std::array<std::string_view, 70>& attribute_type_map{[] {
  static const std::array<std::string_view, 70> i{
      "Unknown", "float", "vec2",    "vec3",    "vec4",    "mat2",   "mat2x3",
      "mat2x4",  "mat3",  "mat3x2",  "mat3x4",  "mat4",    "mat4x2", "mat4x3",
      "int",     "ivec2", "ivec3",   "ivec4",   "uint",    "uvec2",  "uvec3",
      "uvec4",   "bool",  "bvec2",   "bvec3",   "bvec4",   "double", "dvec2",
      "dvec3",   "dvec4", "dmat2",   "dmat2x3", "dmat2x4", "dmat3",  "dmat3x2",
      "dmat3x4", "dmat4", "dmat4x2", "dmat4x3",
      "sampler1D", "sampler2D", "sampler2DMS", "sampler3D",
      "samplerCube", "sampler1DArray", "sampler2DArray", "sampler2DMSArray",
      "sampler3DArray", "samplerCubeArray", "samplerRect", "samplerBuffer",
      "samplerExternalOES", "sampler",
      "image1D", "image2D", "image2DMS", "image3D",
      "imageCube", "image1DArray", "image2DArray", "image2DMSArray",
      "image3DArray", "imageCubeArray", "imageRect", "imageBuffer",
      "Unknown", // Struct
      "float", "vec2", "vec3", "vec4" // Half, Half2, Half3, Half4
  };

  return i;
}()};
}

parser::parser(std::string geom, ShaderType t)
{
  switch(t)
  {
    case ShaderType::GeometryFilter:

      m_source_geometry_filter = std::move(geom);
      parse_geometry_filter();
      break;
    case ShaderType::CSF:
      m_sourceFragment = std::move(geom);
      parse_csf();
      break;
  }
}

parser::parser(std::string vert, std::string frag, int glslVersion, ShaderType t)
    : m_sourceVertex{std::move(vert)}
    , m_sourceFragment{std::move(frag)}
    , m_version{glslVersion}
{
  this->m_desc.default_vertex_shader = vert.empty();

  static const auto is_isf = [](const std::string& str) {
    bool has_isf
        = (str.find("isf") != std::string::npos || str.find("ISF") != std::string::npos
           || str.find("IMG_") != std::string::npos
           || str.find("\"INPUTS\":") != std::string::npos);
    if(!has_isf)
      return false;

    auto start_pos = str.find("/*");
    if(start_pos == std::string::npos)
      return false;

    auto start_json_pos = str.find('{', start_pos);
    if(start_json_pos == std::string::npos || ((start_json_pos - start_pos) > 10))
      return false;
    return true;
  };
  static const auto is_shadertoy_json
      = [](const std::string& str) { return str.starts_with("[{\"ver\":\""); };
  static const auto is_shadertoy = [](const std::string& str) {
    return str.find("void mainImage(") != std::string::npos
           && str.find("\"ISFVSN\"") == std::string::npos;
  };
  static const auto is_glslsandbox = [](const std::string& str) {
    return (str.find("uniform float time;") != std::string::npos
            || str.find("glslsandbox") != std::string::npos)
           && str.find("\"ISFVSN\"") == std::string::npos;
  };
  static const auto is_vsa = [](const std::string& str) {
    // Check for common VSA patterns
    bool has_vertexId = str.find("vertexId") != std::string::npos;
    bool has_v_color = str.find("v_color") != std::string::npos;
    bool no_frag_color = str.find("_FragColor") == std::string::npos;

    return (has_vertexId || has_v_color) && (no_frag_color);
  };
  static const auto is_csf = [](const std::string& str) {
    bool has_dispatch = str.find("\"DISPATCH\"") != std::string::npos;
    bool has_local_size = str.find("\"LOCAL_SIZE\"") != std::string::npos;
    bool has_resources = str.find("\"RESOURCES\"") != std::string::npos;

    return has_dispatch && has_local_size && (has_resources || is_isf(str));
  };
  static const auto is_raw_raster_pipeline = [](const std::string& str) {
    bool res = str.find("\"RAW_RASTER_PIPELINE\"") != std::string::npos;
    return res;
  };

  switch(t)
  {
    case ShaderType::Autodetect: {
      if(is_csf(m_sourceFragment))
        parse_csf();
      else if(is_raw_raster_pipeline(m_sourceFragment))
        parse_raw_raster_pipeline();
      else if(is_shadertoy_json(m_sourceFragment))
        parse_shadertoy_json(m_sourceFragment);
      else if(is_shadertoy(m_sourceFragment))
        parse_shadertoy();
      else if(is_vsa(m_sourceVertex))
        parse_vsa();
      else if(is_isf(m_sourceFragment))
        parse_isf();
      else if(is_glslsandbox(m_sourceFragment))
        parse_glsl_sandbox();
      else
        m_fragment = m_sourceFragment;
      break;
    }

    case ShaderType::ISF: {
      parse_isf();
      break;
    }
    case ShaderType::RawRasterPipeline: {
      parse_raw_raster_pipeline();
      break;
    }
    case ShaderType::CSF: {
      parse_csf();
      break;
    }
    case ShaderType::ShaderToy: {
      if(is_shadertoy_json(m_sourceFragment))
        parse_shadertoy_json(m_sourceFragment);
      else
        parse_shadertoy();
      break;
    }
    case ShaderType::GLSLSandBox: {
      parse_glsl_sandbox();
      break;
    }
    case ShaderType::VertexShaderArt: {
      parse_vsa();
      break;
    }
    default:
      break;
  }
}

descriptor parser::data() const
{
  return m_desc;
}

std::string parser::vertex() const
{
  return m_vertex;
}

std::string parser::fragment() const
{
  return m_fragment;
}

std::string parser::geometry_filter() const
{
  return m_geometry_filter;
}

static bool is_number(sajson::value& v)
{
  auto t = v.get_type();
  return t == sajson::TYPE_INTEGER || t == sajson::TYPE_DOUBLE;
}

static void parse_input_base(input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "NAME")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
      {
        inp.name = val.as_string();
      }
    }
    else if(k == "LABEL")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
      {
        inp.label = val.as_string();
      }
    }
  }
}

template <std::size_t N>
static std::array<double, N> parse_input_impl(sajson::value& v, std::array<double, N>)
{
  if(v.get_type() == sajson::TYPE_ARRAY && v.get_length() >= N)
  {
    std::array<double, N> arr{};
    for(std::size_t i = 0; i < N; i++)
    {
      auto val = v.get_array_element(i);
      if(is_number(val))
        arr[i] = val.get_number_value();
    }
    return arr;
  }
  return {};
}

static double parse_input_impl(sajson::value& v, double)
{
  if(is_number(v))
    return v.get_number_value();
  return 0.;
}
static int64_t parse_input_impl(sajson::value& v, int64_t)
{
  if(is_number(v))
    return v.get_number_value();
  return 0;
}

static bool parse_input_impl(sajson::value& v, bool)
{
  return v.get_type() == sajson::TYPE_TRUE;
}

// Parse sampler-config fields from a JSON input object directly (flat fields,
// no nested "SAMPLER" object). All fields optional; missing = keep default.
static void parse_sampler_config(sampler_config& s, const sajson::value& v)
{
  auto str_field = [&](const char* key, std::string& out) {
    if(auto k = v.find_object_key_insensitive(sajson::literal(key));
       k != v.get_length())
    {
      auto val = v.get_object_value(k);
      if(val.get_type() == sajson::TYPE_STRING)
        out = val.as_string();
    }
  };
  auto float_field = [&](const char* key, std::optional<float>& out) {
    if(auto k = v.find_object_key_insensitive(sajson::literal(key));
       k != v.get_length())
    {
      auto val = v.get_object_value(k);
      if(is_number(val))
        out = (float)val.get_number_value();
    }
  };

  str_field("WRAP",          s.wrap);
  str_field("WRAP_S",        s.wrap_s);
  str_field("WRAP_T",        s.wrap_t);
  str_field("WRAP_R",        s.wrap_r);
  str_field("FILTER",        s.filter);
  str_field("MIN_FILTER",    s.min_filter);
  str_field("MAG_FILTER",    s.mag_filter);
  str_field("MIPMAP_MODE",   s.mipmap_mode);
  str_field("BORDER_COLOR",  s.border_color);
  str_field("COMPARE",       s.compare);
  float_field("ANISOTROPY",  s.anisotropy);
  float_field("LOD_BIAS",    s.lod_bias);
  float_field("MIN_LOD",     s.min_lod);
  float_field("MAX_LOD",     s.max_lod);
}

// Audio inputs expose only FILTER and WRAP — audio textures are 1-mip
// 2D samplers so the rest of sampler_config (COMPARE / BORDER_COLOR / LOD
// / anisotropy) has no meaningful effect.
static void parse_audio_sampler_config(audio_sampler_config& s, const sajson::value& v)
{
  auto str_field = [&](const char* key, std::string& out) {
    if(auto k = v.find_object_key_insensitive(sajson::literal(key));
       k != v.get_length())
    {
      auto val = v.get_object_value(k);
      if(val.get_type() == sajson::TYPE_STRING)
        out = val.as_string();
    }
  };
  str_field("FILTER", s.filter);
  str_field("WRAP",   s.wrap);
}

// Drop COMPARE from a sampler config whose texture shape has no corresponding
// *Shadow GLSL sampler type. A non-"never" COMPARE makes the runtime call
// QRhiSampler::setTextureCompareOp, which on Vulkan requires the shader-side
// binding to be a shadow sampler (compareEnable=VK_TRUE is a validation
// error otherwise) and on the other backends produces undefined reads. The
// only core-GLSL shape without a shadow variant is 3D — sampler3DShadow is
// not a core type. 2D / 2D-array / cube / cube-array all have shadow
// counterparts and are handled by the emitter.
static void drop_unsupported_compare_3d(sampler_config& s, const char* where)
{
  if(s.compare.empty()) return;
  std::string c = s.compare;
  for(auto& ch : c) ch = (char)tolower(ch);
  if(c == "never") return;
  fmt::print(
      stderr,
      "[isf] {}: COMPARE is set but sampler3DShadow is not a core GLSL "
      "sampler type — ignoring. Use a 2D, 2D-array, cubemap or cubemap-array "
      "shadow sampler instead.\n",
      where);
  s.compare.clear();
}

static void parse_input(image_input& inp, const sajson::value& v)
{
  if(auto k = v.find_object_key_insensitive(sajson::literal("DIMENSIONS"));
     k != v.get_length())
  {
    auto val = v.get_object_value(k);
    if(val.get_type() == sajson::TYPE_INTEGER)
    {
      auto d = val.get_integer_value();
      if(d != 2 && d != 3)
        throw invalid_file{
            "image_input DIMENSIONS must be 2 or 3 (got " + std::to_string(d)
            + "). 1D and 4D textures are not supported."};
      inp.dimensions = d;
    }
  }
  if(auto k = v.find_object_key_insensitive(sajson::literal("DEPTH"));
     k != v.get_length())
  {
    inp.depth = v.get_object_value(k).get_type() == sajson::TYPE_TRUE;
  }
  if(auto k = v.find_object_key_insensitive(sajson::literal("IS_ARRAY"));
     k != v.get_length())
  {
    inp.is_array = v.get_object_value(k).get_type() == sajson::TYPE_TRUE;
  }
  else if(auto k2 = v.find_object_key_insensitive(sajson::literal("ARRAY"));
          k2 != v.get_length())
  {
    inp.is_array = v.get_object_value(k2).get_type() == sajson::TYPE_TRUE;
  }
  // STATIC: shader author opts into "upstream publishes a long-lived
  // QRhiTexture, bind it directly". Engine path = same Flag::GrabsFromSource
  // already used for cube / 3D / array inputs (those grab implicitly
  // because they can't be 2D color attachments). For plain 2D texture
  // inputs both modes are valid — RT-render (compositor pattern) is the
  // safe default; STATIC: true opts into direct binding for static-LUT /
  // IBL-bake / asset-cache producers (avnd gpu_texture_output, etc.).
  if(auto k = v.find_object_key_insensitive(sajson::literal("STATIC"));
     k != v.get_length())
  {
    inp.is_static = v.get_object_value(k).get_type() == sajson::TYPE_TRUE;
  }
  parse_sampler_config(inp.sampler, v);
  if(inp.dimensions == 3)
  {
    drop_unsupported_compare_3d(inp.sampler, "image input (DIMENSIONS: 3)");
    if(inp.is_array)
    {
      throw invalid_file{
          "image input: DIMENSIONS: 3 with ARRAY: true is not supported — "
          "sampler3DArray is not a core GLSL type. Use a 3D texture and drop "
          "ARRAY, or a 2D-array texture and drop DIMENSIONS: 3."};
    }
  }
}
static void parse_input(cubemap_input& inp, const sajson::value& v)
{
  if(auto k = v.find_object_key_insensitive(sajson::literal("DEPTH"));
     k != v.get_length())
  {
    inp.depth = v.get_object_value(k).get_type() == sajson::TYPE_TRUE;
  }
  parse_sampler_config(inp.sampler, v);
}

static void parse_input(event_input& inp, const sajson::value& v) { }

static void parse_input(audio_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "MAX")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_INTEGER)
      {
        inp.max = val.get_integer_value();
      }
    }
  }
  parse_audio_sampler_config(inp.sampler, v);
}

static void parse_input(audioHist_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "MAX")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_INTEGER)
      {
        inp.max = val.get_integer_value();
      }
    }
  }
  parse_audio_sampler_config(inp.sampler, v);
}

// CSF-specific parsing functions
static void parse_input(storage_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "ACCESS")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.access = val.as_string();
    }
    else if(k == "LAYOUT")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_ARRAY)
      {
        std::size_t layout_size = val.get_length();
        inp.layout.reserve(layout_size);

        for(std::size_t j = 0; j < layout_size; j++)
        {
          auto field = val.get_array_element(j);
          if(field.get_type() == sajson::TYPE_OBJECT)
          {
            storage_input::layout_field lf;

            // Parse NAME and TYPE
            for(std::size_t f = 0; f < field.get_length(); f++)
            {
              auto field_key = field.get_object_key(f).as_string();
              if(field_key == "NAME")
              {
                auto name_val = field.get_object_value(f);
                if(name_val.get_type() == sajson::TYPE_STRING)
                  lf.name = name_val.as_string();
              }
              else if(field_key == "TYPE")
              {
                auto type_val = field.get_object_value(f);
                if(type_val.get_type() == sajson::TYPE_STRING)
                  lf.type = type_val.as_string();
              }
            }

            inp.layout.push_back(lf);
          }
        }
      }
    }
    else if(k == "BUFFER_USAGE")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.buffer_usage = val.as_string();
    }
    else if(k == "PERSISTENT")
    {
      inp.persistent = v.get_object_value(i).get_type() == sajson::TYPE_TRUE;
    }
    else if(k == "VISIBILITY")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.visibility = val.as_string();
    }
  }

  // Warn on semantically-impossible combinations. PERSISTENT allocates a
  // ping-pong pair and always emits `_prev` as a readonly buffer — if the
  // primary is write_only, nothing ever writes the data that _prev is
  // supposed to read back, so it's silently always zero.
  if(inp.persistent && inp.access == "write_only")
  {
    throw invalid_file{
        "storage input declared as PERSISTENT + ACCESS: write_only is "
        "invalid — _prev would always read zero (no read path exists to "
        "populate it). Use ACCESS: read_write or read_only with PERSISTENT, "
        "or drop PERSISTENT if you don't need frame history."};
  }

  // Reject empty LAYOUT for non-indirect storage_inputs. The graphics
  // emit at isf_emit_graphics_storage / isf_emit_ssbo_decl produces an
  // empty `readonly buffer NAME_buf { };` block which is invalid GLSL
  // (`buffer { };` requires at least one member declarator). shaderc
  // then fails with a cryptic message pointing at the auto-emitted
  // block. uniform_input has the symmetric check at parse_input(uniform).
  // Indirect-draw SSBOs LEGITIMATELY have empty LAYOUT — they are
  // skipped from graphics emit (isf.cpp:3361-3363) when buffer_usage is
  // non-empty. Match that gate here so legitimate indirect-draw paths
  // pass through unchallenged.
  if(inp.layout.empty() && inp.buffer_usage.empty())
  {
    throw invalid_file{
        "storage_input declares an empty LAYOUT and no BUFFER_USAGE — "
        "the SSBO graphics emit would produce `readonly buffer NAME_buf "
        "{ };` which is invalid GLSL (a buffer block must have at least "
        "one member declarator). Empty LAYOUT only makes sense for "
        "indirect-draw SSBOs which set BUFFER_USAGE: \"indirect_draw\" "
        "or \"indirect_draw_indexed\". Either declare members in LAYOUT "
        "or set BUFFER_USAGE."};
  }
}

static void parse_input(uniform_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();
  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "LAYOUT")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_ARRAY)
      {
        std::size_t layout_size = val.get_length();
        inp.layout.reserve(layout_size);
        for(std::size_t j = 0; j < layout_size; j++)
        {
          auto field = val.get_array_element(j);
          if(field.get_type() != sajson::TYPE_OBJECT)
            continue;
          uniform_input::layout_field lf;
          for(std::size_t f = 0; f < field.get_length(); f++)
          {
            auto fk = field.get_object_key(f).as_string();
            if(fk == "NAME")
            {
              auto nv = field.get_object_value(f);
              if(nv.get_type() == sajson::TYPE_STRING)
                lf.name = nv.as_string();
            }
            else if(fk == "TYPE")
            {
              auto tv = field.get_object_value(f);
              if(tv.get_type() == sajson::TYPE_STRING)
                lf.type = tv.as_string();
            }
          }
          inp.layout.push_back(lf);
        }
      }
    }
    else if(k == "VISIBILITY")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.visibility = val.as_string();
    }
  }
  if(inp.layout.empty())
  {
    throw invalid_file{
        "uniform_input declares an empty LAYOUT — std140 interface blocks "
        "must contain at least one field. Either declare its members in "
        "LAYOUT: [{ NAME, TYPE }, ...] or remove the input."};
  }
}

static void parse_input(texture_input& inp, const sajson::value& v)
{
  if(auto k = v.find_object_key_insensitive(sajson::literal("DIMENSIONS"));
     k != v.get_length())
  {
    auto val = v.get_object_value(k);
    if(val.get_type() == sajson::TYPE_INTEGER)
    {
      auto d = val.get_integer_value();
      if(d != 2 && d != 3)
        throw invalid_file{
            "texture_input DIMENSIONS must be 2 or 3 (got " + std::to_string(d)
            + "). 1D and 4D textures are not supported."};
      inp.dimensions = d;
    }
  }
  parse_sampler_config(inp.sampler, v);
  if(inp.dimensions == 3)
    drop_unsupported_compare_3d(inp.sampler, "texture input (DIMENSIONS: 3)");
}

// Parse a COPY_FROM JSON object.
static std::optional<geometry_input::copy_from>
parse_copy_from(const sajson::value& obj)
{
  if(obj.get_type() != sajson::TYPE_OBJECT)
    return std::nullopt;

  geometry_input::copy_from cf;
  for(std::size_t i = 0; i < obj.get_length(); i++)
  {
    auto k = obj.get_object_key(i).as_string();
    auto v = obj.get_object_value(i);
    if(v.get_type() != sajson::TYPE_STRING)
      continue;

    if(k == "GEOMETRY")
      cf.geometry = v.as_string();
    else if(k == "ATTRIBUTE")
      cf.attribute = v.as_string();
    else if(k == "AUXILIARY")
      cf.auxiliary = v.as_string();
  }

  if(cf.geometry.empty())
    return std::nullopt;

  return cf;
}

// Detect whether an AUXILIARY entry declares a texture (TYPE: "image" /
// "cubemap" / "texture") rather than a buffer. Buffers are the default
// (TYPE absent, or "storage" / "buffer").
// Three-way classification of an AUXILIARY JSON entry:
//   Ssbo    — default; declared either without TYPE or with TYPE:
//             "storage" / "buffer" / "ssbo". Layout maps to an std430
//             `buffer` block bound as bufferLoad / bufferStore / bufferLoadStore.
//   Ubo     — TYPE: "uniform" / "ubo". Layout maps to an std140 `uniform`
//             block bound as uniformBuffer.
//   Texture — TYPE: "image" / "texture" / "cubemap" / "image_cube" /
//             "storage_*". Goes through the auxiliary_texture_request pool.
enum class aux_kind { Ssbo, Ubo, Texture };

static aux_kind aux_entry_kind(const sajson::value& aux_obj)
{
  auto k = aux_obj.find_object_key_insensitive(sajson::literal("TYPE"));
  if(k == aux_obj.get_length())
    return aux_kind::Ssbo;
  auto v = aux_obj.get_object_value(k);
  if(v.get_type() != sajson::TYPE_STRING)
    return aux_kind::Ssbo;
  std::string t = v.as_string();
  for(auto& c : t) c = (char)tolower(c);
  if(t == "image" || t == "texture" || t == "cubemap" || t == "image_cube"
     || t == "storage_image" || t == "storage_cube"
     || t == "storage_image_array" || t == "storage_3d")
    return aux_kind::Texture;
  if(t == "uniform" || t == "ubo")
    return aux_kind::Ubo;
  return aux_kind::Ssbo;
}

// Parse a single texture auxiliary entry.
static void parse_auxiliary_texture(
    const sajson::value& aux_obj,
    geometry_input::auxiliary_texture_request& out)
{
  for(std::size_t f = 0; f < aux_obj.get_length(); f++)
  {
    auto fkey = aux_obj.get_object_key(f).as_string();
    auto fval = aux_obj.get_object_value(f);

    if(fkey == "NAME" && fval.get_type() == sajson::TYPE_STRING)
      out.name = fval.as_string();
    else if(fkey == "TYPE" && fval.get_type() == sajson::TYPE_STRING)
    {
      std::string t = fval.as_string();
      for(auto& c : t) c = (char)tolower(c);
      if(t == "cubemap" || t == "image_cube")
        out.is_cubemap = true;
      else if(t == "storage_image")
        out.is_storage = true;
      else if(t == "storage_cube")
      { out.is_storage = true; out.is_cubemap = true; }
      else if(t == "storage_image_array")
      { out.is_storage = true; out.is_array = true; }
      else if(t == "storage_3d")
      { out.is_storage = true; out.dimensions = 3; }
    }
    else if(fkey == "DIMENSIONS")
    {
      if(fval.get_type() == sajson::TYPE_INTEGER)
        out.dimensions = fval.get_integer_value();
    }
    else if(fkey == "IS_ARRAY" || fkey == "ARRAY")
      out.is_array = (fval.get_type() == sajson::TYPE_TRUE);
    else if(fkey == "DEPTH")
    {
      // DEPTH overload — context-dependent:
      //   "DEPTH": true   → legacy sampleable-depth flag (paired with
      //                     COMPARE for shadow-comparison samplers)
      //   "DEPTH": <int>  → 3D-texture depth dimension literal
      //   "DEPTH": "<expr>" → 3D-texture depth dimension expression
      // Distinguishable by sajson type so authors can use either form
      // without the parser silently dropping one.
      const auto t = fval.get_type();
      if(t == sajson::TYPE_TRUE)
        out.is_depth = true;
      else if(t == sajson::TYPE_FALSE)
        out.is_depth = false;
      else if(t == sajson::TYPE_INTEGER)
        out.depth_expression = std::to_string(fval.get_integer_value());
      else if(t == sajson::TYPE_DOUBLE)
        out.depth_expression = std::to_string(fval.get_double_value());
      else if(t == sajson::TYPE_STRING)
        out.depth_expression = fval.as_string();
    }
    else if(fkey == "STORAGE")
      out.is_storage = (fval.get_type() == sajson::TYPE_TRUE);
    else if(fkey == "FORMAT" && fval.get_type() == sajson::TYPE_STRING)
      out.format = fval.as_string();
    else if(fkey == "ACCESS" && fval.get_type() == sajson::TYPE_STRING)
      out.access = fval.as_string();
    // WIDTH / HEIGHT / LAYERS — same expression-or-literal convention as
    // csf_image_input. Strings allow `$var` substitution against the
    // shader's long/float inputs at allocation time.
    else if(fkey == "WIDTH")
    {
      const auto t = fval.get_type();
      if(t == sajson::TYPE_INTEGER)
        out.width_expression = std::to_string(fval.get_integer_value());
      else if(t == sajson::TYPE_DOUBLE)
        out.width_expression = std::to_string(fval.get_double_value());
      else if(t == sajson::TYPE_STRING)
        out.width_expression = fval.as_string();
    }
    else if(fkey == "HEIGHT")
    {
      const auto t = fval.get_type();
      if(t == sajson::TYPE_INTEGER)
        out.height_expression = std::to_string(fval.get_integer_value());
      else if(t == sajson::TYPE_DOUBLE)
        out.height_expression = std::to_string(fval.get_double_value());
      else if(t == sajson::TYPE_STRING)
        out.height_expression = fval.as_string();
    }
    else if(fkey == "LAYERS")
    {
      const auto t = fval.get_type();
      if(t == sajson::TYPE_INTEGER)
        out.layers_expression = std::to_string(fval.get_integer_value());
      else if(t == sajson::TYPE_DOUBLE)
        out.layers_expression = std::to_string(fval.get_double_value());
      else if(t == sajson::TYPE_STRING)
        out.layers_expression = fval.as_string();
    }
  }

  // depth_expression non-empty implies a 3D texture even if DIMENSIONS
  // wasn't set explicitly. Mirrors csf_image_input::is3D() semantics —
  // saves the author from writing both fields.
  if(!out.depth_expression.empty() && out.dimensions == 2)
    out.dimensions = 3;

  // Auto-infer storage-image semantics when FORMAT is explicitly set to
  // anything other than the sampled-texture default (rgba8). Allows
  // author-friendly declarations like:
  //
  //   { "NAME": "voxel_grid", "TYPE": "image", "ACCESS": "read_write",
  //     "FORMAT": "r32ui", "DIMENSIONS": 3, ... }
  //
  // to be parsed as a storage image without forcing the author to
  // additionally write `"STORAGE": true` or use the more-cryptic
  // `"TYPE": "storage_3d"`.
  //
  // ONLY uses FORMAT — NOT ACCESS — because `access` defaults to
  // "read_write" in the struct (it's only meaningful when is_storage is
  // already true), so an ACCESS-based heuristic would mis-fire on every
  // sampled-aux entry that doesn't explicitly override it. FORMAT
  // defaults to "rgba8" which is also the sampled-image default, so the
  // discriminator is "did the author explicitly write a non-rgba8
  // FORMAT?" — unambiguous either way. If you want a storage rgba8
  // image, write `"STORAGE": true` explicitly.
  if(!out.is_storage)
  {
    const bool format_implies_storage
        = !out.format.empty() && out.format != "rgba8";
    if(format_implies_storage)
      out.is_storage = true;
  }
  // Inherit the flat sampler_config fields (WRAP/FILTER/COMPARE/…).
  parse_sampler_config(out.sampler, aux_obj);
  // Storage images don't use the sampler; regular samplers on a 3D texture
  // have no shadow variant. Cubemap and 2D-array shapes have shadow variants
  // and are fine.
  if(!out.is_storage && !out.is_cubemap && out.dimensions == 3)
    drop_unsupported_compare_3d(
        out.sampler,
        fmt::format("auxiliary texture '{}' (DIMENSIONS: 3)", out.name).c_str());
  // Cube-arrays (samplerCubeArray / imageCubeArray) are unsupported: every
  // QRhi backend silently collapses `CubeMap | TextureArray` to one flag or
  // the other at view-creation time (Vulkan qrhivulkan.cpp:7736+,
  // D3D12:1160+, Metal:4025+, GL:6124+), so the shader-side type and the
  // bound resource disagree. Reject at parse time rather than ship broken
  // bindings. Same story for 3D cubemaps (nonsensical).
  if(out.is_cubemap && out.is_array)
  {
    throw invalid_file{
        "auxiliary texture '" + out.name
        + "': cubemap + ARRAY is not supported on any QRhi backend "
          "(cube-array views are not constructible). Use a plain cubemap, "
          "or decompose to a 2D array and do face math in the shader."};
  }
  if(out.is_cubemap && out.dimensions == 3)
  {
    fmt::print(
        stderr,
        "[isf] auxiliary texture '{}': cubemap with DIMENSIONS: 3 is "
        "meaningless (cube faces are 2D). Ignoring DIMENSIONS.\n",
        out.name);
    out.dimensions = 2;
  }
}

// Parse an AUXILIARY JSON array, dispatching each entry by TYPE into
// either the buffer list or the texture list.
// Shared by geometry_input parsing and top-level AUXILIARY key.
static void parse_auxiliary_array(
    const sajson::value& val,
    std::vector<geometry_input::auxiliary_request>& out_buffers,
    std::vector<geometry_input::auxiliary_texture_request>& out_textures)
{
  if(val.get_type() != sajson::TYPE_ARRAY)
    return;

  std::size_t aux_count = val.get_length();
  out_buffers.reserve(out_buffers.size() + aux_count);

  for(std::size_t j = 0; j < aux_count; j++)
  {
    auto aux_obj = val.get_array_element(j);
    if(aux_obj.get_type() != sajson::TYPE_OBJECT)
      continue;

    const aux_kind kind = aux_entry_kind(aux_obj);
    if(kind == aux_kind::Texture)
    {
      geometry_input::auxiliary_texture_request tr;
      parse_auxiliary_texture(aux_obj, tr);
      if(!tr.name.empty())
        out_textures.push_back(std::move(tr));
      continue;
    }

    geometry_input::auxiliary_request ar;
    // UBO kind: flag set on the request so both parser-side GLSL emission
    // and runtime-side binding know to treat it as a std140 uniform block.
    // Buffer-kind SSBO is the default (is_uniform stays false).
    ar.is_uniform = (kind == aux_kind::Ubo);

    for(std::size_t f = 0; f < aux_obj.get_length(); f++)
    {
      auto fkey = aux_obj.get_object_key(f).as_string();
      auto fval = aux_obj.get_object_value(f);

      if(fkey == "NAME" && fval.get_type() == sajson::TYPE_STRING)
        ar.name = fval.as_string();
      else if(fkey == "ACCESS" && fval.get_type() == sajson::TYPE_STRING)
        ar.access = fval.as_string();
      else if(fkey == "SIZE")
      {
        if(fval.get_type() == sajson::TYPE_STRING)
          ar.size = fval.as_string();
        else if(fval.get_type() == sajson::TYPE_INTEGER)
          ar.size = std::to_string(fval.get_integer_value());
        else if(fval.get_type() == sajson::TYPE_DOUBLE)
          ar.size = std::to_string((int)fval.get_double_value());
      }
      else if(fkey == "LAYOUT" && fval.get_type() == sajson::TYPE_ARRAY)
      {
        std::size_t layout_size = fval.get_length();
        ar.layout.reserve(layout_size);
        for(std::size_t l = 0; l < layout_size; l++)
        {
          auto field = fval.get_array_element(l);
          if(field.get_type() != sajson::TYPE_OBJECT)
            continue;
          storage_input::layout_field lf;
          for(std::size_t ff = 0; ff < field.get_length(); ff++)
          {
            auto field_key = field.get_object_key(ff).as_string();
            if(field_key == "NAME")
            {
              auto name_val = field.get_object_value(ff);
              if(name_val.get_type() == sajson::TYPE_STRING)
                lf.name = name_val.as_string();
            }
            else if(field_key == "TYPE")
            {
              auto type_val = field.get_object_value(ff);
              if(type_val.get_type() == sajson::TYPE_STRING)
                lf.type = type_val.as_string();
            }
          }
          ar.layout.push_back(lf);
        }
      }
      else if(fkey == "COPY_FROM")
      {
        ar.forward = parse_copy_from(fval);
      }
      else if(fkey == "PERSISTENT")
      {
        if(fval.get_type() == sajson::TYPE_TRUE)
          ar.persistent = true;
        else if(fval.get_type() == sajson::TYPE_FALSE)
          ar.persistent = false;
      }
    }

    if(ar.access.empty())
      ar.access = "read_only";

    out_buffers.push_back(std::move(ar));
  }
}

// Validate that every geometry_input ATTRIBUTE.TYPE either names a
// built-in GLSL scalar/vector/matrix type or matches a user-defined
// struct declared in descriptor::types. Run AFTER both RESOURCES and
// TYPES are parsed (TYPES may appear in any order in the JSON) — i.e.
// once at the end of parse_csf / parse_raw_raster_pipeline. Catches
// typos in TYPE strings at parse time instead of as a confusing
// "undefined identifier" GLSL compile error 30 lines deep into the
// generated shader.
static void validate_attribute_types(const descriptor& d)
{
  static constexpr std::string_view builtins[] = {
    "float", "int",   "uint",  "bool",
    "vec2",  "vec3",  "vec4",
    "ivec2", "ivec3", "ivec4",
    "uvec2", "uvec3", "uvec4",
    "mat2",  "mat3",  "mat4"
  };
  auto is_builtin = [](std::string_view t) noexcept {
    for(auto b : builtins) if(t == b) return true;
    return false;
  };
  auto is_user_type = [&](std::string_view t) noexcept {
    for(const auto& td : d.types) if(td.name == t) return true;
    return false;
  };
  for(const auto& inp : d.inputs)
  {
    auto* gi = ossia::get_if<geometry_input>(&inp.data);
    if(!gi) continue;
    for(const auto& ar : gi->attributes)
    {
      if(ar.type.empty()) continue;
      if(is_builtin(ar.type) || is_user_type(ar.type)) continue;
      throw invalid_file{
          "ATTRIBUTES \"" + ar.name + "\" on geometry resource \"" + inp.name
          + "\" declares TYPE \"" + ar.type
          + "\", which is neither a built-in GLSL scalar/vector/matrix type "
            "nor a user-defined type from the TYPES section."};
    }
  }
}

static void parse_input(geometry_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "ATTRIBUTES")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_ARRAY)
      {
        std::size_t attr_count = val.get_length();
        inp.attributes.reserve(attr_count);

        for(std::size_t j = 0; j < attr_count; j++)
        {
          auto attr_obj = val.get_array_element(j);
          if(attr_obj.get_type() != sajson::TYPE_OBJECT)
            continue;

          geometry_input::attribute_request ar;

          for(std::size_t f = 0; f < attr_obj.get_length(); f++)
          {
            auto fkey = attr_obj.get_object_key(f).as_string();
            auto fval = attr_obj.get_object_value(f);

            if(fkey == "NAME" && fval.get_type() == sajson::TYPE_STRING)
              ar.name = fval.as_string();
            else if(fkey == "SEMANTIC" && fval.get_type() == sajson::TYPE_STRING)
              ar.semantic = fval.as_string();
            else if(fkey == "TYPE" && fval.get_type() == sajson::TYPE_STRING)
              ar.type = fval.as_string();
            else if(fkey == "ACCESS" && fval.get_type() == sajson::TYPE_STRING)
              ar.access = fval.as_string();
            else if(fkey == "RATE" && fval.get_type() == sajson::TYPE_STRING)
              ar.rate = fval.as_string();
            else if(fkey == "REQUIRED")
            {
              if(fval.get_type() == sajson::TYPE_FALSE)
                ar.required = false;
              else if(fval.get_type() == sajson::TYPE_TRUE)
                ar.required = true;
            }
            else if(fkey == "COPY_FROM")
            {
              ar.forward = parse_copy_from(fval);
            }
          }

          // Default access to read_write if not specified
          if(ar.access.empty())
            ar.access = "read_write";

          // Default semantic to the attribute name if not specified
          if(ar.semantic.empty())
            ar.semantic = ar.name;

          inp.attributes.push_back(std::move(ar));
        }
      }
    }
    else if(k == "VERTEX_COUNT")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.vertex_count = val.as_string();
      else if(val.get_type() == sajson::TYPE_INTEGER)
        inp.vertex_count = std::to_string(val.get_integer_value());
      else if(val.get_type() == sajson::TYPE_DOUBLE)
        inp.vertex_count = std::to_string((int)val.get_double_value());
    }
    else if(k == "INSTANCE_COUNT")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.instance_count = val.as_string();
      else if(val.get_type() == sajson::TYPE_INTEGER)
        inp.instance_count = std::to_string(val.get_integer_value());
      else if(val.get_type() == sajson::TYPE_DOUBLE)
        inp.instance_count = std::to_string((int)val.get_double_value());
    }
    else if(k == "FORMAT_ID")
    {
      // String tag stamped on the consumer geometry's filter_tag
      // (rapidhash truncated to 32 bits). Lets a CSF that produces
      // primitive-cloud-shaped output declare its format identity in
      // the JSON header without engine-side knowledge of the format.
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.format_id = val.as_string();
    }
    else if(k == "AUXILIARY")
    {
      parse_auxiliary_array(v.get_object_value(i), inp.auxiliary, inp.auxiliary_textures);
    }
    else if(k == "INDIRECT")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_OBJECT)
      {
        geometry_input::indirect_request req;
        for(std::size_t j = 0; j < val.get_length(); j++)
        {
          auto ik = val.get_object_key(j).as_string();
          boost::algorithm::to_upper(ik);
          if(ik == "COUNT")
          {
            auto iv = val.get_object_value(j);
            if(iv.get_type() == sajson::TYPE_STRING)
              req.count = iv.as_string();
            else if(iv.get_type() == sajson::TYPE_INTEGER)
              req.count = std::to_string(iv.get_integer_value());
            else if(iv.get_type() == sajson::TYPE_DOUBLE)
              req.count = std::to_string((int)iv.get_double_value());
          }
        }
        if(req.count.empty())
          req.count = "1";
        inp.indirect = req;
      }
    }
    else if(k == "INDIRECT_DRAW")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_TRUE)
        inp.indirect = geometry_input::indirect_request{.count = "1"};
    }
  }
}

// Known GLSL image format qualifiers. Used for a parse-time sanity check —
// lets the shader author see a typo ("rgba16" vs "rgba16f") before the
// runtime silently falls back to rgba8. Strict GLSL image-format typing
// validation (matching imageStore argument types to declared formats) would
// need a full GLSL AST which this parser does not build; the most useful
// check we can do cheaply is reject unknown format strings.
static bool isf_is_known_image_format(std::string fmt)
{
  boost::algorithm::to_lower(fmt);
  static const ossia::hash_set<std::string> known{
      "rgba8",  "rgba8_snorm",  "rgba8ui", "rgba8i",
      "rgba16", "rgba16_snorm", "rgba16f", "rgba16ui", "rgba16i",
      "rgba32f","rgba32ui",     "rgba32i",
      "rg8",    "rg8_snorm",    "rg8ui",   "rg8i",
      "rg16",   "rg16_snorm",   "rg16f",   "rg16ui", "rg16i",
      "rg32f",  "rg32ui",       "rg32i",
      "r8",     "r8_snorm",     "r8ui",    "r8i",
      "r16",    "r16_snorm",    "r16f",    "r16ui",  "r16i",
      "r32f",   "r32ui",        "r32i",
      "rgb10_a2", "rgb10_a2ui", "r11f_g11f_b10f",
      "bgra8"};
  return known.count(fmt) > 0;
}

static void parse_input(csf_image_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "ACCESS")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.access = val.as_string();
    }
    else if(k == "FORMAT")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
      {
        inp.format = val.as_string();
        if(!inp.format.empty() && !isf_is_known_image_format(inp.format))
        {
          fmt::print(
              stderr,
              "[isf] csf_image_input FORMAT \"{}\" is not a recognised GLSL "
              "image qualifier — will fall back to rgba8 at runtime. Check "
              "for typos (e.g. \"rgba16\" vs \"rgba16f\").\n",
              inp.format);
        }
      }
    }
    else if(k == "WIDTH")
    {
      auto val = v.get_object_value(i);
      auto t = val.get_type();
      if(t == sajson::TYPE_STRING)
      {
        inp.width_expression = val.as_string();
      }
      else if(t == sajson::TYPE_DOUBLE)
      {
        inp.width_expression = std::to_string(val.get_double_value());
      }
      else if(t == sajson::TYPE_INTEGER)
      {
        inp.width_expression = std::to_string(val.get_integer_value());
      }
    }
    else if(k == "HEIGHT")
    {
      auto val = v.get_object_value(i);
      auto t = val.get_type();
      if(t == sajson::TYPE_STRING)
      {
        inp.height_expression = val.as_string();
      }
      else if(t == sajson::TYPE_DOUBLE)
      {
        inp.height_expression = std::to_string(val.get_double_value());
      }
      else if(t == sajson::TYPE_INTEGER)
      {
        inp.height_expression = std::to_string(val.get_integer_value());
      }
    }
    else if(k == "DEPTH")
    {
      auto val = v.get_object_value(i);
      auto t = val.get_type();
      if(t == sajson::TYPE_STRING)
      {
        inp.depth_expression = val.as_string();
      }
      else if(t == sajson::TYPE_DOUBLE)
      {
        inp.depth_expression = std::to_string(val.get_double_value());
      }
      else if(t == sajson::TYPE_INTEGER)
      {
        inp.depth_expression = std::to_string(val.get_integer_value());
      }
    }
    else if(k == "DIMENSIONS")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_INTEGER)
      {
        auto d = val.get_integer_value();
        if(d != 2 && d != 3)
          throw invalid_file{
              "csf_image_input DIMENSIONS must be 2 or 3 (got " + std::to_string(d)
              + "). 1D and 4D textures are not supported."};
        inp.dimensions = d;
      }
      else if(val.get_type() == sajson::TYPE_DOUBLE)
      {
        auto d = (int)val.get_double_value();
        if(d != 2 && d != 3)
          throw invalid_file{
              "csf_image_input DIMENSIONS must be 2 or 3 (got " + std::to_string(d)
              + "). 1D and 4D textures are not supported."};
        inp.dimensions = d;
      }
    }
    else if(k == "VISIBILITY")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.visibility = val.as_string();
    }
    else if(k == "PERSISTENT")
    {
      inp.persistent = v.get_object_value(i).get_type() == sajson::TYPE_TRUE;
    }
    else if(k == "GENERATE_MIPS")
    {
      inp.generate_mips = v.get_object_value(i).get_type() == sajson::TYPE_TRUE;
    }
    else if(k == "IS_ARRAY" || k == "ARRAY")
    {
      inp.is_array = v.get_object_value(i).get_type() == sajson::TYPE_TRUE;
    }
    else if(k == "LAYERS")
    {
      auto val = v.get_object_value(i);
      auto t = val.get_type();
      if(t == sajson::TYPE_STRING)
        inp.layers_expression = val.as_string();
      else if(t == sajson::TYPE_INTEGER)
        inp.layers_expression = std::to_string(val.get_integer_value());
      else if(t == sajson::TYPE_DOUBLE)
        inp.layers_expression = std::to_string(val.get_double_value());
    }
    else if(k == "CUBEMAP" || k == "IS_CUBE")
    {
      inp.cubemap = v.get_object_value(i).get_type() == sajson::TYPE_TRUE;
    }
  }

  // See the matching note on storage_input — persistent + write_only has no
  // useful semantics because _prev is readonly and nothing writes it.
  if(inp.persistent && inp.access == "write_only")
  {
    throw invalid_file{
        "csf_image_input declared as PERSISTENT + ACCESS: write_only is "
        "invalid — _prev would always read zero (no read path exists to "
        "populate it). Use ACCESS: read_write or read_only with PERSISTENT, "
        "or drop PERSISTENT."};
  }

  // Cube-array writable images are unsupported (see sampler-side analysis in
  // parse_auxiliary_texture / isf.hpp). Reject here so downstream allocators
  // and the GLSL emitter can assume the combo never shows up.
  if(inp.is_array && inp.cubemap)
  {
    throw invalid_file{
        "csf_image_input: IS_ARRAY + image_cube is not supported — "
        "imageCubeArray views are broken on every QRhi backend. Bind N "
        "separate cubemaps or use image2DArray and do face math in the "
        "shader."};
  }
  // 3D arrays do not exist as a core GLSL image type either.
  if(inp.is_array && inp.is3D())
  {
    fmt::print(
        stderr,
        "[isf] csf_image_input: IS_ARRAY + 3D image (DIMENSIONS: 3 or DEPTH "
        "expression) is not a valid GLSL type (image3DArray is not core). "
        "Dropping IS_ARRAY.\n");
    inp.is_array = false;
  }
}

static void parse_input(audioFFT_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "MAX")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_INTEGER)
      {
        inp.max = val.get_integer_value();
      }
    }
  }
  parse_audio_sampler_config(inp.sampler, v);
}

static void parse_input(long_input& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "VALUES")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_ARRAY)
      {
        const std::size_t N = val.get_length();
        inp.values.reserve(N);
        for(std::size_t i = 0; i < N; i++)
        {
          auto arr_value = val.get_array_element(i);
          if(arr_value.get_type() == sajson::TYPE_INTEGER)
          {
            inp.values.push_back(int64_t(arr_value.get_integer_value()));
          }
          else if(arr_value.get_type() == sajson::TYPE_DOUBLE)
          {
            inp.values.push_back(arr_value.get_double_value());
          }
          else if(arr_value.get_type() == sajson::TYPE_STRING)
          {
            inp.values.push_back(arr_value.as_string());
          }
        }
      }
    }
    else if(k == "LABELS")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_ARRAY)
      {
        const std::size_t N = val.get_length();
        inp.labels.reserve(N);
        for(std::size_t i = 0; i < N; i++)
        {
          auto arr_value = val.get_array_element(i);
          if(arr_value.get_type() == sajson::TYPE_STRING)
          {
            inp.labels.push_back(arr_value.as_string());
          }
        }
      }
    }
    else if(k == "DEFAULT")
    {
      auto val = v.get_object_value(i);
      inp.def = parse_input_impl(val, int64_t{});
    }
    else if(k == "MIN")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_INTEGER)
        inp.min = val.get_integer_value();
      else if(val.get_type() == sajson::TYPE_DOUBLE)
        inp.min = static_cast<int64_t>(val.get_double_value());
    }
    else if(k == "MAX")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_INTEGER)
        inp.max = val.get_integer_value();
      else if(val.get_type() == sajson::TYPE_DOUBLE)
        inp.max = static_cast<int64_t>(val.get_double_value());
    }
  }

  // If we have VALUES/LABELS (enum mode), clamp def to valid index
  if(!inp.values.empty())
  {
    inp.def = std::min((int64_t)inp.def, (int64_t)(inp.values.size()) - 1);
    auto min_size = std::min(inp.labels.size(), inp.values.size());
    if(min_size > 0)
      inp.def = std::min(inp.def, min_size - 1);
    if(inp.labels.size() < min_size)
      inp.labels.resize(min_size);
    if(inp.values.size() < min_size)
      inp.values.resize(min_size);
  }
}

static auto make_value(std::optional<double>& res, double f, auto op)
{
  f = op(f);
  res = f;
}
static auto make_value(double& res, double f, auto op)
{
  f = op(f);
  res = f;
}

static auto make_value(std::optional<std::array<double, 2>>& res, double f, auto op)
{
  f = op(f);
  res = std::array<double, 2>{f, f};
}

static auto
make_value(std::optional<std::array<double, 2>>& res, std::array<double, 2> f, auto op)
{
  res = std::array<double, 2>{op(f[0]), op(f[1])};
}

static auto make_value(std::optional<std::array<double, 3>>& res, double f, auto op)
{
  f = op(f);
  res = std::array<double, 3>{f, f, f};
}

static auto
make_value(std::optional<std::array<double, 3>>& res, std::array<double, 3> f, auto op)
{
  res = std::array<double, 3>{op(f[0]), op(f[1]), op(f[2])};
}

static auto make_value(std::optional<std::array<double, 4>>& res, double f, auto op)
{
  f = op(f);
  res = std::array<double, 4>{f, f, f, f};
}

static auto
make_value(std::optional<std::array<double, 4>>& res, std::array<double, 4> f, auto op)
{
  res = std::array<double, 4>{op(f[0]), op(f[1]), op(f[2]), op(f[3])};
}

template <typename T>
static auto make_value(std::optional<T>& res, std::optional<T> f, auto op)
{
  assert(f);
  make_value(res, *f, op);
  assert(res);
}

static auto check_value_greater(double& min, double& max)
{
  if(min > max)
    std::swap(min, max);
}

template <std::size_t N>
static auto check_value_greater(std::array<double, N>& min, std::array<double, N>& max)
{
  for(int i = 0; i < N; i++)
    if(min[i] > max[i])
      std::swap(min[i], max[i]);
}
template <typename T>
static auto check_value_greater(std::optional<T>& min, std::optional<T>& max)
{
  assert(min);
  assert(max);
  check_value_greater(*min, *max);
}

template <typename Input_T>
  requires Input_T::has_minmax::value
static void parse_input(Input_T& inp, const sajson::value& v)
{
  using value_type = typename Input_T::value_type;
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "MIN")
    {
      auto val = v.get_object_value(i);
      inp.min = parse_input_impl(val, value_type{});
    }
    else if(k == "MAX")
    {
      auto val = v.get_object_value(i);
      inp.max = parse_input_impl(val, value_type{});
    }
    else if(k == "DEFAULT")
    {
      auto val = v.get_object_value(i);
      inp.def = parse_input_impl(val, value_type{});
    }
    else if(k == "AS_COLOR")
    {
      if constexpr(requires { inp.as_color; })
      {
        inp.as_color = v.get_object_value(i).get_type() == sajson::TYPE_TRUE;
      }
    }
  }

  // Handle shaders without min / max

  if(!inp.min && !inp.max)
  {
    if(!inp.def)
    {
      make_value(inp.min, 0., std::identity{});
      make_value(inp.max, 1., std::identity{});
    }
    else
    {
      make_value(inp.min, inp.def, [](double v) { return v != 0 ? -std::abs(v) : -1.; });
      make_value(
          inp.max, inp.def, [](double v) { return v != 0 ? 2. * std::abs(v) : 1.; });
    }
  }
  else if(!inp.min)
  {
    if(!inp.def)
    {
      make_value(inp.min, inp.max, [](double v) {
        if(v == 0.)
          return -1.;
        return v - std::abs(v);
      });
    }
    else
    {
      make_value(inp.min, inp.def, [](double v) {
        if(v == 0.)
          return -1.;
        return v - std::abs(v);
      });
    }
  }
  else if(!inp.max)
  {
    make_value(inp.max, inp.min, [](double v) {
      if(v < 0)
        return -v;
      else
        return v + std::abs(v);
    });
  }

  if constexpr(requires { inp.min.reset(); })
  {
    assert(inp.min);
    assert(inp.max);
  }

  // Some ISF shaders have e.g. "MIN": 0, "MAX": -5 to show them reversed in the ISF editor gui...
  check_value_greater(inp.min, inp.max);

  if(inp.min == inp.max)
  {
    if((inp.def == inp.min && inp.def == inp.max) || !inp.def)
    {
      make_value(inp.max, inp.min, [](double v) {
        if(v < 0.)
          return 0.;
        else if(v == 0.)
          return 1.;
        else
          return 2. * v;
      });
    }
    else
    {
      make_value(inp.max, inp.def, [](double v) { return 2. * std::abs(v); });
    }
  }

  if(inp.def < inp.min)
    inp.def = inp.min;

  if(inp.def > inp.max)
    inp.def = inp.max;
}

template <typename Input_T>
  requires Input_T::has_default::value
static void parse_input(Input_T& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "DEFAULT")
    {
      auto val = v.get_object_value(i);
      inp.def = parse_input_impl(val, typename Input_T::value_type{});
    }
  }
}

template <typename T>
input parse(const sajson::value& v)
{
  input i;
  parse_input_base(i, v);
  T inp;
  parse_input(inp, v);
  i.data = inp;
  return i;
}

// --- PIPELINE_STATE / MULTIVIEW parsing helpers ---------------------------

static bool get_bool(const sajson::value& v, bool& out)
{
  if(v.get_type() == sajson::TYPE_TRUE) { out = true;  return true; }
  if(v.get_type() == sajson::TYPE_FALSE){ out = false; return true; }
  return false;
}
static bool get_float(const sajson::value& v, float& out)
{
  if(v.get_type() == sajson::TYPE_DOUBLE)  { out = (float)v.get_double_value();  return true; }
  if(v.get_type() == sajson::TYPE_INTEGER) { out = (float)v.get_integer_value(); return true; }
  return false;
}
static bool get_int(const sajson::value& v, int& out)
{
  if(v.get_type() == sajson::TYPE_INTEGER) { out = v.get_integer_value(); return true; }
  if(v.get_type() == sajson::TYPE_DOUBLE)  { out = (int)v.get_double_value(); return true; }
  return false;
}
static bool get_uint(const sajson::value& v, uint32_t& out)
{
  int x{};
  if(get_int(v, x)) { out = (uint32_t)x; return true; }
  return false;
}
static bool get_str(const sajson::value& v, std::string& out)
{
  if(v.get_type() == sajson::TYPE_STRING) { out = v.as_string(); return true; }
  return false;
}

static void parse_blend_attachment(const sajson::value& v, blend_attachment& out)
{
  if(v.get_type() != sajson::TYPE_OBJECT)
    return;
  std::size_t n = v.get_length();
  for(std::size_t i = 0; i < n; i++)
  {
    auto k = v.get_object_key(i).as_string();
    auto val = v.get_object_value(i);
    bool b{};
    if     (k == "ENABLE"     ) { get_bool(val, b); out.enable = b; }
    else if(k == "SRC_COLOR"  ) get_str(val, out.src_color);
    else if(k == "DST_COLOR"  ) get_str(val, out.dst_color);
    else if(k == "OP_COLOR"   ) get_str(val, out.op_color);
    else if(k == "SRC_ALPHA"  ) get_str(val, out.src_alpha);
    else if(k == "DST_ALPHA"  ) get_str(val, out.dst_alpha);
    else if(k == "OP_ALPHA"   ) get_str(val, out.op_alpha);
    else if(k == "COLOR_WRITE") get_str(val, out.color_write);
    // Legacy shorter names
    else if(k == "SRC"        ) { get_str(val, out.src_color); out.src_alpha = out.src_color; }
    else if(k == "DST"        ) { get_str(val, out.dst_color); out.dst_alpha = out.dst_color; }
    else if(k == "OP"         ) { get_str(val, out.op_color);  out.op_alpha  = out.op_color;  }
  }
}

static void parse_stencil_op_state(const sajson::value& v, stencil_op_state& out)
{
  if(v.get_type() != sajson::TYPE_OBJECT)
    return;
  std::size_t n = v.get_length();
  for(std::size_t i = 0; i < n; i++)
  {
    auto k = v.get_object_key(i).as_string();
    auto val = v.get_object_value(i);
    if     (k == "FAIL_OP"      ) get_str(val, out.fail_op);
    else if(k == "DEPTH_FAIL_OP") get_str(val, out.depth_fail_op);
    else if(k == "PASS_OP"      ) get_str(val, out.pass_op);
    else if(k == "COMPARE_OP"   ) get_str(val, out.compare_op);
    else if(k == "COMPARE"      ) get_str(val, out.compare_op);
  }
}

static void parse_pipeline_state(const sajson::value& v, pipeline_state& out)
{
  if(v.get_type() != sajson::TYPE_OBJECT)
    return;
  std::size_t n = v.get_length();
  for(std::size_t i = 0; i < n; i++)
  {
    auto k = v.get_object_key(i).as_string();
    auto val = v.get_object_value(i);
    bool b{};
    float f{};
    uint32_t u{};
    std::string s;

    if     (k == "DEPTH_TEST" )             { if(get_bool(val, b)) out.depth_test  = b; }
    else if(k == "DEPTH_WRITE")             { if(get_bool(val, b)) out.depth_write = b; }
    else if(k == "DEPTH_COMPARE")           { if(get_str(val, s))  out.depth_compare = s; }
    else if(k == "DEPTH_BIAS")              { if(get_float(val, f)) out.depth_bias = f; }
    else if(k == "SLOPE_SCALED_DEPTH_BIAS") { if(get_float(val, f)) out.slope_scaled_depth_bias = f; }
    else if(k == "CULL_MODE")               { if(get_str(val, s))  out.cull_mode = s; }
    else if(k == "FRONT_FACE")              { if(get_str(val, s))  out.front_face = s; }
    else if(k == "POLYGON_MODE")            { if(get_str(val, s))  out.polygon_mode = s; }
    else if(k == "LINE_WIDTH")              { if(get_float(val, f)) out.line_width = f; }
    else if(k == "VERTEX_COUNT")            { if(get_uint(val, u)) out.vertex_count = u; }
    else if(k == "INSTANCE_COUNT")          { if(get_uint(val, u)) out.instance_count = u; }
    else if(k == "TOPOLOGY")                { if(get_str(val, s))  out.topology = s; }
    else if(k == "BLEND")
    {
      // Shortcut: "BLEND": true/false turns on the default alpha-blend.
      if(val.get_type() == sajson::TYPE_TRUE || val.get_type() == sajson::TYPE_FALSE)
      {
        blend_attachment a{};
        a.enable = val.get_type() == sajson::TYPE_TRUE;
        out.blend_all = a;
      }
      else if(val.get_type() == sajson::TYPE_OBJECT)
      {
        blend_attachment a{};
        a.enable = true;
        parse_blend_attachment(val, a);
        out.blend_all = a;
      }
    }
    else if(k == "BLEND_PER_ATTACHMENT")
    {
      if(val.get_type() == sajson::TYPE_ARRAY)
      {
        std::size_t m = val.get_length();
        out.blend_per_attachment.clear();
        out.blend_per_attachment.reserve(m);
        for(std::size_t j = 0; j < m; j++)
        {
          blend_attachment a{};
          a.enable = true;
          parse_blend_attachment(val.get_array_element(j), a);
          out.blend_per_attachment.push_back(a);
        }
      }
    }
    else if(k == "STENCIL_TEST")       { if(get_bool(val, b)) out.stencil_test = b; }
    else if(k == "STENCIL_READ_MASK")  { if(get_uint(val, u)) out.stencil_read_mask = u; }
    else if(k == "STENCIL_WRITE_MASK") { if(get_uint(val, u)) out.stencil_write_mask = u; }
    else if(k == "STENCIL_FRONT")
    {
      stencil_op_state st{};
      parse_stencil_op_state(val, st);
      out.stencil_front = st;
    }
    else if(k == "STENCIL_BACK")
    {
      stencil_op_state st{};
      parse_stencil_op_state(val, st);
      out.stencil_back = st;
    }
    else if(k == "SHADING_RATE")
    {
      if(val.get_type() == sajson::TYPE_ARRAY && val.get_length() >= 2)
      {
        int w{}, h{};
        if(get_int(val.get_array_element(0), w)
           && get_int(val.get_array_element(1), h)
           && w >= 1 && h >= 1)
        {
          out.shading_rate = std::array<int, 2>{w, h};
        }
      }
    }
  }
}

using root_fun = void (*)(descriptor&, const sajson::value&);
using input_fun = input (*)(const sajson::value&);
static const ossia::string_map<root_fun>& root_parse{[] {
  static ossia::string_map<root_fun> p;
  p.insert({"DESCRIPTION", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_STRING)
      d.description = v.as_string();
  }});
  p.insert({"CREDIT", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_STRING)
      d.credits = v.as_string();
  }});
  p.insert({"CATEGORIES", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto elem = v.get_array_element(i);
        if(elem.get_type() == sajson::TYPE_STRING)
          d.categories.push_back(elem.as_string());
      }
    }
  }});

  static const ossia::hash_map<std::string, input_fun>& input_parse{[] {
    static ossia::hash_map<std::string, input_fun> i;
    i.insert({"float", [](const auto& s) { return parse<float_input>(s); }});
    i.insert({"long", [](const auto& s) { return parse<long_input>(s); }});
    i.insert({"bool", [](const auto& s) { return parse<bool_input>(s); }});
    i.insert({"event", [](const auto& s) { return parse<event_input>(s); }});
    i.insert({"image", [](const auto& s) { return parse<image_input>(s); }});
    i.insert({"cubemap", [](const auto& s) { return parse<cubemap_input>(s); }});
    i.insert({"point2d", [](const auto& s) { return parse<point2d_input>(s); }});
    i.insert({"point3d", [](const auto& s) { return parse<point3d_input>(s); }});
    i.insert({"color", [](const auto& s) { return parse<color_input>(s); }});
    i.insert({"audio", [](const auto& s) { return parse<audio_input>(s); }});
    i.insert({"audiofft", [](const auto& s) { return parse<audioFFT_input>(s); }});
    i.insert(
        {"audiohistogram", [](const auto& s) { return parse<audioHist_input>(s); }});
    i.insert({"audiofloathistogram", [](const auto& s) {
      return parse<audioHist_input>(s);
    }});

    // CSF-specific types - note: 'image' in CSF context is csf_image_input, not image_input
    i.insert({"storage", [](const auto& s) { return parse<storage_input>(s); }});
    i.insert({"uniform", [](const auto& s) { return parse<uniform_input>(s); }});
    i.insert({"texture", [](const auto& s) { return parse<texture_input>(s); }});
    i.insert({"geometry", [](const auto& s) { return parse<geometry_input>(s); }});

    return i;
  }()};

  p.insert({"INPUTS", [](descriptor& d, const sajson::value& v) {
    using namespace std::literals;
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          auto k = obj.find_object_key_insensitive(sajson::literal("TYPE"));
          if(k != obj.get_length())
          {
            std::string type_str;
            if(!get_str(obj.get_object_value(k), type_str))
              continue;
            boost::algorithm::to_lower(type_str);

            // "image" with ACCESS or FORMAT → storage image (csf_image_input),
            // same as the RESOURCES section. This lets users declare storage
            // images in INPUTS without having to move them to RESOURCES.
            if(type_str == "image"
               && (obj.find_object_key_insensitive(sajson::literal("ACCESS")) != obj.get_length()
                || obj.find_object_key_insensitive(sajson::literal("FORMAT")) != obj.get_length()))
            {
              input inp;
              parse_input_base(inp, obj);
              csf_image_input ci;
              parse_input(ci, obj);
              inp.data = ci;
              d.inputs.push_back(inp);
            }
            else
            {
              auto inp = input_parse.find(type_str);
              if(inp != input_parse.end())
                d.inputs.push_back((inp->second)(obj));
            }
          }
          else
          {
            // No TYPE specified — default to storage (SSBO). Matches the
            // nested-AUXILIARY default (`aux_entry_kind`, ~L820) so the
            // top-level INPUTS dispatcher behaves the same as nested
            // declarations. This is the right default because:
            //   - The dual-bind UBO/SSBO design (scene_counts etc.) is
            //     SSBO-only after the cross-backend cleanup; readers
            //     declare `TYPE: "storage", ACCESS: "read_only"`.
            //   - Authors who omit TYPE on a buffer-shaped declaration
            //     almost always mean storage, not uniform — uniforms
            //     have a much smaller addressable subset (no runtime
            //     arrays, std140 padding) and writers always need
            //     storage anyway.
            //   - The previous behaviour silently dropped the entry
            //     without an error, so a typo'd `TYPE: "uniform"` →
            //     missing TYPE flipped scene_counts off entirely with
            //     no warning. Defaulting to storage means the next
            //     stage (binding emission) will catch the misuse via
            //     a layout/std430 check rather than a silent skip.
            d.inputs.push_back(parse<storage_input>(obj));
          }
        }
      }
    }
  }});

  // How many GLSL interface-block input/output locations a given type
  // consumes, per GLSL 4.50 spec §4.4.1 "A matrix of sizes matM or matMxN
  // takes M locations (one per column)". Non-matrix types consume one
  // location. Doubles of >dvec2 width technically consume two locations
  // each on desktop GL, but those are vanishingly rare in shader-toy-
  // style pipelines — if anyone hits the edge they can pin LOCATION
  // explicitly. The mat{M,MxN} cases matter because every existing
  // preset that wants mat4 per-instance or per-vertex would otherwise
  // have its subsequent attribute collide with column 2/3/4 of the
  // matrix.
  static constexpr auto locations_consumed = [](attribute_type t) noexcept -> int {
    using A = attribute_type;
    switch(t)
    {
      case A::Mat2:    case A::Mat2x3:  case A::Mat2x4:
      case A::DMat2:   case A::DMat2x3: case A::DMat2x4:
        return 2;
      case A::Mat3:    case A::Mat3x2:  case A::Mat3x4:
      case A::DMat3:   case A::DMat3x2: case A::DMat3x4:
        return 3;
      case A::Mat4:    case A::Mat4x2:  case A::Mat4x3:
      case A::DMat4:   case A::DMat4x2: case A::DMat4x3:
        return 4;
      default:
        return 1;
    }
  };

  static constexpr auto parse_attributes
      = []<typename T, auto member>(descriptor& d, const sajson::value& v) {
    using namespace std::literals;
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          T ip{};
          ip.location = -1;

          if(auto k = obj.find_object_key_insensitive(sajson::literal("LOCATION"));
             k != obj.get_length())
          {
            const auto& loc_obj = obj.get_object_value(k);
            if(loc_obj.get_type() == sajson::TYPE_INTEGER)
            {
              ip.location = loc_obj.get_integer_value();
            }
            else if(loc_obj.get_type() == sajson::TYPE_STRING)
            {
              // Parse as integer, e.g. "LOCATION": "3". std::stoi throws
              // std::invalid_argument (a logic_error, not runtime_error)
              // on non-numeric input — catch it locally and surface a
              // useful invalid_file message instead. The previous
              // unguarded call escaped through the parser's outer
              // catch(const std::runtime_error&) and either terminated
              // (when the parser was invoked from a noexcept context;
              // see ProcessDropHandler.cpp) or surfaced as the generic
              // "Unknown error" via the catch(...) fallback at
              // ShaderProgram.cpp.
              // FIXME parse standard locations from ossia::geometry_port
              try
              {
                ip.location = std::stoi(loc_obj.as_string());
              }
              catch(const std::exception&)
              {
                throw invalid_file{
                  std::string("LOCATION must be integer or numeric "
                              "string, got: \"")
                  + std::string(loc_obj.as_string()) + "\""};
              }
            }
          }

          if(auto k = obj.find_object_key_insensitive(sajson::literal("TYPE"));
             k != obj.get_length())
          {
            std::string type_str;
            if(get_str(obj.get_object_value(k), type_str))
            {
              boost::algorithm::to_lower(type_str);
              auto inp = attribute_type_parse.find(type_str);
              if(inp != attribute_type_parse.end())
                ip.type = inp->second;
            }
          }

          if(auto k = obj.find_object_key_insensitive(sajson::literal("NAME"));
             k != obj.get_length())
          {
            get_str(obj.get_object_value(k), ip.name);
          }

          // SEMANTIC (only meaningful on vertex_input): explicit ossia
          // attribute semantic name to use for upstream-buffer matching.
          // When omitted, name is used as the semantic key. When set to
          // "custom" the runtime falls back to NAME-based matching.
          if(auto k = obj.find_object_key_insensitive(sajson::literal("SEMANTIC"));
             k != obj.get_length())
          {
            auto val = obj.get_object_value(k);
            if(val.get_type() == sajson::TYPE_STRING)
              ip.semantic = val.as_string();
          }

          // Interpolation qualifier: "smooth" (default, not emitted), "flat",
          // "noperspective", "centroid", "sample". Applies to vertex outputs
          // and fragment inputs (no effect on vertex inputs / fragment outputs).
          if(auto k = obj.find_object_key_insensitive(sajson::literal("INTERPOLATION"));
             k != obj.get_length())
          {
            auto val = obj.get_object_value(k);
            if(val.get_type() == sajson::TYPE_STRING)
              ip.interpolation = val.as_string();
          }

          // REQUIRED / DEFAULT: only meaningful on vertex_input (raw raster
          // pipeline's strictness-vs-fallback control). Silently ignored on
          // vertex_output / fragment_input / fragment_output — their matching
          // rules are author-owned, not upstream-dependent.
          if constexpr (std::is_same_v<T, vertex_input>)
          {
            if(auto k = obj.find_object_key_insensitive(sajson::literal("REQUIRED"));
               k != obj.get_length())
            {
              const auto& rv = obj.get_object_value(k);
              if(rv.get_type() == sajson::TYPE_FALSE)
                ip.required = false;
              else if(rv.get_type() == sajson::TYPE_TRUE)
                ip.required = true;
              // Other JSON types left at default (true). No error here —
              // strict JSON typing is already enforced upstream by sajson.
            }

            if(auto k = obj.find_object_key_insensitive(sajson::literal("DEFAULT"));
               k != obj.get_length())
            {
              const auto& dv = obj.get_object_value(k);
              if(dv.get_type() == sajson::TYPE_ARRAY)
              {
                const std::size_t len = dv.get_length();
                ip.default_val.reserve(len);
                for(std::size_t j = 0; j < len; ++j)
                {
                  const auto& e = dv.get_array_element(j);
                  if(e.get_type() == sajson::TYPE_INTEGER)
                    ip.default_val.push_back((double)e.get_integer_value());
                  else if(e.get_type() == sajson::TYPE_DOUBLE)
                    ip.default_val.push_back(e.get_double_value());
                  // Non-numeric entries silently skipped — the runtime's
                  // component-pad rule will fill missing slots with zero.
                }
              }
              else if(dv.get_type() == sajson::TYPE_INTEGER)
              {
                // Allow a bare scalar for 1-wide types: "DEFAULT": 1
                ip.default_val.push_back((double)dv.get_integer_value());
              }
              else if(dv.get_type() == sajson::TYPE_DOUBLE)
              {
                ip.default_val.push_back(dv.get_double_value());
              }
            }
          }

          // If LOCATION was not specified, assign sequentially with
          // per-type location counts so mat3/mat4 and their rectangular
          // cousins claim the right number of slots (matMxN consumes M
          // consecutive locations under GLSL 4.50 §4.4.1). Previously
          // this was `(int)(d.*member).size()` — off-by-3 the moment a
          // shader declared any mat4 input, and the next attribute
          // would land inside the matrix, which the driver rejects.
          //
          // For mixed explicit / auto layouts the cumulative-sum above
          // can collide with a user-pinned LOCATION; that's a pre-existing
          // policy tradeoff left untouched here — the simpler "always
          // auto" pattern is what 99% of shipped shaders use.
          if(ip.location < 0 && !ip.name.empty())
          {
            int next_loc = 0;
            for(const auto& prev : d.*member)
              next_loc += locations_consumed(prev.type);
            ip.location = next_loc;
          }

          if(ip.type != attribute_type::Unknown && ip.location >= 0 && !ip.name.empty())
          {
            (d.*member).push_back(ip);
          }
        }
      }
    }
  };

  p.insert({"VERTEX_INPUTS", [](descriptor& d, const sajson::value& v) {
    parse_attributes.operator()<vertex_input, &descriptor::vertex_inputs>(d, v);
  }});

  p.insert({"VERTEX_OUTPUTS", [](descriptor& d, const sajson::value& v) {
    parse_attributes.operator()<vertex_output, &descriptor::vertex_outputs>(d, v);
  }});

  p.insert({"FRAGMENT_INPUTS", [](descriptor& d, const sajson::value& v) {
    parse_attributes.operator()<fragment_input, &descriptor::fragment_inputs>(d, v);
  }});

  p.insert({"FRAGMENT_OUTPUTS", [](descriptor& d, const sajson::value& v) {
    parse_attributes.operator()<fragment_output, &descriptor::fragment_outputs>(d, v);
  }});

  // Top-level AUXILIARY for RAW_RASTER_PIPELINE: SSBOs AND textures travelling
  // bundled with the upstream geometry. Buffer entries (default / TYPE:
  // "storage") land in d.auxiliary; texture entries (TYPE: "image" /
  // "texture" / "cubemap" / "image_cube") land in d.auxiliary_textures.
  p.insert({"AUXILIARY", [](descriptor& d, const sajson::value& v) {
    parse_auxiliary_array(v, d.auxiliary, d.auxiliary_textures);
  }});

  // Add RESOURCES parsing for CSF (which can contain both inputs and resources)
  p.insert({"RESOURCES", [](descriptor& d, const sajson::value& v) {
    using namespace std::literals;
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          auto k = obj.find_object_key_insensitive(sajson::literal("TYPE"));
          if(k != obj.get_length())
          {
            std::string type_str;
            if(!get_str(obj.get_object_value(k), type_str))
              continue;

            boost::algorithm::to_lower(type_str);
            // Handle special cases for CSF image types
            //   "image"      → 2D / 3D storage image (image2D / image3D)
            //   "image_cube" → writable cubemap storage image (imageCube)
            if(type_str == "image" || type_str == "image_cube")
            {
              input inp;
              parse_input_base(inp, obj);
              csf_image_input ci;
              parse_input(ci, obj);
              if(type_str == "image_cube")
                ci.cubemap = true;
              inp.data = ci;
              d.inputs.push_back(inp);
            }
            else
            {
              // Try to parse as regular input type
              auto inp_parser = input_parse.find(type_str);
              if(inp_parser != input_parse.end())
              {
                d.inputs.push_back((inp_parser->second)(obj));
              }
              else
              {
              }
            }
          }
          else
          {
          }
        }
      }
    }
  }});

  // Add DISPATCH parsing for CSF (backward compatibility)
  p.insert({"DISPATCH", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_OBJECT)
    {
      d.mode = descriptor::CSF;

      descriptor::dispatch_info dispatch;

      // Parse LOCAL_SIZE
      if(auto ls_k = v.find_object_key_insensitive(sajson::literal("LOCAL_SIZE"));
         ls_k != v.get_length())
      {
        auto ls_val = v.get_object_value(ls_k);
        if(ls_val.get_type() == sajson::TYPE_ARRAY && ls_val.get_length() >= 3)
        {
          for(int i = 0; i < 3; i++)
          {
            auto elem = ls_val.get_array_element(i);
            if(elem.get_type() == sajson::TYPE_INTEGER)
              dispatch.local_size[i] = elem.get_integer_value();
          }
        }
      }

      // Parse EXECUTION_MODEL
      if(auto em_k = v.find_object_key_insensitive(sajson::literal("EXECUTION_MODEL"));
         em_k != v.get_length())
      {
        auto em_val = v.get_object_value(em_k);
        if(em_val.get_type() == sajson::TYPE_OBJECT)
        {
          // Parse TYPE
          if(auto type_k = em_val.find_object_key_insensitive(sajson::literal("TYPE"));
             type_k != em_val.get_length())
          {
            auto type_val = em_val.get_object_value(type_k);
            if(type_val.get_type() == sajson::TYPE_STRING)
              dispatch.execution_type = type_val.as_string();
          }

          // Parse TARGET
          if(auto target_k
             = em_val.find_object_key_insensitive(sajson::literal("TARGET"));
             target_k != em_val.get_length())
          {
            auto target_val = em_val.get_object_value(target_k);
            if(target_val.get_type() == sajson::TYPE_STRING)
              dispatch.target_resource = target_val.as_string();
          }

          // Parse STRIDE (legacy, sets X only) and STRIDE_X/Y/Z (per-axis, supports formulas)
          if(auto stride_k
             = em_val.find_object_key_insensitive(sajson::literal("STRIDE"));
             stride_k != em_val.get_length())
          {
            auto stride_val = em_val.get_object_value(stride_k);
            if(stride_val.get_type() == sajson::TYPE_INTEGER)
              dispatch.stride[0] = std::to_string(stride_val.get_integer_value());
            else if(stride_val.get_type() == sajson::TYPE_STRING)
              dispatch.stride[0] = stride_val.as_string();
          }
          {
            auto parse_stride_axis = [&](const sajson::string& key, int axis) {
              if(auto sk = em_val.find_object_key_insensitive(key);
                 sk != em_val.get_length())
              {
                auto sv = em_val.get_object_value(sk);
                if(sv.get_type() == sajson::TYPE_INTEGER)
                  dispatch.stride[axis] = std::to_string(sv.get_integer_value());
                else if(sv.get_type() == sajson::TYPE_STRING)
                  dispatch.stride[axis] = sv.as_string();
              }
            };
            parse_stride_axis(sajson::literal("STRIDE_X"), 0);
            parse_stride_axis(sajson::literal("STRIDE_Y"), 1);
            parse_stride_axis(sajson::literal("STRIDE_Z"), 2);
          }

          // Parse WORKGROUPS
          if(auto wg_k
             = em_val.find_object_key_insensitive(sajson::literal("WORKGROUPS"));
             wg_k != em_val.get_length())
          {
            auto wg_val = em_val.get_object_value(wg_k);
            if(wg_val.get_type() == sajson::TYPE_ARRAY && wg_val.get_length() >= 3)
            {
              for(int i = 0; i < 3; i++)
              {
                auto elem = wg_val.get_array_element(i);
                if(elem.get_type() == sajson::TYPE_INTEGER)
                  dispatch.workgroups[i] = elem.get_integer_value();
              }
            }
          }
        }
      }

      // Add single dispatch as first pass for backward compatibility
      d.csf_passes.push_back(dispatch);
    }
  }});

  p.insert({"PASSES", [](descriptor& d, const sajson::value& v) {
    using namespace std::literals;
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          // Check if this looks like a CSF dispatch pass (has LOCAL_SIZE)
          if(auto ls_k = obj.find_object_key_insensitive(sajson::literal("LOCAL_SIZE"));
             ls_k != obj.get_length())
          {
            // This is a CSF dispatch pass
            d.mode = descriptor::CSF;

            descriptor::dispatch_info dispatch;

            // Parse LOCAL_SIZE
            auto ls_val = obj.get_object_value(ls_k);
            if(ls_val.get_type() == sajson::TYPE_ARRAY && ls_val.get_length() >= 3)
            {
              for(int idx = 0; idx < 3; idx++)
              {
                auto elem = ls_val.get_array_element(idx);
                if(elem.get_type() == sajson::TYPE_INTEGER)
                  dispatch.local_size[idx] = elem.get_integer_value();
              }
            }

            // Parse EXECUTION_MODEL
            if(auto em_k
               = obj.find_object_key_insensitive(sajson::literal("EXECUTION_MODEL"));
               em_k != obj.get_length())
            {
              auto em_val = obj.get_object_value(em_k);
              if(em_val.get_type() == sajson::TYPE_OBJECT)
              {
                // Parse TYPE
                if(auto type_k
                   = em_val.find_object_key_insensitive(sajson::literal("TYPE"));
                   type_k != em_val.get_length())
                {
                  auto type_val = em_val.get_object_value(type_k);
                  if(type_val.get_type() == sajson::TYPE_STRING)
                    dispatch.execution_type = type_val.as_string();
                }

                // Parse TARGET
                if(auto target_k
                   = em_val.find_object_key_insensitive(sajson::literal("TARGET"));
                   target_k != em_val.get_length())
                {
                  auto target_val = em_val.get_object_value(target_k);
                  if(target_val.get_type() == sajson::TYPE_STRING)
                    dispatch.target_resource = target_val.as_string();
                }

                // Parse STRIDE (legacy, sets X only) and STRIDE_X/Y/Z (per-axis, supports formulas)
                if(auto stride_k
                   = em_val.find_object_key_insensitive(sajson::literal("STRIDE"));
                   stride_k != em_val.get_length())
                {
                  auto stride_val = em_val.get_object_value(stride_k);
                  if(stride_val.get_type() == sajson::TYPE_INTEGER)
                    dispatch.stride[0] = std::to_string(stride_val.get_integer_value());
                  else if(stride_val.get_type() == sajson::TYPE_STRING)
                    dispatch.stride[0] = stride_val.as_string();
                }
                {
                  auto parse_stride_axis = [&](const sajson::string& key, int axis) {
                    if(auto sk = em_val.find_object_key_insensitive(key);
                       sk != em_val.get_length())
                    {
                      auto sv = em_val.get_object_value(sk);
                      if(sv.get_type() == sajson::TYPE_INTEGER)
                        dispatch.stride[axis] = std::to_string(sv.get_integer_value());
                      else if(sv.get_type() == sajson::TYPE_STRING)
                        dispatch.stride[axis] = sv.as_string();
                    }
                  };
                  parse_stride_axis(sajson::literal("STRIDE_X"), 0);
                  parse_stride_axis(sajson::literal("STRIDE_Y"), 1);
                  parse_stride_axis(sajson::literal("STRIDE_Z"), 2);
                }

                // Parse WORKGROUPS
                if(auto wg_k
                   = em_val.find_object_key_insensitive(sajson::literal("WORKGROUPS"));
                   wg_k != em_val.get_length())
                {
                  auto wg_val = em_val.get_object_value(wg_k);
                  if(wg_val.get_type() == sajson::TYPE_ARRAY && wg_val.get_length() >= 3)
                  {
                    for(int idx = 0; idx < 3; idx++)
                    {
                      auto elem = wg_val.get_array_element(idx);
                      if(elem.get_type() == sajson::TYPE_INTEGER)
                        dispatch.workgroups[idx] = elem.get_integer_value();
                    }
                  }
                }
              }
            }

            d.csf_passes.push_back(dispatch);
          }
          else
          {
            // This is an ISF pass
            pass p;
            if(auto target_k
               = obj.find_object_key_insensitive(sajson::literal("TARGET"));
               target_k != obj.get_length())
            {
              if(get_str(obj.get_object_value(target_k), p.target)
                 && !p.target.empty())
              {
                d.pass_targets.push_back(p.target);
              }
            }

            if(auto persistent_k
               = obj.find_object_key_insensitive(sajson::literal("PERSISTENT"));
               persistent_k != obj.get_length())
            {
              p.persistent
                  = obj.get_object_value(persistent_k).get_type() == sajson::TYPE_TRUE;
            }

            if(auto float_k = obj.find_object_key_insensitive(sajson::literal("FLOAT"));
               float_k != obj.get_length())
            {
              p.float_storage
                  = obj.get_object_value(float_k).get_type() == sajson::TYPE_TRUE;
            }

            if(auto float_k = obj.find_object_key_insensitive(sajson::literal("FILTER"));
               float_k != obj.get_length())
            {
              const auto& val = obj.get_object_value(float_k);
              p.nearest_filter = val.get_type() == sajson::TYPE_STRING
                                 && val.as_string() == std::string_view("NEAREST");
            }

            if(auto width_k = obj.find_object_key_insensitive(sajson::literal("WIDTH"));
               width_k != obj.get_length())
            {
              auto t = obj.get_object_value(width_k).get_type();
              if(t == sajson::TYPE_STRING)
              {
                p.width_expression = obj.get_object_value(width_k).as_string();
              }
              else if(t == sajson::TYPE_DOUBLE)
              {
                p.width_expression
                    = std::to_string(obj.get_object_value(width_k).get_double_value());
              }
              else if(t == sajson::TYPE_INTEGER)
              {
                p.width_expression
                    = std::to_string(obj.get_object_value(width_k).get_integer_value());
              }
            }

            if(auto height_k
               = obj.find_object_key_insensitive(sajson::literal("HEIGHT"));
               height_k != obj.get_length())
            {
              auto t = obj.get_object_value(height_k).get_type();
              if(t == sajson::TYPE_STRING)
              {
                p.height_expression = obj.get_object_value(height_k).as_string();
              }
              else if(t == sajson::TYPE_DOUBLE)
              {
                p.height_expression
                    = std::to_string(obj.get_object_value(height_k).get_double_value());
              }
              else if(t == sajson::TYPE_INTEGER)
              {
                p.height_expression
                    = std::to_string(obj.get_object_value(height_k).get_integer_value());
              }
            }

            // LAYER: render to a specific layer of a texture-array output.
            if(auto layer_k
               = obj.find_object_key_insensitive(sajson::literal("LAYER"));
               layer_k != obj.get_length())
            {
              int lyr{};
              if(get_int(obj.get_object_value(layer_k), lyr))
                p.layer = lyr;
            }

            // Z: render to a specific Z-slice of a 3D target. Stored as an
            // expression so it can reference $USER or input sizes; resolved
            // at render time.
            if(auto z_k = obj.find_object_key_insensitive(sajson::literal("Z"));
               z_k != obj.get_length())
            {
              auto t = obj.get_object_value(z_k).get_type();
              if(t == sajson::TYPE_STRING)
                p.z_expression = obj.get_object_value(z_k).as_string();
              else if(t == sajson::TYPE_INTEGER)
                p.z_expression
                    = std::to_string(obj.get_object_value(z_k).get_integer_value());
              else if(t == sajson::TYPE_DOUBLE)
                p.z_expression
                    = std::to_string((int)obj.get_object_value(z_k).get_double_value());
            }

            // FORMAT: override the intermediate-render-target format for
            // this pass only. Useful for separable-filter chains where one
            // intermediate wants extra precision (rgba16f) but the final
            // output is RGBA8.
            if(auto fmt_k
               = obj.find_object_key_insensitive(sajson::literal("FORMAT"));
               fmt_k != obj.get_length())
            {
              auto v2 = obj.get_object_value(fmt_k);
              if(v2.get_type() == sajson::TYPE_STRING)
                p.format = v2.as_string();
            }

            // PIPELINE_STATE: per-pass pipeline state overrides.
            if(auto ps_k
               = obj.find_object_key_insensitive(sajson::literal("PIPELINE_STATE"));
               ps_k != obj.get_length())
            {
              parse_pipeline_state(obj.get_object_value(ps_k), p.override_state);
            }

            d.passes.push_back(std::move(p));
          }
        }
      }
    }
  }});

  p.insert({"OUTPUTS", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          output_declaration out;

          if(auto name_k = obj.find_object_key_insensitive(sajson::literal("NAME"));
             name_k != obj.get_length())
          {
            get_str(obj.get_object_value(name_k), out.name);
          }

          if(auto type_k = obj.find_object_key_insensitive(sajson::literal("TYPE"));
             type_k != obj.get_length())
          {
            get_str(obj.get_object_value(type_k), out.type);
          }

          // Default type to "color" if not specified
          if(out.type.empty())
            out.type = "color";

          // LAYERS: >1 allocates a texture array with this many layers.
          if(auto layers_k = obj.find_object_key_insensitive(sajson::literal("LAYERS"));
             layers_k != obj.get_length())
          {
            int l{};
            if(get_int(obj.get_object_value(layers_k), l) && l > 0)
              out.layers = l;
          }

          // DEPTH: >1 allocates a 3D texture with this depth. Passes targeting
          // this output can specify Z to write into a specific slice.
          if(auto depth_k = obj.find_object_key_insensitive(sajson::literal("DEPTH"));
             depth_k != obj.get_length())
          {
            int d_val{};
            if(get_int(obj.get_object_value(depth_k), d_val) && d_val > 0)
              out.depth = d_val;
          }

          // FORMAT: optional explicit texture format (e.g. "rgba16f", "r32f", "d32f").
          if(auto fmt_k = obj.find_object_key_insensitive(sajson::literal("FORMAT"));
             fmt_k != obj.get_length())
          {
            auto v2 = obj.get_object_value(fmt_k);
            if(v2.get_type() == sajson::TYPE_STRING)
              out.format = v2.as_string();
          }

          // SAMPLES: MSAA sample count (1, 2, 4, 8, 16, ...).
          if(auto s_k = obj.find_object_key_insensitive(sajson::literal("SAMPLES"));
             s_k != obj.get_length())
          {
            int s{};
            if(get_int(obj.get_object_value(s_k), s) && s >= 1)
              out.samples = s;
          }

          // CUBEMAP: when true the layered output is allocated as a cubemap
          // (six faces sampled via samplerCube downstream) rather than a
          // plain 2D array. Combines with `LAYERS: 6` + `MULTIVIEW: 6` for
          // the IBL precompute case (one draw writes all six faces of the
          // target cube). Consumer shaders declare a matching
          // `TYPE: "cubemap"` INPUT to read it.
          if(auto cube_k = obj.find_object_key_insensitive(sajson::literal("CUBEMAP"));
             cube_k != obj.get_length())
          {
            auto v2 = obj.get_object_value(cube_k);
            if(v2.get_type() == sajson::TYPE_TRUE)
              out.is_cubemap = true;
            else if(v2.get_type() == sajson::TYPE_INTEGER)
              out.is_cubemap = (v2.get_integer_value() != 0);
          }

          // GENERATE_MIPS: post-pass mip-chain auto-fill. Implies the
          // MipMapped + UsedWithGenerateMips allocator flags. Runtime
          // issues a QRhiResourceUpdateBatch::generateMips after the
          // render loop (and after any CUBEMAP+MULTIVIEW cube-copy).
          if(auto gm_k = obj.find_object_key_insensitive(sajson::literal("GENERATE_MIPS"));
             gm_k != obj.get_length())
          {
            auto v2 = obj.get_object_value(gm_k);
            if(v2.get_type() == sajson::TYPE_TRUE)
              out.generate_mips = true;
            else if(v2.get_type() == sajson::TYPE_INTEGER)
              out.generate_mips = (v2.get_integer_value() != 0);
          }

          // WIDTH / HEIGHT: explicit offscreen target size. Integer
          // literal (fast path) or string expression (evaluated at
          // init time against input-image sizes / scalar ports,
          // mirroring CSF dispatch-expression semantics). Zero /
          // unset → fall back to renderer.state.renderSize.
          if(auto w_k = obj.find_object_key_insensitive(sajson::literal("WIDTH"));
             w_k != obj.get_length())
          {
            auto v2 = obj.get_object_value(w_k);
            if(v2.get_type() == sajson::TYPE_INTEGER)
              out.width = v2.get_integer_value();
            else if(v2.get_type() == sajson::TYPE_DOUBLE)
              out.width = (int)v2.get_double_value();
            else if(v2.get_type() == sajson::TYPE_STRING)
              out.width_expression = v2.as_string();
          }
          if(auto h_k = obj.find_object_key_insensitive(sajson::literal("HEIGHT"));
             h_k != obj.get_length())
          {
            auto v2 = obj.get_object_value(h_k);
            if(v2.get_type() == sajson::TYPE_INTEGER)
              out.height = v2.get_integer_value();
            else if(v2.get_type() == sajson::TYPE_DOUBLE)
              out.height = (int)v2.get_double_value();
            else if(v2.get_type() == sajson::TYPE_STRING)
              out.height_expression = v2.as_string();
          }

          d.outputs.push_back(std::move(out));
        }
      }
    }
  }});

  p.insert({"PIPELINE_STATE", [](descriptor& d, const sajson::value& v) {
    parse_pipeline_state(v, d.default_state);
  }});

  p.insert({"MULTIVIEW", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_INTEGER)
      d.multiview_count = v.get_integer_value();
    else if(v.get_type() == sajson::TYPE_DOUBLE)
      d.multiview_count = (int)v.get_double_value();
    else if(v.get_type() == sajson::TYPE_TRUE)
      d.multiview_count = 2; // "MULTIVIEW": true => 2 views by default
  }});

  // EXECUTION_MODEL (top-level, RAW_RASTER_PIPELINE). Shape:
  //   "EXECUTION_MODEL": {
  //     "TYPE":   "SINGLE" | "PER_MIP" | "PER_CUBE_FACE" | "PER_LAYER" | "MANUAL",
  //     "TARGET": "<output name>",    // PER_MIP / PER_CUBE_FACE / PER_LAYER
  //     "COUNT":  "<expression>"      // MANUAL (int literal accepted too)
  //   }
  // Distinct from the per-pass EXECUTION_MODEL inside DISPATCH / PASSES
  // (CSF compute), which lives in `dispatch_info::execution_type`.
  p.insert({"EXECUTION_MODEL", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() != sajson::TYPE_OBJECT)
      return;
    if(auto type_k
       = v.find_object_key_insensitive(sajson::literal("TYPE"));
       type_k != v.get_length())
    {
      auto tv = v.get_object_value(type_k);
      if(tv.get_type() == sajson::TYPE_STRING)
        d.execution_model.type = tv.as_string();
    }
    if(auto target_k
       = v.find_object_key_insensitive(sajson::literal("TARGET"));
       target_k != v.get_length())
    {
      auto tv = v.get_object_value(target_k);
      if(tv.get_type() == sajson::TYPE_STRING)
        d.execution_model.target = tv.as_string();
    }
    if(auto count_k
       = v.find_object_key_insensitive(sajson::literal("COUNT"));
       count_k != v.get_length())
    {
      auto tv = v.get_object_value(count_k);
      if(tv.get_type() == sajson::TYPE_STRING)
        d.execution_model.count_expression = tv.as_string();
      else if(tv.get_type() == sajson::TYPE_INTEGER)
        d.execution_model.count_expression
            = std::to_string(tv.get_integer_value());
    }
  }});

  p.insert({"CLIP_DISTANCES", [](descriptor& d, const sajson::value& v) {
    int n{};
    if(get_int(v, n) && n > 0 && n <= 8)
      d.clip_distances = n;
  }});

  p.insert({"CULL_DISTANCES", [](descriptor& d, const sajson::value& v) {
    int n{};
    if(get_int(v, n) && n > 0 && n <= 8)
      d.cull_distances = n;
  }});

  p.insert({"DEPTH_LAYOUT", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_STRING)
      d.depth_layout = v.as_string();
  }});

  p.insert({"EXTENSIONS", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() != sajson::TYPE_ARRAY)
      return;
    std::size_t n = v.get_length();
    d.extensions.reserve(d.extensions.size() + n);
    for(std::size_t i = 0; i < n; i++)
    {
      auto e = v.get_array_element(i);
      if(e.get_type() == sajson::TYPE_STRING)
        d.extensions.emplace_back(e.as_string());
    }
  }});

  p.insert({"POINT_COUNT", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_INTEGER)
      d.point_count = v.get_integer_value();
  }});

  p.insert({"PRIMITIVE_MODE", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_STRING)
      d.primitive_mode = v.as_string();
  }});

  p.insert({"LINE_SIZE", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_STRING)
      d.line_size = v.as_string();
  }});

  p.insert({"BACKGROUND_COLOR", [](descriptor& d, const sajson::value& v) {
    if(v.get_type() == sajson::TYPE_ARRAY && v.get_length() == 4)
    {
      if(auto e = v.get_array_element(0);
         e.get_type() == sajson::TYPE_DOUBLE || e.get_type() == sajson::TYPE_INTEGER)
        d.background_color[0] = e.get_number_value();
      if(auto e = v.get_array_element(1);
         e.get_type() == sajson::TYPE_DOUBLE || e.get_type() == sajson::TYPE_INTEGER)
        d.background_color[1] = e.get_number_value();
      if(auto e = v.get_array_element(2);
         e.get_type() == sajson::TYPE_DOUBLE || e.get_type() == sajson::TYPE_INTEGER)
        d.background_color[2] = e.get_number_value();
      if(auto e = v.get_array_element(3);
         e.get_type() == sajson::TYPE_DOUBLE || e.get_type() == sajson::TYPE_INTEGER)
        d.background_color[3] = e.get_number_value();
    }
  }});

  p.insert({"TYPES", [](descriptor& d, const sajson::value& v) {
    using namespace std::literals;
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          descriptor::type_definition type_def;

          // Parse NAME field
          auto name_key = obj.find_object_key_insensitive(sajson::literal("NAME"));
          if(name_key != obj.get_length())
          {
            get_str(obj.get_object_value(name_key), type_def.name);
          }

          // Parse LAYOUT field
          auto layout_key = obj.find_object_key_insensitive(sajson::literal("LAYOUT"));
          if(layout_key != obj.get_length())
          {
            auto layout_array = obj.get_object_value(layout_key);
            if(layout_array.get_type() == sajson::TYPE_ARRAY)
            {
              std::size_t layout_count = layout_array.get_length();
              for(std::size_t j = 0; j < layout_count; j++)
              {
                auto field_obj = layout_array.get_array_element(j);
                if(field_obj.get_type() == sajson::TYPE_OBJECT)
                {
                  storage_input::layout_field field;

                  // Parse field NAME
                  auto field_name_key
                      = field_obj.find_object_key_insensitive(sajson::literal("NAME"));
                  if(field_name_key != field_obj.get_length())
                  {
                    get_str(field_obj.get_object_value(field_name_key), field.name);
                  }

                  // Parse field TYPE
                  auto field_type_key
                      = field_obj.find_object_key_insensitive(sajson::literal("TYPE"));
                  if(field_type_key != field_obj.get_length())
                  {
                    get_str(field_obj.get_object_value(field_type_key), field.type);
                  }

                  type_def.layout.push_back(field);
                }
              }
            }
          }

          d.types.push_back(type_def);
        }
      }
    }
  }});

  return p;
}()};

// A non-empty compare op different from "never" turns the sampler into a
// shadow/comparison sampler. Mirrors QRhiSampler::CompareOp interpretation.
static bool isf_is_comparison_sampler(const sampler_config& s)
{
  if(s.compare.empty())
    return false;
  std::string c = s.compare;
  for(auto& ch : c) ch = (char)tolower(ch);
  return c != "never";
}


struct create_val_visitor_450
{
  struct return_type
  {
    std::string text;
    bool sampler;
  };
  return_type operator()(const float_input&) { return {"float", false}; }
  return_type operator()(const long_input&) { return {"int", false}; }
  return_type operator()(const event_input&) { return {"bool", false}; }
  return_type operator()(const bool_input&) { return {"bool", false}; }
  return_type operator()(const point2d_input&) { return {"vec2", false}; }
  return_type operator()(const point3d_input&) { return {"vec3", false}; }
  return_type operator()(const color_input&) { return {"vec4", false}; }
  return_type operator()(const image_input& i)
  {
    const bool cmp = isf_is_comparison_sampler(i.sampler);
    if(i.dimensions == 3)
      return {"uniform sampler3D", true}; // 3D shadow samplers not commonly used
    if(i.is_array)
      return {cmp ? "uniform sampler2DArrayShadow" : "uniform sampler2DArray", true};
    return {cmp ? "uniform sampler2DShadow" : "uniform sampler2D", true};
  }
  return_type operator()(const cubemap_input& c)
  {
    return {isf_is_comparison_sampler(c.sampler) ? "uniform samplerCubeShadow"
                                                 : "uniform samplerCube",
            true};
  }
  return_type operator()(const audio_input&) { return {"uniform sampler2D", true}; }
  return_type operator()(const audioFFT_input&) { return {"uniform sampler2D", true}; }
  return_type operator()(const audioHist_input&) { return {"uniform sampler2D", true}; }
  return_type operator()(const storage_input&) { return {"buffer", true}; }
  return_type operator()(const uniform_input&) { return {"uniform", true}; }
  return_type operator()(const texture_input& i)
  {
    const bool cmp = isf_is_comparison_sampler(i.sampler);
    if(i.dimensions == 3)
      return {"uniform sampler3D", true};
    return {cmp ? "uniform sampler2DShadow" : "uniform sampler2D", true};
  }
  return_type operator()(const csf_image_input& i)
  {
    if(i.isCube())
      return {"uniform imageCube", true};
    if(i.is3D())
      return {"uniform image3D", true};
    if(i.is_array)
      return {"uniform image2DArray", true};
    return {"uniform image2D", true};
  }
  return_type operator()(const geometry_input&) { return {"buffer", true}; }
};

std::pair<int, descriptor> parser::parse_isf_header(std::string_view source)
{
  using namespace std::literals;
  auto start = source.find("/*");
  if(start == std::string::npos)
    throw invalid_file{"Missing start comment"};
  auto end = source.find("*/", start);
  if(end == std::string::npos)
    throw invalid_file{"Unfinished comment"};

  // First comes the json part
  auto doc = sajson::parse(
      sajson::dynamic_allocation(),
      sajson::mutable_string_view(
          (end - start - 2), const_cast<char*>(source.data()) + start + 2));
  if(!doc.is_valid())
  {
    std::stringstream err;
    err << "JSON error: '" << doc.get_error_message() << "' at L" << doc.get_error_line()
        << ":" << doc.get_error_column();
    throw invalid_file{err.str()};
  }

  // Read the parameters
  auto root = doc.get_root();
  if(root.get_type() != sajson::TYPE_OBJECT)
    throw invalid_file{"Not a JSON object"};

  descriptor d;
  for(std::size_t i = 0; i < root.get_length(); i++)
  {
    std::string key = root.get_object_key(i).as_string();
    for(char& c : key)
      c = toupper(c);

    auto it = root_parse.find(key);
    if(it != root_parse.end())
    {
      (it->second)(d, root.get_object_value(i));
    }
  }
  return {end, std::move(d)};
}

static ossia::flat_set<std::string>
extract_glsl_function_definitions(std::string_view str)
{
  struct glsl_parse_context context;

  glsl_parse_context_init(&context);

  // :upside-down-face:
  ossia::flat_set<std::string> defs;
  // FIXME: this is leaky due to some strdup calls.
  // Calling glsl_parse_dealloc(&context); crashes.
  bool error = glsl_parse_string(&context, str.data());
  if(!error && context.root
     && context.root->code == (int)glsl_tokentype::TRANSLATION_UNIT)
  {
    for(int trans_i = 0; trans_i < context.root->child_count; trans_i++)
    {
      if(context.root->children[trans_i]->code
         == (int)glsl_tokentype::FUNCTION_DEFINITION)
      {
        auto fdef = context.root->children[trans_i];
        for(int fdef_i = 0; fdef_i < fdef->child_count; fdef_i++)
        {
          if(fdef->children[fdef_i]->code == (int)glsl_tokentype::FUNCTION_DECLARATION)
          {
            auto fdecl = fdef->children[fdef_i];
            for(int fdecl_i = 0; fdecl_i < fdecl->child_count; fdecl_i++)
            {
              if(fdecl->children[fdecl_i]->code == (int)glsl_tokentype::FUNCTION_HEADER)
              {
                auto fhead = fdecl->children[fdecl_i];
                for(int fhead_i = 0; fhead_i < fhead->child_count; fhead_i++)
                {
                  if(fhead->children[fhead_i]->code == (int)glsl_tokentype::IDENTIFIER)
                  {
                    auto identifier = fhead->children[fhead_i];
                    defs.insert(identifier->data.str);
                    break;
                  }
                }
                break;
              }
            }
            break;
          }
        }
      }
    }
  }
  glsl_parse_context_destroy(&context);

  return defs;
}

void parser::parse_geometry_filter()
{
  using namespace std::literals;

  auto [end, desc] = parse_isf_header(m_source_geometry_filter);
  m_desc = std::move(desc);

  std::string& geomWithoutISF = m_source_geometry_filter;
  geomWithoutISF.erase(0, end + 2);

  // There is always one pass at least
  if(m_desc.passes.empty())
  {
    m_desc.passes.push_back(isf::pass{});
  }

  boost::algorithm::trim(geomWithoutISF);
  auto funcs = extract_glsl_function_definitions("\n" + geomWithoutISF + "\n");
  // Technically not necessary but will save us in case there's
  // a parser bug in glsl_parser.c (of which there are many) and we just
  // want to have *one* geometry filter
  funcs.insert("process_vertex");

  boost::algorithm::replace_all(geomWithoutISF, "this_filter", "filter_%node%");

  for(auto& func : funcs)
    boost::algorithm::replace_all(geomWithoutISF, func, func + "_%node%");

  std::string filter_ubo;
  if(!m_desc.inputs.empty())
  {
    std::string globalvars;
    filter_ubo += "layout(std140, binding = %next%) uniform filter_%node%_t {\n";
    for(const isf::input& val : m_desc.inputs)
    {
      auto [type, isSampler] = ossia::visit(create_val_visitor_450{}, val.data);

      {
        filter_ubo += "  ";
        filter_ubo += type;
        filter_ubo += ' ';
        filter_ubo += val.name;
        filter_ubo += ";\n";

        // // See comment above regarding little dance to make spirv-cross happy
        // globalvars += type;
        // globalvars += ' ';
        // globalvars += val.name;
        // globalvars += " = filter_%node%.";
        // globalvars += val.name;
        // globalvars += ";\n";
      }
    }

    filter_ubo += "} filter_%node%;\n";
    filter_ubo += "\n";
    // filter_ubo += globalvars;
    // filter_ubo += "\n";
  }
  m_geometry_filter = filter_ubo + geomWithoutISF + "\n";
}

// --- GLSL helpers for graphics-visible storage resources ----------------
//
// Derive GLSL image/sampler prefix from a format string.
// Unsigned integer formats (R32UI, RGBA16UI, ...) → "u"
// Signed integer formats (R32I, RGBA16I, ...) → "i"
// Float/unorm formats (R32F, RGBA8, ...) → ""
static std::string isf_glsl_type_prefix(const std::string& format)
{
  if(format.empty())
    return "";
  std::string fmt = format;
  for(auto& c : fmt) c = (char)toupper(c);
  if(fmt.find("UI") != std::string::npos)
    return "u";
  if(fmt.size() >= 2 && fmt.back() == 'I' && fmt[fmt.size() - 2] != 'U')
    return "i";
  return "";
}

// Returns true when the visibility string indicates this resource should be
// declared in a graphics pipeline (vertex or fragment stage).
static bool is_graphics_visibility(std::string_view vis)
{
  return vis == "fragment" || vis == "vertex" || vis == "vertex+fragment"
         || vis == "both" || vis == "graphics";
}

// Emit GLSL `struct <name> { <fields...> };` declarations from the TYPES
// section. Must be injected BEFORE any SSBO/UBO body that references the
// struct, in BOTH vertex and fragment stages — otherwise scene shaders that
// declare e.g. `Light` and use `readonly buffer { Light entries[]; }` fail
// VS compilation when the SSBO leaks into a vertex pipeline that never
// included the struct (the fragment-only TYPES emission was the long-standing
// bug here). The compute path has its own copy of this logic at
// parse_compute_shader; this helper is shared by parse_isf and
// parse_raw_raster_pipeline.
static std::string isf_emit_types_struct(const std::vector<descriptor::type_definition>& types)
{
  if(types.empty())
    return {};

  std::string out;
  out += "// Struct definitions from TYPES section\n";
  for(const auto& type_def : types)
  {
    out += "struct " + type_def.name + " {\n";
    for(const auto& field : type_def.layout)
    {
      auto bracket = field.type.find('[');
      if(bracket != std::string::npos)
        out += "    " + field.type.substr(0, bracket) + " " + field.name
               + field.type.substr(bracket) + ";\n";
      else
        out += "    " + field.type + " " + field.name + ";\n";
    }
    out += "};\n\n";
  }
  return out;
}

static std::string isf_emit_ssbo_decl(
    int binding, std::string_view name, const storage_input& s, bool alias_prev)
{
  std::string out;
  out += "layout(binding = ";
  out += std::to_string(binding);
  out += ", std430) ";
  if(alias_prev || s.access == "read_only")
    out += "readonly ";
  else if(s.access == "write_only")
    out += "writeonly ";
  else
    out += "restrict ";
  out += "buffer ";
  out += name;
  out += "_buf {\n";
  for(const auto& field : s.layout)
  {
    auto bracket = field.type.find('[');
    if(bracket != std::string::npos)
      out += "    " + field.type.substr(0, bracket) + " " + field.name
             + field.type.substr(bracket) + ";\n";
    else
      out += "    " + field.type + " " + field.name + ";\n";
  }
  out += "} ";
  out += name;
  out += ";\n\n";
  return out;
}

static std::string isf_emit_ubo_decl(
    int binding, std::string_view name, const uniform_input& u)
{
  std::string out;
  out += "layout(binding = ";
  out += std::to_string(binding);
  out += ", std140) uniform ";
  out += name;
  out += "_t {\n";
  for(const auto& field : u.layout)
  {
    auto bracket = field.type.find('[');
    if(bracket != std::string::npos)
      out += "    " + field.type.substr(0, bracket) + " " + field.name
             + field.type.substr(bracket) + ";\n";
    else
      out += "    " + field.type + " " + field.name + ";\n";
  }
  out += "} ";
  out += name;
  out += ";\n\n";
  return out;
}

static std::string isf_emit_image_decl(
    int binding, std::string_view name, const csf_image_input& img,
    bool alias_prev = false)
{
  std::string out;
  out += "layout(binding = ";
  out += std::to_string(binding);
  std::string fmt = img.format.empty() ? "rgba8" : img.format;
  boost::algorithm::to_lower(fmt);
  out += ", ";
  out += fmt;
  out += ") ";
  if(alias_prev || img.access == "read_only")
    out += "readonly ";
  else if(img.access == "write_only")
    out += "writeonly ";
  else
    out += "restrict ";
  auto prefix = isf_glsl_type_prefix(img.format);
  out += "uniform ";
  out += prefix;
  // Shape dispatch must mirror the compute-stage emit at isf_emit_compute_-
  // image_decl below: parser admits CUBEMAP / IS_ARRAY / 3D shapes; the
  // bound texture's QRhi flags must agree with the GLSL declaration.
  // Cube and array variants on graphics-stage csf_image_input were
  // previously emitted as flat image2D, mismatching the cube/array texture
  // bound by IsfBindingsBuilder's allocator and triggering Vulkan
  // VUID-VkGraphicsPipelineCreateInfo-layout-07990.
  // Priority: cubemap > 3D > array > 2D (matches the parser's own reject
  // table at isf.cpp:1446-1463 which forbids cube+array and array+3D).
  const char* shape = "image2D ";
  if(img.isCube())      shape = "imageCube ";
  else if(img.is3D())   shape = "image3D ";
  else if(img.is_array) shape = "image2DArray ";
  out += shape;
  out += name;
  out += ";\n";
  return out;
}

// Emit declarations for storage_input / csf_image_input inputs for a graphics
// shader (ISF or RawRaster). Starts at `binding`, returns the next free binding.
// Also emits `name_prev` readonly declarations for persistent SSBOs.
static int isf_emit_graphics_storage(
    std::string& out, int binding, const std::vector<input>& inputs)
{
  for(const auto& inp : inputs)
  {
    if(auto* s = ossia::get_if<storage_input>(&inp.data))
    {
      if(!is_graphics_visibility(s->visibility))
        continue;
      // Indirect-draw buffers don't need shader visibility.
      if(!s->buffer_usage.empty())
        continue;
      out += isf_emit_ssbo_decl(binding, inp.name, *s, /*alias_prev=*/false);
      binding++;
      if(s->persistent)
      {
        out += isf_emit_ssbo_decl(
            binding, inp.name + "_prev", *s, /*alias_prev=*/true);
        binding++;
      }
    }
    else if(auto* img = ossia::get_if<csf_image_input>(&inp.data))
    {
      if(!is_graphics_visibility(img->visibility))
        continue;
      out += isf_emit_image_decl(binding, inp.name, *img, /*alias_prev=*/false);
      binding++;
      if(img->persistent)
      {
        out += isf_emit_image_decl(
            binding, inp.name + "_prev", *img, /*alias_prev=*/true);
        binding++;
      }
    }
    else if(auto* u = ossia::get_if<uniform_input>(&inp.data))
    {
      if(!is_graphics_visibility(u->visibility))
        continue;
      out += isf_emit_ubo_decl(binding, inp.name, *u);
      binding++;
    }
  }
  return binding;
}

// The #extension pragma must come BEFORE any declarations — emit it separately
// so it can be prepended right after #version.
static std::string isf_emit_multiview_extension(int view_count)
{
  std::string out;
  out += "#extension GL_EXT_multiview : require\n";
  out += "#define VIEW_INDEX gl_ViewIndex\n";
  out += "#define NUM_VIEWS ";
  out += std::to_string(view_count);
  out += "\n";
  return out;
}

// User-declared EXTENSIONS from the descriptor. Emitted alongside the
// multiview extension, each as `#extension <name> : require`. Advanced
// effects (subgroup ops, atomic floats, ray queries, …) go through here.
static std::string isf_emit_user_extensions(const std::vector<std::string>& exts)
{
  std::string out;
  for(const auto& e : exts)
  {
    if(e.empty())
      continue;
    out += "#extension ";
    out += e;
    out += " : require\n";
  }
  return out;
}

// Emit the multiview view-projection UBO.
static std::string isf_emit_multiview_ubo(int binding, int view_count)
{
  std::string out;
  out += "layout(std140, binding = ";
  out += std::to_string(binding);
  out += ") uniform multiview_t { mat4 viewProjection[";
  out += std::to_string(view_count);
  out += "]; } isf_mv;\n";
  return out;
}

void parser::parse_isf()
{
  using namespace std::literals;

  auto [end, desc] = parse_isf_header(m_sourceFragment);
  m_desc = std::move(desc);

  std::string& fragWithoutISF = m_sourceFragment;
  fragWithoutISF.erase(0, end + 2);

  boost::replace_all(fragWithoutISF, "gl_FragCoord", "isf_FragCoord");

  // There is always one pass at least
  if(m_desc.passes.empty())
  {
    m_desc.passes.push_back(isf::pass{});
  }

  // Fragment-mode ISF cannot drive PASSES that target a 3D / Z-sliced
  // OUTPUT: that requires per-Z-slice color attachments / 3D image
  // storage plumbing through the pass-target allocator and the
  // beginPass site, which the RenderedISFNode renderer does not yet
  // wire end-to-end. Authors should use a CSF compute shader
  // (EXECUTION_MODEL: 3D_IMAGE) for true volumetric writes; refusing
  // to load here is loud and prevents a silent 2D downgrade that
  // would make every imageStore / fragment write target the wrong
  // memory.
  for(const auto& pass : m_desc.passes)
  {
    bool target_is_3d = false;
    for(const auto& out : m_desc.outputs)
    {
      if(out.name == pass.target && out.depth > 1)
      {
        target_is_3d = true;
        break;
      }
    }
    if(!pass.z_expression.empty() || target_is_3d)
    {
      throw invalid_file{
          "fragment-mode ISF with PASSES targeting Z / 3D OUTPUTS is not "
          "yet supported in this engine — use CSF compute "
          "(EXECUTION_MODEL: 3D_IMAGE) for volumetric writes."};
    }
  }

  auto& d = m_desc;

  // We start from empty strings.

  bool simpleVS = false;
  m_vertex.clear();
  m_fragment.clear();

  // Create the GLSL prelude
  switch(m_version)
  {
    case 450: {
      // Extensions pragma block — must come right after #version, before
      // any layout/uniform/in/out declarations.
      std::string extensions_prelude;
      if(d.multiview_count >= 2)
        extensions_prelude += isf_emit_multiview_extension(d.multiview_count);
      extensions_prelude += isf_emit_user_extensions(d.extensions);

      // Setup vertex shader
      {
        m_vertex = GLSL45.versionPrelude;
        m_vertex += extensions_prelude;

        if(m_sourceVertex.empty())
        {
          m_vertex += GLSL45.vertexPrelude;
          simpleVS = true;
        }
        else if(m_sourceVertex.find("isf_vertShaderInit()") != std::string::npos)
        {
          m_vertex += GLSL45.vertexPrelude;
        }
      }

      {
        // Setup fragment shader
        m_fragment = GLSL45.versionPrelude;
        m_fragment += extensions_prelude;

        // LAYER_INDEX for layered / multi-layer outputs: the vertex shader writes
        // to gl_Layer and the fragment shader receives it via a flat varying.
        bool has_layered_output = (d.multiview_count >= 2);
        for(const auto& out : d.outputs)
          if(out.layers > 1)
            has_layered_output = true;
        if(has_layered_output)
        {
          m_fragment += "#define LAYER_INDEX gl_Layer\n";
        }

        if(d.outputs.empty())
        {
          m_fragment += GLSL45.fragmentPrelude;
        }
        else
        {
          // MRT: generate per-output declarations
          m_fragment += "\nlayout(location = 0) in vec2 isf_FragNormCoord;\n";
          int color_location = 0;
          for(const auto& out : d.outputs)
          {
            if(out.type == "depth")
            {
              // depth is written via gl_FragDepth; provide a #define alias
              m_fragment += "#define ";
              m_fragment += out.name;
              m_fragment += " gl_FragDepth\n";
            }
            else
            {
              m_fragment += "layout(location = ";
              m_fragment += std::to_string(color_location);
              m_fragment += ") out vec4 ";
              m_fragment += out.name;
              m_fragment += ";\n";
              // Alias the first color output as isf_FragColor for backward compat
              if(color_location == 0)
              {
                m_fragment += "#define isf_FragColor ";
                m_fragment += out.name;
                m_fragment += "\n";
              }
              color_location++;
            }
          }
        }

        // Conservative-depth qualifier on gl_FragDepth (ISF path).
        if(!d.depth_layout.empty())
        {
          std::string dl = d.depth_layout;
          for(auto& c : dl) c = (char)tolower(c);
          const char* q = nullptr;
          if(dl == "greater")        q = "depth_greater";
          else if(dl == "less")      q = "depth_less";
          else if(dl == "unchanged") q = "depth_unchanged";
          else if(dl == "any")       q = "depth_any";
          if(q)
          {
            m_fragment += "layout(";
            m_fragment += q;
            m_fragment += ") out float gl_FragDepth;\n";
          }
        }
      }

      // Setup the parameters UBOs
      std::string material_ubos = GLSL45.defaultUniforms;

      // TYPES section structs must be visible in BOTH stages because SSBO
      // declarations referencing them (e.g. `Light entries[]`) are appended
      // to material_ubos, which is in turn injected into both VS and FS.
      material_ubos += isf_emit_types_struct(d.types);

      int sampler_binding = 3;

      if(!d.inputs.empty() || !d.pass_targets.empty())
      {
        std::string uniforms;
        std::string samplers;
        std::string globalvars;
        int num_uniform = 0;
        uniforms += "layout(std140, binding = 2) uniform material_t {\n";
        for(const isf::input& val : d.inputs)
        {
          // Storage buffers / storage images are declared separately after
          // samplers — skip them here to avoid emitting invalid GLSL.
          if(ossia::get_if<isf::storage_input>(&val.data)
             || ossia::get_if<isf::csf_image_input>(&val.data)
             || ossia::get_if<isf::geometry_input>(&val.data)
             || ossia::get_if<isf::uniform_input>(&val.data))
            continue;

          auto [type, isSampler] = ossia::visit(create_val_visitor_450{}, val.data);

          if(isSampler)
          {
            samplers += "layout(binding = ";
            samplers += std::to_string(sampler_binding);
            samplers += ") ";
            samplers += type;
            samplers += ' ';
            samplers += val.name;
            samplers += ";\n";

            sampler_binding++;

            if(auto* img = ossia::get_if<isf::image_input>(&val.data))
            {
              if(img->depth)
              {
                samplers += "layout(binding = ";
                samplers += std::to_string(sampler_binding);
                samplers += ") uniform sampler2D ";
                samplers += val.name;
                samplers += "_depth;\n";
                sampler_binding++;
              }
            }
            else if(auto* cube = ossia::get_if<isf::cubemap_input>(&val.data))
            {
              if(cube->depth)
              {
                samplers += "layout(binding = ";
                samplers += std::to_string(sampler_binding);
                samplers += ") uniform samplerCube ";
                samplers += val.name;
                samplers += "_depth;\n";
                sampler_binding++;
              }
            }
          }
          else
          {
            num_uniform++;
            uniforms += type;
            uniforms += ' ';
            uniforms += val.name;
            uniforms += ";\n";

            // See comment above regarding little dance to make spirv-cross happy
            globalvars += type;
            globalvars += ' ';
            globalvars += val.name;
            globalvars += " = isf_material_uniforms.";
            globalvars += val.name;
            globalvars += ";\n";
          }
        }

        // Pass targets are bound as sampler2D for cross-pass reads. Two
        // independent dedup checks:
        //   1) the same TARGET can appear in multiple PASSES entries (e.g.
        //      LAYERS where each layer is a pass writing to the same target)
        //      — we must only emit one sampler per distinct name.
        //   2) a TARGET may also appear as a FRAGMENT_OUTPUT for the current
        //      pass (typical for OUTPUTS with LAYERS) — those collide with
        //      the `out vec4 <name>;` declaration emitted above and would
        //      cause "redefinition" at GLSL compile time.
        std::set<std::string> output_names;
        for(const auto& out : d.outputs)
          output_names.insert(out.name);
        std::set<std::string> emitted_targets;
        for(const std::string& target : d.pass_targets)
        {
          if(output_names.count(target))
            continue;
          if(!emitted_targets.insert(target).second)
            continue;
          samplers += "layout(binding = ";
          samplers += std::to_string(sampler_binding);
          samplers += ") uniform sampler2D ";
          samplers += target;
          samplers += ";\n";
          sampler_binding++;
        }

        // empty uniform blocks are not allowed
        if(num_uniform > 0)
        {
          uniforms += "} isf_material_uniforms;\n";
          uniforms += "\n";
          uniforms += globalvars;
          uniforms += "\n";

          material_ubos += uniforms;
        }

        material_ubos += samplers;

        // Storage buffers (SSBOs) and storage images visible to the graphics
        // pipeline. Bindings continue after samplers.
        sampler_binding = isf_emit_graphics_storage(
            material_ubos, sampler_binding, d.inputs);

        // Multiview UBO: injected when MULTIVIEW >= 2 in the descriptor.
        // Only the UBO here — the #extension pragma must come right after
        // #version, so it's emitted separately below.
        if(d.multiview_count >= 2)
        {
          material_ubos += isf_emit_multiview_ubo(
              sampler_binding, d.multiview_count);
          sampler_binding++;
        }
      }

      m_vertex += material_ubos;
      if(!simpleVS)
      {
        m_vertex += GLSL45.vertexInitFunc;
      }
      else
      {
        m_vertex += GLSL45.vertexInitFunc;
        m_vertex += GLSL45.vertexDefaultMain;
      }

      m_fragment += material_ubos;
      m_fragment += GLSL45.defaultFunctions;
      break;
    }
  }

  // Add the actual vert / frag code
  if(!simpleVS)
    m_vertex += m_sourceVertex;
  m_fragment += fragWithoutISF;

  // Replace the special ISF stuff
  boost::replace_all(m_fragment, "gl_FragColor", "isf_FragColor");
  boost::replace_all(m_fragment, "vv_Frag", "isf_Frag");
}

void parser::parse_raw_raster_pipeline()
{
  using namespace std::literals;

  auto [end, desc] = parse_isf_header(m_sourceFragment);
  m_desc = std::move(desc);

  std::string& fragWithoutISF = m_sourceFragment;
  fragWithoutISF.erase(0, end + 2);

  boost::replace_all(fragWithoutISF, "gl_FragCoord", "isf_FragCoord");

  // Raw raster cannot support multi-pass as the further passes would
  // require different input attributes so it does not really make sense...
  m_desc.passes.clear();
  m_desc.pass_targets.clear();
  m_desc.passes.push_back(isf::pass{});

  m_desc.mode = isf::descriptor::RawRaster;

  // If FRAGMENT_OUTPUTS declares multiple outputs but OUTPUTS was not
  // explicitly provided, auto-populate desc.outputs so the node graph
  // creates the right number of output ports (one per attachment).
  if(m_desc.outputs.empty() && m_desc.fragment_outputs.size() > 1)
  {
    for(const auto& fo : m_desc.fragment_outputs)
    {
      m_desc.outputs.push_back(output_declaration{.name = fo.name, .type = "color"});
    }
  }

  // Add the raw raster uniforms
  {
    static const auto default_ins = [] {
      std::vector<input> default_inputs;
      /*
      default_inputs.push_back(
          input{.name = "Position", .label = "Position", .data = point3d_input{}});
      default_inputs.push_back(
          input{.name = "Center", .label = "Center", .data = point3d_input{}});
      default_inputs.push_back(
          input{.name = "FOV", .label = "FOV", .data = float_input{}});
      default_inputs.push_back(
          input{.name = "Near", .label = "Near", .data = float_input{}});
      default_inputs.push_back(
          input{.name = "Far", .label = "Far", .data = float_input{}});
*/
      auto long_enum = [](int index, auto&&... args) {
        long_input l;
        l.def = index;
        int64_t n = 0;
        auto add = [&l, &n](const auto& arg) {
          l.values.push_back(n++);
          l.labels.push_back(arg);
        };
        (add(args), ...);
        return l;
      };
      default_inputs.push_back(
          input{
              .name = "Mode",
              .label = "Mode",
              .data = long_enum(0, "Triangles", "Points", "Lines")});

      static const auto blend_factors = long_enum(
          1, "Zero", "One", "SrcColor", "OneMinusSrcColor", "DstColor",
          "OneMinusDstColor", "SrcAlpha", "OneMinusSrcAlpha", "DstAlpha",
          "OneMinusDstAlpha", "ConstantColor", "OneMinusConstantColor", "ConstantAlpha",
          "OneMinusConstantAlpha", "SrcAlphaSaturate");
      static const auto blend_ops
          = long_enum(0, "Add", "Substract", "Reverse Substract", "Min", "Max");

      // Default blend: standard alpha blending
      // SrcColor=SrcAlpha(6), DstColor=OneMinusSrcAlpha(7), OpColor=Add(0)
      // SrcAlpha=One(1), DstAlpha=OneMinusSrcAlpha(7), OpAlpha=Add(0)
      auto blend_factors_src_color = blend_factors;
      blend_factors_src_color.def = 6; // SrcAlpha
      auto blend_factors_dst_color = blend_factors;
      blend_factors_dst_color.def = 7; // OneMinusSrcAlpha
      auto blend_factors_src_alpha = blend_factors;
      blend_factors_src_alpha.def = 1; // One
      auto blend_factors_dst_alpha = blend_factors;
      blend_factors_dst_alpha.def = 7; // OneMinusSrcAlpha

      default_inputs.push_back(
          input{.name = "EnableBlend", .label = "Enable blend", .data = bool_input{}});
      default_inputs.push_back(
          input{.name = "SrcColor", .label = "Src Color", .data = blend_factors_src_color});
      default_inputs.push_back(
          input{.name = "DstColor", .label = "Dst Color", .data = blend_factors_dst_color});
      default_inputs.push_back(
          input{.name = "OpColor", .label = "Op Color", .data = blend_ops});
      default_inputs.push_back(
          input{.name = "SrcAlpha", .label = "Src Alpha", .data = blend_factors_src_alpha});
      default_inputs.push_back(
          input{.name = "DstAlpha", .label = "Dst Alpha", .data = blend_factors_dst_alpha});
      default_inputs.push_back(
          input{.name = "OpAlpha", .label = "Op Alpha", .data = blend_ops});
      return default_inputs;
    }();
    m_desc.inputs.insert(m_desc.inputs.begin(), default_ins.begin(), default_ins.end());
  }

  auto& d = m_desc;

  // We start from empty strings.
  m_vertex.clear();
  m_fragment.clear();

  m_vertex = GLSL45.versionPrelude;
  m_fragment = GLSL45.versionPrelude;

  // Extensions pragma block — must come right after #version.
  // GL_ARB_shader_draw_parameters exposes gl_BaseInstance / gl_BaseVertex /
  // gl_DrawIDARB in the vertex shader. Required by MDI shaders that index
  // per-draw data (per_draws[gl_BaseInstance], etc.). Harmless when unused.
  m_vertex += "#extension GL_ARB_shader_draw_parameters : require\n";

  if(m_desc.multiview_count >= 2)
  {
    std::string ext = isf_emit_multiview_extension(m_desc.multiview_count);
    m_vertex += ext;
    m_fragment += ext;
  }

  {
    std::string user_ext = isf_emit_user_extensions(m_desc.extensions);
    m_vertex += user_ext;
    m_fragment += user_ext;
  }

  // LAYER_INDEX for layered outputs.
  {
    bool has_layered_output = (m_desc.multiview_count >= 2);
    for(const auto& out : m_desc.outputs)
      if(out.layers > 1)
        has_layered_output = true;
    if(has_layered_output)
      m_fragment += "#define LAYER_INDEX gl_Layer\n";
  }

  // Write down the inputs / outputs
  {
    // Integer / boolean types require the `flat` interpolation qualifier on
    // varyings (VERTEX_OUTPUTS → FRAGMENT_INPUTS). Without it, Vulkan GLSL
    // compilation fails: "'uint' : must be qualified as flat in".
    auto needs_flat = [](attribute_type t) {
      return (t >= attribute_type::Int && t <= attribute_type::Uint4)
          || (t >= attribute_type::Bool && t <= attribute_type::Bool4);
    };

    // Interpolation qualifier for a varying: user-specified (if valid) wins
    // over the auto "flat" promotion for integer/bool types.
    auto interp_qualifier = [&](const vertex_attribute& a) -> const char* {
      if(a.interpolation == "flat") return "flat";
      if(a.interpolation == "noperspective") return "noperspective";
      if(a.interpolation == "centroid") return "centroid";
      if(a.interpolation == "sample") return "sample";
      if(a.interpolation == "smooth") return ""; // default, no keyword needed
      return needs_flat(a.type) ? "flat" : "";
    };

    // Vertex
    for(auto& attr : m_desc.vertex_inputs)
      m_vertex += fmt::format(
          "layout(location = {}) in {} {};\n", attr.location,
          attribute_type_map.at((int)attr.type), attr.name);
    for(auto& attr : m_desc.vertex_outputs)
      m_vertex += fmt::format(
          "layout(location = {}) {} out {} {};\n", attr.location,
          interp_qualifier(attr),
          attribute_type_map.at((int)attr.type), attr.name);

    for(auto& attr : m_desc.fragment_inputs)
      m_fragment += fmt::format(
          "layout(location = {}) {} in {} {};\n", attr.location,
          interp_qualifier(attr),
          attribute_type_map.at((int)attr.type), attr.name);
    for(auto& attr : m_desc.fragment_outputs)
      m_fragment += fmt::format(
          "layout(location = {}) out {} {};\n", attr.location,
          attribute_type_map.at((int)attr.type), attr.name);

    // Clip / cull distances: user-declared count controls the size of the
    // gl_ClipDistance / gl_CullDistance arrays. Required on some GLSL
    // profiles; always explicit on Vulkan GLSL.
    if(m_desc.clip_distances > 0)
      m_vertex += fmt::format(
          "out float gl_ClipDistance[{}];\n", m_desc.clip_distances);
    if(m_desc.cull_distances > 0)
      m_vertex += fmt::format(
          "out float gl_CullDistance[{}];\n", m_desc.cull_distances);

    // Conservative-depth qualifier on gl_FragDepth. Allowed values map to
    // GLSL layout qualifiers: greater/less/unchanged/any.
    if(!m_desc.depth_layout.empty())
    {
      std::string dl = m_desc.depth_layout;
      for(auto& c : dl) c = (char)tolower(c);
      const char* q = nullptr;
      if(dl == "greater")        q = "depth_greater";
      else if(dl == "less")      q = "depth_less";
      else if(dl == "unchanged") q = "depth_unchanged";
      else if(dl == "any")       q = "depth_any";
      if(q)
        m_fragment += fmt::format(
            "layout({}) out float gl_FragDepth;\n", q);
    }
  }
  {
    // Setup the parameters UBOs
    std::string material_ubos = GLSL45.defaultUniforms;

    // TYPES section structs visible in BOTH stages — see the matching emit
    // in parse_isf for the rationale (SSBO bodies referencing user structs
    // leak into VS via material_ubos and previously failed to compile when
    // VISIBILITY was fragment-only).
    material_ubos += isf_emit_types_struct(d.types);

    int sampler_binding = 3;

    if(!d.inputs.empty())
    {
      std::string uniforms;
      std::string samplers;
      std::string globalvars;
      int num_uniform = 0;
      uniforms += "layout(std140, binding = 2) uniform material_t {\n";
      for(const isf::input& val : d.inputs)
      {
        // Storage buffers / storage images / geometry inputs / UBOs are declared
        // separately after samplers. BUT their synthesized host-side size ints
        // (storage flex-array size, geometry $USER counts) ARE packed into this
        // material blob, so they must be declared here too — otherwise every
        // uniform after them reads shifted. Mirrors the CSF Params block.
        if(auto* storage = ossia::get_if<isf::storage_input>(&val.data))
        {
          if(storage->access.find("write") != std::string::npos
             && !storage->layout.empty()
             && storage->layout.back().type.find("[]") != std::string::npos)
          {
            num_uniform++;
            uniforms += "int " + val.name + "_size;\n";
            globalvars += "int " + val.name + "_size = isf_material_uniforms."
                          + val.name + "_size;\n";
          }
          continue;
        }
        if(auto* geo = ossia::get_if<isf::geometry_input>(&val.data))
        {
          auto emit_synth_int = [&](const std::string& nm) {
            num_uniform++;
            uniforms += "int " + nm + ";\n";
            globalvars += "int " + nm + " = isf_material_uniforms." + nm + ";\n";
          };
          if(geo->vertex_count.find("$USER") != std::string::npos)
            emit_synth_int(val.name + "_vertex_count");
          if(geo->instance_count.find("$USER") != std::string::npos)
            emit_synth_int(val.name + "_instance_count");
          for(const auto& aux : geo->auxiliary)
            if(aux.size.find("$USER") != std::string::npos)
              emit_synth_int(val.name + "_" + aux.name + "_size");
          continue;
        }
        if(ossia::get_if<isf::csf_image_input>(&val.data)
           || ossia::get_if<isf::uniform_input>(&val.data))
          continue;

        auto [type, isSampler] = ossia::visit(create_val_visitor_450{}, val.data);

        if(isSampler)
        {
          samplers += "layout(binding = ";
          samplers += std::to_string(sampler_binding);
          samplers += ") ";
          samplers += type;
          samplers += ' ';
          samplers += val.name;
          samplers += ";\n";

          sampler_binding++;

          if(auto* img = ossia::get_if<isf::image_input>(&val.data))
          {
            if(img->depth)
            {
              samplers += "layout(binding = ";
              samplers += std::to_string(sampler_binding);
              samplers += ") uniform sampler2D ";
              samplers += val.name;
              samplers += "_depth;\n";
              sampler_binding++;
            }
          }
          else if(auto* cube = ossia::get_if<isf::cubemap_input>(&val.data))
          {
            if(cube->depth)
            {
              samplers += "layout(binding = ";
              samplers += std::to_string(sampler_binding);
              samplers += ") uniform samplerCube ";
              samplers += val.name;
              samplers += "_depth;\n";
              sampler_binding++;
            }
          }
        }
        else
        {
          num_uniform++;
          uniforms += type;
          uniforms += ' ';
          uniforms += val.name;
          uniforms += ";\n";

          // See comment above regarding little dance to make spirv-cross happy
          globalvars += type;
          globalvars += ' ';
          globalvars += val.name;
          globalvars += " = isf_material_uniforms.";
          globalvars += val.name;
          globalvars += ";\n";
        }
      }

      // no pass target

      // empty uniform blocks are not allowed
      if(num_uniform > 0)
      {
        uniforms += "} isf_material_uniforms;\n";
        uniforms += "\n";
        uniforms += globalvars;
        uniforms += "\n";

        material_ubos += uniforms;
      }

      material_ubos += samplers;
    }

    // Storage buffers (SSBOs) and storage images declared via INPUTS with
    // TYPE=storage or TYPE=image (visible to graphics stages).
    sampler_binding = isf_emit_graphics_storage(
        material_ubos, sampler_binding, d.inputs);

    // Auxiliary SSBOs (from top-level AUXILIARY key)
    std::string ssbo_decls;

    // Emit a single buffer block for an auxiliary. `qualifier` is the std430
    // access qualifier ("readonly" / "writeonly" / "restrict") and `var` is
    // the variable name (differs from `aux.name` for the _prev ping-pong
    // slot).
    auto emit_aux_block
        = [&](const geometry_input::auxiliary_request& aux, int binding,
              const char* qualifier, const std::string& var) {
      if(aux.is_uniform)
      {
        // std140 UBO: no access qualifier (UBOs are inherently read-only
        // from GLSL), `uniform` instead of `buffer`.
        ssbo_decls += "layout(std140, binding = " + std::to_string(binding) + ") uniform ";
      }
      else
      {
        ssbo_decls += "layout(binding = " + std::to_string(binding) + ", std430) ";
        ssbo_decls += qualifier;
        ssbo_decls += " buffer ";
      }
      ssbo_decls += var;
      ssbo_decls += "_buf {\n";
      for(const auto& field : aux.layout)
      {
        auto bracket = field.type.find('[');
        if(bracket != std::string::npos)
          ssbo_decls += "    " + field.type.substr(0, bracket) + " " + field.name
                        + field.type.substr(bracket) + ";\n";
        else
          ssbo_decls += "    " + field.type + " " + field.name + ";\n";
      }
      ssbo_decls += "} ";
      ssbo_decls += var;
      ssbo_decls += ";\n\n";
    };

    for(const auto& aux : d.auxiliary)
    {
      const char* access_qualifier
          = (aux.access == "read_only")  ? "readonly"
          : (aux.access == "write_only") ? "writeonly"
                                         : "restrict";

      // Persistent ping-pong only makes sense for writable SSBOs. UBOs
      // declared persistent silently fall back to a single-block decl
      // (the flag is ignored by the runtime allocator on the UBO path).
      if(aux.persistent && !aux.is_uniform)
      {
        // Ping-pong pair: _prev is the previous frame's read-only snapshot,
        // <name> is the current frame's writable buffer. Runtime swaps
        // the two buffer pointers each frame.
        emit_aux_block(aux, sampler_binding, "readonly", aux.name + "_prev");
        sampler_binding++;
        emit_aux_block(aux, sampler_binding, access_qualifier, aux.name);
        sampler_binding++;
      }
      else
      {
        emit_aux_block(aux, sampler_binding, access_qualifier, aux.name);
        sampler_binding++;
      }
    }
    material_ubos += ssbo_decls;

    // Auxiliary textures (from top-level AUXILIARY with TYPE: image /
    // texture / cubemap / image_cube / storage_*). No input port; the
    // renderer resolves them from ossia::geometry::auxiliary_textures
    // by name. Sampled textures emit `sampler*` decls with texture()
    // semantics; storage images emit `image*` decls with imageLoad /
    // imageStore semantics.
    std::string aux_tex_decls;
    for(const auto& atx : d.auxiliary_textures)
    {
      if(atx.is_storage)
      {
        // Storage image: imageLoad/Store target. FORMAT layout qualifier
        // is mandatory on writable images; defaults to rgba8.
        // Cube-arrays are parser-rejected so no imageCubeArray branch.
        const char* image_type = "image2D";
        if(atx.is_cubemap)                 image_type = "imageCube";
        else if(atx.dimensions == 3)       image_type = "image3D";
        else if(atx.is_array)              image_type = "image2DArray";

        const char* access_q =
            (atx.access == "read_only") ? "readonly " :
            (atx.access == "write_only") ? "writeonly " : "";

        // Integer formats (r32ui, r32i, rgba32ui, …) require the
        // `uimage*` / `iimage*` GLSL variants — the bare `image*` type
        // paired with an integer layout qualifier is a compile error.
        // Reuses the same prefix helper csf_image_input declarations
        // already use, so float / int / uint emission stays consistent
        // across the rasterizer-aux and csf-input code paths.
        std::string scalar_prefix = isf_glsl_type_prefix(atx.format);

        aux_tex_decls += "layout(binding = " + std::to_string(sampler_binding)
                         + ", " + atx.format + ") uniform " + access_q
                         + scalar_prefix + image_type + " "
                         + atx.name + ";\n";
        sampler_binding++;
      }
      else
      {
        const bool cmp = isf_is_comparison_sampler(atx.sampler);
        const char* sampler_type = "sampler2D";
        // Precedence: cubemap > 3D > array > 2D. sampler3D does not nest
        // with array in core GLSL, so is_array is ignored when dimensions==3.
        // Cube-arrays (samplerCubeArray) are parser-rejected — no backend
        // plumbs CubeMap|TextureArray views correctly.
        if(atx.is_cubemap)
          sampler_type = cmp ? "samplerCubeShadow" : "samplerCube";
        else if(atx.dimensions == 3)
          sampler_type = "sampler3D";
        else if(atx.is_array)
          sampler_type = cmp ? "sampler2DArrayShadow" : "sampler2DArray";
        else
          sampler_type = cmp ? "sampler2DShadow" : "sampler2D";

        aux_tex_decls += "layout(binding = " + std::to_string(sampler_binding)
                         + ") uniform " + sampler_type + " " + atx.name + ";\n";
        sampler_binding++;

        // Paired depth sampler when DEPTH:true on a plain 2D tex.
        if(atx.is_depth && !atx.is_cubemap && atx.dimensions != 3 && !atx.is_array)
        {
          aux_tex_decls += "layout(binding = " + std::to_string(sampler_binding)
                           + ") uniform sampler2D " + atx.name + "_depth;\n";
          sampler_binding++;
        }
      }
    }
    material_ubos += aux_tex_decls;

    // Multiview UBO: injected when MULTIVIEW >= 2.
    if(m_desc.multiview_count >= 2)
    {
      material_ubos += isf_emit_multiview_ubo(
          sampler_binding, m_desc.multiview_count);
      sampler_binding++;
    }

    int model_ubo_binding = sampler_binding;
    material_ubos += fmt::format(
        R"_(layout(std140, binding = {}) uniform model_material_t {{
  mat4 MODEL_;
}} isf_model_uniforms;

#define MODEL_MATRIX isf_model_uniforms.MODEL_
)_",
        model_ubo_binding);

    m_vertex += material_ubos;
    m_fragment += material_ubos;
  }

  // The raw-raster path replaces gl_FragCoord → isf_FragCoord for the
  // same Y-flip behaviour as fullscreen ISF, but unlike ISF the raw-raster
  // FS prelude didn't define the macro — causing "isf_FragCoord :
  // undeclared identifier" for any shader using gl_FragCoord.
  m_fragment += R"_(
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
#define isf_FragCoord vec4(gl_FragCoord.x, RENDERSIZE.y - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w)
#else
#define isf_FragCoord gl_FragCoord
#endif
)_";

  // Add the actual vert / frag code
  m_vertex += m_sourceVertex;
  m_fragment += fragWithoutISF;

  // Replace the special ISF stuff
  boost::replace_all(m_fragment, "gl_FragColor", "isf_FragColor");
  boost::replace_all(m_fragment, "vv_Frag", "isf_Frag");

  // Sanity-check ATTRIBUTES.TYPE references — see helper above.
  validate_attribute_types(m_desc);
}

void parser::parse_shadertoy()
{
  /* Shadertoy uniforms mapping:
  uniform vec3 iResolution;           // viewport resolution (in pixels)
  uniform float iTime;                // shader playback time (in seconds)
  uniform float iTimeDelta;           // render time (in seconds)
  uniform int iFrame;                 // shader playback frame
  uniform float iChannelTime[4];      // channel playback time (in seconds)
  uniform vec3 iChannelResolution[4]; // channel resolution (in pixels)
  uniform vec4 iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
  uniform samplerXX iChannel0..3;     // input channel. XX = 2D/Cube
  uniform vec4 iDate;                 // (year, month, day, time in seconds)
  uniform float iSampleRate;          // sound sample rate (i.e., 44100)
  */

  {
    m_fragment = GLSL45.versionPrelude;
    m_fragment += GLSL45.fragmentPrelude;
    m_fragment += GLSL45.defaultUniforms;
    m_fragment += GLSL45.defaultFunctions;

    // Add Shadertoy compatibility layer
    m_fragment += R"_(
// Shadertoy compatibility uniforms
#define iResolution vec3(RENDERSIZE, 1.0)
#define iTime TIME
#define iTimeDelta TIMEDELTA
#define iFrame FRAMEINDEX
#define iMouse vec2(0.0, 0.0) // fixme MOUSE
#define iDate DATE
#define iSampleRate SAMPLERATE

// For now, we'll use simple defines for channel time/resolution
#define iChannelTime vec4(TIME, TIME, TIME, TIME)
#define iChannelResolution mat4(vec4(RENDERSIZE, 1.0, 0.0), vec4(RENDERSIZE, 1.0, 0.0), vec4(RENDERSIZE, 1.0, 0.0), vec4(RENDERSIZE, 1.0, 0.0))

// Compatibility macros for older shaders
#define iGlobalTime iTime
#define iGlobalDelta iTimeDelta
#define iGlobalFrame iFrame
)_";
  }

  // Add the original Shadertoy code
  m_fragment += m_sourceFragment;

  // Add main function wrapper for mainImage
  {
    m_fragment += R"_(
void main(void)
{
    vec4 fragColor = vec4(0.0);
    mainImage(fragColor, isf_FragCoord.xy);
    isf_FragColor = fragColor;
}
)_";
  }

  // Generate vertex shader
  {
    m_vertex = GLSL45.versionPrelude;
    m_vertex += GLSL45.vertexPrelude;
    m_vertex += GLSL45.defaultUniforms;
    m_vertex += GLSL45.vertexInitFunc;
    m_vertex += GLSL45.vertexDefaultMain;
  }

  // Check if shader uses mainSound or mainVR and add descriptor info
  if(m_sourceFragment.find("vec2 mainSound(") != std::string::npos
     || m_sourceFragment.find("vec2 mainSound (") != std::string::npos)
  {
    // This is a sound shader
    m_desc.categories.push_back("Shadertoy Sound");
  }

  if(m_sourceFragment.find("void mainVR(") != std::string::npos
     || m_sourceFragment.find("void mainVR (") != std::string::npos)
  {
    // This is a VR shader
    m_desc.categories.push_back("Shadertoy VR");
  }

  // Add channel inputs to descriptor
  for(int i = 0; i < 4; ++i)
  {
    input channel_input;
    channel_input.name = "iChannel" + std::to_string(i);
    if(!m_sourceFragment.contains(channel_input.name))
      continue;
    channel_input.label = "Channel " + std::to_string(i);
    channel_input.data = image_input{};
    m_desc.inputs.push_back(channel_input);
  }
}

void parser::parse_glsl_sandbox()
{
  m_fragment += "uniform float TIME;\n";
  m_fragment += "uniform vec2 MOUSE;\n";
  m_fragment += "uniform vec2 RENDERSIZE;\n";
  m_fragment += "out vec2 isf_FragNormCoord;\n";

  m_fragment += m_sourceFragment;

  boost::replace_all(m_fragment, "time", "TIME");
  boost::replace_all(m_fragment, "resolution", "RENDERSIZE");
  boost::replace_all(m_fragment, "mouse", "MOUSE");

  m_vertex =
      R"_(
            in vec2 position;
            uniform vec2 RENDERSIZE;
            out vec2 isf_FragNormCoord;

            void main(void) {
            gl_Position = vec4( position, 0.0, 1.0 );
            isf_FragNormCoord = vec2((gl_Position.x+1.0)/2.0, (gl_Position.y+1.0)/2.0);
            }
   )_";
}

void parser::parse_shadertoy_json(const std::string& json)
{
  descriptor desc;

  // Parse the JSON
  auto doc = sajson::parse(
      sajson::dynamic_allocation(),
      sajson::mutable_string_view(json.length(), const_cast<char*>(json.data())));
  if(!doc.is_valid())
  {
    throw invalid_file{"Invalid JSON: " + std::string(doc.get_error_message())};
  }

  auto root = doc.get_root();
  if(root.get_type() != sajson::TYPE_ARRAY)
  {
    throw invalid_file{"Expected JSON array at root"};
  }

  if(root.get_length() == 0)
  {
    throw invalid_file{"Empty shader array"};
  }

  // Get the first shader
  auto shader = root.get_array_element(0);
  if(shader.get_type() != sajson::TYPE_OBJECT)
  {
    throw invalid_file{"Expected shader object"};
  }

  // Extract shader info
  if(auto info_k = shader.find_object_key(sajson::literal("info"));
     info_k != shader.get_length())
  {
    auto info = shader.get_object_value(info_k);
    if(info.get_type() == sajson::TYPE_OBJECT)
    {
      // Get shader name
      if(auto name_k = info.find_object_key(sajson::literal("name"));
         name_k != info.get_length())
      {
        auto name = info.get_object_value(name_k);
        if(name.get_type() == sajson::TYPE_STRING)
        {
          desc.description = "Shadertoy: " + std::string(name.as_string());
        }
      }

      // Get shader description
      if(auto desc_k = info.find_object_key(sajson::literal("description"));
         desc_k != info.get_length())
      {
        auto description = info.get_object_value(desc_k);
        if(description.get_type() == sajson::TYPE_STRING)
        {
          if(!desc.description.empty())
            desc.description += "\n";
          desc.description += description.as_string();
        }
      }

      // Get author info
      if(auto user_k = info.find_object_key(sajson::literal("username"));
         user_k != info.get_length())
      {
        auto username = info.get_object_value(user_k);
        if(username.get_type() == sajson::TYPE_STRING)
        {
          desc.credits = "By " + std::string(username.as_string()) + " on Shadertoy";
        }
      }

      // Add tags as categories
      if(auto tags_k = info.find_object_key(sajson::literal("tags"));
         tags_k != info.get_length())
      {
        auto tags = info.get_object_value(tags_k);
        if(tags.get_type() == sajson::TYPE_ARRAY)
        {
          for(std::size_t i = 0; i < tags.get_length(); i++)
          {
            auto tag = tags.get_array_element(i);
            if(tag.get_type() == sajson::TYPE_STRING)
            {
              desc.categories.push_back(tag.as_string());
            }
          }
        }
      }
    }
  }

  // Process render passes
  if(auto passes_k = shader.find_object_key(sajson::literal("renderpass"));
     passes_k != shader.get_length())
  {
    auto renderpasses = shader.get_object_value(passes_k);
    if(renderpasses.get_type() == sajson::TYPE_ARRAY)
    {
      // Count passes and check for special types
      int imagePassCount = 0;
      bool hasSound = false;
      bool hasCubemap = false;
      bool hasBuffer = false;

      for(std::size_t i = 0; i < renderpasses.get_length(); i++)
      {
        auto pass = renderpasses.get_array_element(i);
        if(pass.get_type() == sajson::TYPE_OBJECT)
        {
          if(auto type_k = pass.find_object_key(sajson::literal("type"));
             type_k != pass.get_length())
          {
            auto type = pass.get_object_value(type_k);
            if(type.get_type() == sajson::TYPE_STRING)
            {
              std::string typeStr = type.as_string();
              if(typeStr == "image")
                imagePassCount++;
              else if(typeStr == "sound")
                hasSound = true;
              else if(typeStr == "cubemap")
                hasCubemap = true;
              else if(typeStr == "buffer")
                hasBuffer = true;
            }
          }
        }
      }

      // Add appropriate categories
      if(hasSound)
        desc.categories.push_back("Shadertoy Sound");
      if(hasCubemap)
        desc.categories.push_back("Shadertoy Cubemap");
      if(hasBuffer || imagePassCount > 1)
        desc.categories.push_back("Shadertoy Multipass");

      // Process inputs from all passes
      ossia::flat_set<std::string> processedChannels;

      for(std::size_t pass_i = 0; pass_i < renderpasses.get_length(); pass_i++)
      {
        auto pass = renderpasses.get_array_element(pass_i);
        if(pass.get_type() == sajson::TYPE_OBJECT)
        {
          // Get inputs for this pass
          if(auto inputs_k = pass.find_object_key(sajson::literal("inputs"));
             inputs_k != pass.get_length())
          {
            auto inputs = pass.get_object_value(inputs_k);
            if(inputs.get_type() == sajson::TYPE_ARRAY)
            {
              for(std::size_t inp_i = 0; inp_i < inputs.get_length(); inp_i++)
              {
                auto input_obj = inputs.get_array_element(inp_i);
                if(input_obj.get_type() == sajson::TYPE_OBJECT)
                {
                  // Get channel number
                  int channel = -1;
                  if(auto chan_k = input_obj.find_object_key(sajson::literal("channel"));
                     chan_k != input_obj.get_length())
                  {
                    auto chan = input_obj.get_object_value(chan_k);
                    if(chan.get_type() == sajson::TYPE_INTEGER)
                    {
                      channel = chan.get_integer_value();
                    }
                  }

                  if(channel >= 0 && channel < 4)
                  {
                    std::string channelName = "iChannel" + std::to_string(channel);

                    // Only add if not already processed
                    if(processedChannels.find(channelName) == processedChannels.end())
                    {
                      processedChannels.insert(channelName);

                      // Get input type
                      std::string inputType = "texture";
                      if(auto type_k
                         = input_obj.find_object_key(sajson::literal("ctype"));
                         type_k != input_obj.get_length())
                      {
                        auto ctype = input_obj.get_object_value(type_k);
                        if(ctype.get_type() == sajson::TYPE_STRING)
                        {
                          inputType = ctype.as_string();
                        }
                      }

                      // Create appropriate input based on type
                      input inp;
                      inp.name = channelName;
                      inp.label = "Channel " + std::to_string(channel);

                      if(inputType == "texture" || inputType == "cubemap"
                         || inputType == "buffer" || inputType == "video")
                      {
                        inp.data = image_input{};
                      }
                      else if(inputType == "music" || inputType == "musicstream")
                      {
                        inp.data = audio_input{};
                      }
                      else if(inputType == "mic" || inputType == "webcam")
                      {
                        inp.data = image_input{}; // Treat as image input for now
                        inp.label += " (" + inputType + ")";
                      }
                      else
                      {
                        inp.data = image_input{}; // Default to image
                      }

                      desc.inputs.push_back(inp);
                    }
                  }
                }
              }
            }
          }
        }
      }

      // If no specific inputs were found, add the default 4 channels
      if(desc.inputs.empty())
      {
        if(json.contains("iChannel"))
          for(int i = 0; i < 4; ++i)
          {
            input channel_input;
            channel_input.name = "iChannel" + std::to_string(i);
            channel_input.label = "Channel " + std::to_string(i);
            channel_input.data = image_input{};
            desc.inputs.push_back(channel_input);
          }
      }
    }
  }

  // Mark as Shadertoy shader
  if(desc.categories.empty())
  {
    desc.categories.push_back("Shadertoy");
  }

  m_desc = desc;

  // Extract shader code from the first image pass
  std::string mainImageCode;
  if(auto passes_k = shader.find_object_key(sajson::literal("renderpass"));
     passes_k != shader.get_length())
  {
    auto renderpasses = shader.get_object_value(passes_k);
    if(renderpasses.get_type() == sajson::TYPE_ARRAY)
    {
      for(std::size_t i = 0; i < renderpasses.get_length(); i++)
      {
        auto pass = renderpasses.get_array_element(i);
        if(pass.get_type() == sajson::TYPE_OBJECT)
        {
          // Check if this is an image pass
          bool isImagePass = false;
          if(auto type_k = pass.find_object_key(sajson::literal("type"));
             type_k != pass.get_length())
          {
            auto type = pass.get_object_value(type_k);
            if(type.get_type() == sajson::TYPE_STRING
               && std::string(type.as_string()) == "image")
            {
              isImagePass = true;
            }
          }

          if(isImagePass)
          {
            // Extract the shader code
            if(auto code_k = pass.find_object_key(sajson::literal("code"));
               code_k != pass.get_length())
            {
              auto code = pass.get_object_value(code_k);
              if(code.get_type() == sajson::TYPE_STRING)
              {
                mainImageCode = code.as_string();
                break; // Take the first image pass
              }
            }
          }
        }
      }
    }
  }

  // Generate ISF-compatible shaders
  if(!mainImageCode.empty())
  {
    // Generate fragment shader with ISF compatibility
    {
      // Add Shadertoy compatibility layer
      m_fragment += R"_(
// Shadertoy compatibility uniforms
vec3 iResolution  = vec3(RENDERSIZE, 1.0);
float iTime = TIME;
float iTimeDelta = TIMEDELTA;
int iFrame = FRAMEINDEX;
vec4 iMouse = vec2(0.0, 0.0); // FIXME
vec4 iDate = DATE;
float iSampleRate = isf_process_uniforms.SAMPLERATE_;

// For now, we'll use simple defines for channel time/resolution
vec4 iChannelTime = vec4(TIME, TIME, TIME, TIME);
mat4 iChannelResolution = mat4(vec4(RENDERSIZE, 1.0, 0.0), vec4(RENDERSIZE, 1.0, 0.0), vec4(RENDERSIZE, 1.0, 0.0), vec4(RENDERSIZE, 1.0, 0.0));

// Compatibility macros for older shaders
float iGlobalTime = iTime;
float iGlobalDelta = iTimeDelta;
int iGlobalFrame = iFrame;
)_";

      // Add the main Shadertoy code
      m_fragment += mainImageCode;

      // Add main function wrapper
      m_fragment += R"_(

void main(void)
{
    vec4 fragColor = vec4(0.0);
    mainImage(fragColor, isf_FragCoord.xy);
    isf_FragColor = fragColor;
}
)_";

      // Generate vertex shader
      m_vertex = GLSL45.versionPrelude;
      m_vertex += GLSL45.vertexPrelude;
      m_vertex += GLSL45.defaultUniforms;
      m_vertex += GLSL45.vertexInitFunc;
      m_vertex += GLSL45.vertexDefaultMain;
    }
  }
  else
  {
    throw invalid_file{"No valid image shader code found in Shadertoy JSON"};
  }
}

// Helper function to escape JSON strings
// Serialize a sampler_config's non-empty fields as JSON key/value pairs
// onto `oss`, each prefixed with `", "`. Mirrors parse_sampler_config
// exactly so the JSON round-trip is lossless. Writes nothing when every
// field is at its default (empty strings, unset optionals).
static void emit_sampler_config(std::ostream& oss, const isf::sampler_config& s)
{
  auto esc = [](const std::string& x) {
    std::string out;
    out.reserve(x.size());
    for(char c : x)
    {
      if(c == '"' || c == '\\') { out += '\\'; out += c; }
      else out += c;
    }
    return out;
  };
  auto str_field = [&](const char* key, const std::string& val) {
    if(!val.empty())
      oss << ", \"" << key << "\": \"" << esc(val) << "\"";
  };
  auto float_field = [&](const char* key, const std::optional<float>& val) {
    if(val) oss << ", \"" << key << "\": " << *val;
  };

  str_field("WRAP",         s.wrap);
  str_field("WRAP_S",       s.wrap_s);
  str_field("WRAP_T",       s.wrap_t);
  str_field("WRAP_R",       s.wrap_r);
  str_field("FILTER",       s.filter);
  str_field("MIN_FILTER",   s.min_filter);
  str_field("MAG_FILTER",   s.mag_filter);
  str_field("MIPMAP_MODE",  s.mipmap_mode);
  str_field("BORDER_COLOR", s.border_color);
  str_field("COMPARE",      s.compare);
  float_field("ANISOTROPY", s.anisotropy);
  float_field("LOD_BIAS",   s.lod_bias);
  float_field("MIN_LOD",    s.min_lod);
  float_field("MAX_LOD",    s.max_lod);
}

static auto escape_json(const std::string& str) -> std::string
{
  std::string result;
  for(char c : str)
  {
    switch(c)
    {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
};
std::string parser::write_isf() const
{
  std::ostringstream oss;

  // Generate ISF JSON header
  oss << "/*\n{\n";

  // Add description if present
  if(!m_desc.description.empty())
  {
    oss << "  \"DESCRIPTION\": \"" << escape_json(m_desc.description) << "\",\n";
  }

  // Add credits if present
  if(!m_desc.credits.empty())
  {
    oss << "  \"CREDIT\": \"" << escape_json(m_desc.credits) << "\",\n";
  }

  // Add categories if present
  if(!m_desc.categories.empty())
  {
    oss << "  \"CATEGORIES\": [\n";
    for(size_t i = 0; i < m_desc.categories.size(); ++i)
    {
      oss << "    \"" << escape_json(m_desc.categories[i]) << "\"";
      if(i < m_desc.categories.size() - 1)
        oss << ",";
      oss << "\n";
    }
    oss << "  ]";
    if(!m_desc.inputs.empty() || !m_desc.passes.empty()
       || !m_desc.extensions.empty())
      oss << ",";
    oss << "\n";
  }

  // Add extensions if present
  if(!m_desc.extensions.empty())
  {
    oss << "  \"EXTENSIONS\": [\n";
    for(size_t i = 0; i < m_desc.extensions.size(); ++i)
    {
      oss << "    \"" << escape_json(m_desc.extensions[i]) << "\"";
      if(i + 1 < m_desc.extensions.size())
        oss << ",";
      oss << "\n";
    }
    oss << "  ]";
    if(!m_desc.inputs.empty() || !m_desc.passes.empty())
      oss << ",";
    oss << "\n";
  }

  // Add inputs
  if(!m_desc.inputs.empty())
  {
    oss << "  \"INPUTS\": [\n";

    for(size_t i = 0; i < m_desc.inputs.size(); ++i)
    {
      const auto& input = m_desc.inputs[i];
      oss << "    {\n";
      oss << "      \"NAME\": \"" << escape_json(input.name) << "\",\n";

      if(!input.label.empty())
      {
        oss << "      \"LABEL\": \"" << escape_json(input.label) << "\",\n";
      }

      // Handle different input types
      struct input_serializer
      {
        std::ostringstream& oss;

        void operator()(const float_input& f)
        {
          oss << "      \"TYPE\": \"float\",\n";
          oss << "      \"MIN\": " << f.min << ",\n";
          oss << "      \"MAX\": " << f.max << ",\n";
          oss << "      \"DEFAULT\": " << f.def << "\n";
        }

        void operator()(const long_input& l)
        {
          oss << "      \"TYPE\": \"long\",\n";
          if(!l.values.empty())
          {
            oss << "      \"VALUES\": [";
            for(size_t i = 0; i < l.values.size(); ++i)
            {
              oss << l.values[i];
              if(i < l.values.size() - 1)
                oss << ", ";
            }
            oss << "],\n";
          }
          if(!l.labels.empty())
          {
            oss << "      \"LABELS\": [";
            for(size_t i = 0; i < l.labels.size(); ++i)
            {
              oss << "\"" << escape_json(l.labels[i]) << "\"";
              if(i < l.labels.size() - 1)
                oss << ", ";
            }
            oss << "],\n";
          }
          if(l.min)
            oss << "      \"MIN\": " << *l.min << ",\n";
          if(l.max)
            oss << "      \"MAX\": " << *l.max << ",\n";
          oss << "      \"DEFAULT\": " << l.def << "\n";
        }

        void operator()(const bool_input& b)
        {
          oss << "      \"TYPE\": \"bool\",\n";
          oss << "      \"DEFAULT\": " << (b.def ? "true" : "false") << "\n";
        }

        void operator()(const event_input&) { oss << "      \"TYPE\": \"event\"\n"; }

        void operator()(const point2d_input& p)
        {
          oss << "      \"TYPE\": \"point2D\"";
          if(p.min)
          {
            oss << ",\n      \"MIN\": [" << (*p.min)[0] << ", " << (*p.min)[1] << "]";
          }
          if(p.max)
          {
            oss << ",\n      \"MAX\": [" << (*p.max)[0] << ", " << (*p.max)[1] << "]";
          }
          if(p.def)
          {
            oss << ",\n      \"DEFAULT\": [" << (*p.def)[0] << ", " << (*p.def)[1]
                << "]";
          }
          oss << "\n";
        }

        void operator()(const point3d_input& p)
        {
          oss << "      \"TYPE\": \"point3D\"";
          if(p.min)
          {
            oss << ",\n      \"MIN\": [" << (*p.min)[0] << ", " << (*p.min)[1] << ", "
                << (*p.min)[2] << "]";
          }
          if(p.max)
          {
            oss << ",\n      \"MAX\": [" << (*p.max)[0] << ", " << (*p.max)[1] << ", "
                << (*p.max)[2] << "]";
          }
          if(p.def)
          {
            oss << ",\n      \"DEFAULT\": [" << (*p.def)[0] << ", " << (*p.def)[1]
                << ", " << (*p.def)[2] << "]";
          }
          if(p.as_color)
            oss << ",\n      \"AS_COLOR\": true";
          oss << "\n";
        }

        void operator()(const color_input& c)
        {
          oss << "      \"TYPE\": \"color\"";
          if(c.min)
          {
            oss << ",\n      \"MIN\": [" << (*c.min)[0] << ", " << (*c.min)[1] << ", "
                << (*c.min)[2] << ", " << (*c.min)[3] << "]";
          }
          if(c.max)
          {
            oss << ",\n      \"MAX\": [" << (*c.max)[0] << ", " << (*c.max)[1] << ", "
                << (*c.max)[2] << ", " << (*c.max)[3] << "]";
          }
          if(c.def)
          {
            oss << ",\n      \"DEFAULT\": [" << (*c.def)[0] << ", " << (*c.def)[1]
                << ", " << (*c.def)[2] << ", " << (*c.def)[3] << "]";
          }
          oss << "\n";
        }

        void operator()(const image_input& img) {
          oss << "      \"TYPE\": \"image\"";
          if(img.depth)
            oss << ",\n      \"DEPTH\": true";
          if(img.is_array)
            oss << ",\n      \"IS_ARRAY\": true";
          if(img.dimensions != 2)
            oss << ",\n      \"DIMENSIONS\": " << img.dimensions;
          oss << "\n";
        }
        void operator()(const cubemap_input& c)
        {
          oss << "      \"TYPE\": \"cubemap\"";
          if(c.depth)
            oss << ",\n      \"DEPTH\": true";
          oss << "\n";
        }

        void operator()(const audio_input& a)
        {
          oss << "      \"TYPE\": \"audio\"";
          if(a.max > 0)
            oss << ",\n      \"MAX\": " << a.max;
          if(!a.sampler.filter.empty())
            oss << ",\n      \"FILTER\": \"" << escape_json(a.sampler.filter) << "\"";
          if(!a.sampler.wrap.empty())
            oss << ",\n      \"WRAP\": \"" << escape_json(a.sampler.wrap) << "\"";
          oss << "\n";
        }

        void operator()(const audioFFT_input& a)
        {
          oss << "      \"TYPE\": \"audioFFT\"";
          if(a.max > 0)
            oss << ",\n      \"MAX\": " << a.max;
          if(!a.sampler.filter.empty())
            oss << ",\n      \"FILTER\": \"" << escape_json(a.sampler.filter) << "\"";
          if(!a.sampler.wrap.empty())
            oss << ",\n      \"WRAP\": \"" << escape_json(a.sampler.wrap) << "\"";
          oss << "\n";
        }

        void operator()(const audioHist_input& a)
        {
          oss << "      \"TYPE\": \"audioHistogram\"";
          if(a.max > 0)
            oss << ",\n      \"MAX\": " << a.max;
          if(!a.sampler.filter.empty())
            oss << ",\n      \"FILTER\": \"" << escape_json(a.sampler.filter) << "\"";
          if(!a.sampler.wrap.empty())
            oss << ",\n      \"WRAP\": \"" << escape_json(a.sampler.wrap) << "\"";
          oss << "\n";
        }

        // CSF-specific input handlers
        void operator()(const storage_input& s)
        {
          oss << "      \"TYPE\": \"storage\",\n";
          oss << "      \"ACCESS\": \"" << s.access << "\"";
          if(!s.buffer_usage.empty())
            oss << ",\n      \"BUFFER_USAGE\": \"" << escape_json(s.buffer_usage) << "\"";
          if(s.persistent)
            oss << ",\n      \"PERSISTENT\": true";
          if(!s.visibility.empty() && s.visibility != "fragment")
            oss << ",\n      \"VISIBILITY\": \"" << escape_json(s.visibility) << "\"";
          if(!s.layout.empty())
          {
            oss << ",\n      \"LAYOUT\": [\n";
            for(size_t i = 0; i < s.layout.size(); ++i)
            {
              const auto& field = s.layout[i];
              oss << "        {\"name\": \"" << escape_json(field.name)
                  << "\", \"type\": \"" << escape_json(field.type) << "\"}";
              if(i < s.layout.size() - 1)
                oss << ",";
              oss << "\n";
            }
            oss << "      ]";
          }
          oss << "\n";
        }

        void operator()(const uniform_input& u)
        {
          oss << "      \"TYPE\": \"uniform\",\n";
          oss << "      \"LAYOUT\": [\n";
          for(std::size_t k = 0; k < u.layout.size(); ++k)
          {
            const auto& f = u.layout[k];
            oss << "        { \"NAME\": \"" << escape_json(f.name)
                << "\", \"TYPE\": \"" << escape_json(f.type) << "\" }";
            if(k + 1 < u.layout.size())
              oss << ",";
            oss << "\n";
          }
          oss << "      ]";
          if(!u.visibility.empty() && u.visibility != "vertex+fragment")
            oss << ",\n      \"VISIBILITY\": \"" << escape_json(u.visibility) << "\"";
          oss << "\n";
        }

        void operator()(const texture_input&) { oss << "      \"TYPE\": \"texture\"\n"; }

        void operator()(const csf_image_input& img)
        {
          oss << "      \"TYPE\": \"image\",\n";
          oss << "      \"ACCESS\": \"" << img.access << "\",\n";
          oss << "      \"FORMAT\": \"" << img.format << "\"";
          if(!img.visibility.empty() && img.visibility != "compute")
            oss << ",\n      \"VISIBILITY\": \"" << escape_json(img.visibility) << "\"";
          if(img.persistent)
            oss << ",\n      \"PERSISTENT\": true";
          if(img.is_array)
            oss << ",\n      \"IS_ARRAY\": true";
          if(!img.layers_expression.empty())
            oss << ",\n      \"LAYERS\": \"" << escape_json(img.layers_expression) << "\"";
          oss << "\n";
        }

        void operator()(const geometry_input& geo)
        {
          oss << "      \"TYPE\": \"geometry\"";
          if(!geo.vertex_count.empty())
          {
            // Try to emit as integer if possible, otherwise as string
            try { std::stoi(geo.vertex_count); oss << ",\n      \"VERTEX_COUNT\": " << geo.vertex_count; }
            catch(...) { oss << ",\n      \"VERTEX_COUNT\": \"" << escape_json(geo.vertex_count) << "\""; }
          }
          if(!geo.instance_count.empty())
          {
            try { std::stoi(geo.instance_count); oss << ",\n      \"INSTANCE_COUNT\": " << geo.instance_count; }
            catch(...) { oss << ",\n      \"INSTANCE_COUNT\": \"" << escape_json(geo.instance_count) << "\""; }
          }
          if(!geo.format_id.empty())
            oss << ",\n      \"FORMAT_ID\": \"" << escape_json(geo.format_id) << "\"";
          if(!geo.attributes.empty())
          {
            oss << ",\n      \"ATTRIBUTES\": [\n";
            for(size_t i = 0; i < geo.attributes.size(); ++i)
            {
              const auto& attr = geo.attributes[i];
              oss << "        {\"NAME\": \"" << escape_json(attr.name) << "\"";
              if(!attr.semantic.empty())
                oss << ", \"SEMANTIC\": \"" << escape_json(attr.semantic) << "\"";
              if(!attr.type.empty())
                oss << ", \"TYPE\": \"" << escape_json(attr.type) << "\"";
              if(!attr.access.empty())
                oss << ", \"ACCESS\": \"" << escape_json(attr.access) << "\"";
              if(!attr.rate.empty() && attr.rate != "vertex")
                oss << ", \"RATE\": \"" << escape_json(attr.rate) << "\"";
              if(!attr.required)
                oss << ", \"REQUIRED\": false";
              oss << "}";
              if(i < geo.attributes.size() - 1)
                oss << ",";
              oss << "\n";
            }
            oss << "      ]";
          }
          if(!geo.auxiliary.empty() || !geo.auxiliary_textures.empty())
          {
            oss << ",\n      \"AUXILIARY\": [\n";
            const size_t nb = geo.auxiliary.size();
            const size_t nt = geo.auxiliary_textures.size();
            for(size_t i = 0; i < nb; ++i)
            {
              const auto& aux = geo.auxiliary[i];
              oss << "        {\"NAME\": \"" << escape_json(aux.name) << "\"";
              // TYPE: "uniform" for UBO-kind aux. SSBO kind omits TYPE —
              // default parse dispatch lands there.
              if(aux.is_uniform)
                oss << ", \"TYPE\": \"uniform\"";
              if(!aux.access.empty() && !aux.is_uniform)
                oss << ", \"ACCESS\": \"" << escape_json(aux.access) << "\"";
              if(!aux.size.empty())
              {
                try { std::stoi(aux.size); oss << ", \"SIZE\": " << aux.size; }
                catch(...) { oss << ", \"SIZE\": \"" << escape_json(aux.size) << "\""; }
              }
              if(!aux.layout.empty())
              {
                oss << ", \"LAYOUT\": [";
                for(size_t j = 0; j < aux.layout.size(); ++j)
                {
                  const auto& field = aux.layout[j];
                  oss << "{\"NAME\": \"" << escape_json(field.name)
                      << "\", \"TYPE\": \"" << escape_json(field.type) << "\"}";
                  if(j < aux.layout.size() - 1)
                    oss << ", ";
                }
                oss << "]";
              }
              oss << "}";
              if(i < nb - 1 || nt > 0)
                oss << ",";
              oss << "\n";
            }
            // Texture auxiliaries — identifying TYPE field so parse round-
            // trips via aux_entry_is_texture. Full sampler_config fields
            // are emitted via emit_sampler_config so WRAP/FILTER/COMPARE
            // etc. round-trip losslessly.
            for(size_t i = 0; i < nt; ++i)
            {
              const auto& atx = geo.auxiliary_textures[i];
              oss << "        {\"NAME\": \"" << escape_json(atx.name) << "\"";
              // TYPE field — reuse the specific storage_* variants so
              // parse dispatch and re-emit stay symmetric.
              if(atx.is_storage)
              {
                if(atx.is_cubemap && atx.is_array)
                  oss << ", \"TYPE\": \"storage_cube\""; // Note: no array-cube storage variant in current vocabulary
                else if(atx.is_cubemap)
                  oss << ", \"TYPE\": \"storage_cube\"";
                else if(atx.dimensions == 3)
                  oss << ", \"TYPE\": \"storage_3d\"";
                else if(atx.is_array)
                  oss << ", \"TYPE\": \"storage_image_array\"";
                else
                  oss << ", \"TYPE\": \"storage_image\"";
              }
              else if(atx.is_cubemap)
                oss << ", \"TYPE\": \"cubemap\"";
              else
                oss << ", \"TYPE\": \"image\"";
              if(atx.is_array && !atx.is_storage)
                oss << ", \"IS_ARRAY\": true";
              if(atx.dimensions != 2 && !atx.is_storage)
                oss << ", \"DIMENSIONS\": " << atx.dimensions;
              if(atx.is_depth)
                oss << ", \"DEPTH\": true";
              if(atx.is_storage)
              {
                if(!atx.format.empty() && atx.format != "rgba8")
                  oss << ", \"FORMAT\": \"" << escape_json(atx.format) << "\"";
                if(!atx.access.empty() && atx.access != "read_write")
                  oss << ", \"ACCESS\": \"" << escape_json(atx.access) << "\"";
              }
              emit_sampler_config(oss, atx.sampler);
              oss << "}";
              if(i < nt - 1)
                oss << ",";
              oss << "\n";
            }
            oss << "      ]";
          }
          oss << "\n";
        }
      };

      ossia::visit(input_serializer{oss}, input.data);

      oss << "    }";
      if(i < m_desc.inputs.size() - 1)
        oss << ",";
      oss << "\n";
    }

    oss << "  ]";

    // Add comma if there are passes
    if(!m_desc.passes.empty())
    {
      oss << ",";
    }
    oss << "\n";
  }

  // Add passes if present
  if(!m_desc.passes.empty())
  {
    oss << "  \"PASSES\": [\n";

    for(size_t i = 0; i < m_desc.passes.size(); ++i)
    {
      const auto& pass = m_desc.passes[i];
      oss << "    {\n";

      if(!pass.target.empty())
      {
        oss << "      \"TARGET\": \"" << escape_json(pass.target) << "\",\n";
      }

      if(pass.persistent)
      {
        oss << "      \"PERSISTENT\": true,\n";
      }

      if(pass.float_storage)
      {
        oss << "      \"FLOAT\": true,\n";
      }

      if(pass.nearest_filter)
      {
        oss << "      \"FILTER\": \"NEAREST\",\n";
      }

      if(!pass.width_expression.empty())
      {
        // Check if it's a numeric value or expression
        try
        {
          std::stod(pass.width_expression);
          oss << "      \"WIDTH\": " << pass.width_expression << ",\n";
        }
        catch(...)
        {
          oss << "      \"WIDTH\": \"" << escape_json(pass.width_expression) << "\",\n";
        }
      }

      if(!pass.height_expression.empty())
      {
        // Check if it's a numeric value or expression
        try
        {
          std::stod(pass.height_expression);
          oss << "      \"HEIGHT\": " << pass.height_expression << ",\n";
        }
        catch(...)
        {
          oss << "      \"HEIGHT\": \"" << escape_json(pass.height_expression) << "\",\n";
        }
      }

      if(!pass.z_expression.empty())
      {
        try
        {
          std::stod(pass.z_expression);
          oss << "      \"Z\": " << pass.z_expression << ",\n";
        }
        catch(...)
        {
          oss << "      \"Z\": \"" << escape_json(pass.z_expression) << "\",\n";
        }
      }

      if(!pass.format.empty())
      {
        oss << "      \"FORMAT\": \"" << escape_json(pass.format) << "\",\n";
      }

      // Remove trailing comma if last property
      auto str = oss.str();
      if(str.size() > 2 && str[str.size() - 2] == ',')
      {
        oss.str(str.substr(0, str.size() - 2) + "\n");
      }

      oss << "    }";
      if(i < m_desc.passes.size() - 1)
        oss << ",";
      oss << "\n";
    }

    oss << "  ]\n";
  }

  oss << "}\n*/\n\n";

  // Add the fragment shader code
  if(!m_fragment.empty())
  {
    oss << m_fragment;
  }

  return oss.str();
}

void parser::parse_vsa()
{
  // VSA shaders are vertex shaders, so we process the vertex source

  auto [end, desc] = parse_isf_header(m_sourceVertex);
  m_desc = std::move(desc);

  // Remove the ISF json
  m_sourceVertex.erase(0, end + 2);

  // Replace main() so that we can override gl_Position.y in vulkan
  static constexpr auto main_rexp_str
      = ctll::fixed_string{R"_(main\s*\(\s*(void)?\s*\))_"};
  static constexpr auto main_rex = ctre::search<main_rexp_str>;

  if(auto match = main_rex(m_sourceVertex))
  {
    auto idx = match.begin();
    auto e = match.end();
    m_sourceVertex.replace(
        idx - m_sourceVertex.begin(), int(e - idx), "main__vsa_ossia()");
  }
  else
  {
    return;
  }

  // There is always one pass at least
  if(m_desc.passes.empty())
  {
    m_desc.passes.push_back(isf::pass{});
  }

  std::string vsaSource = m_sourceVertex;

  // Set up descriptor for VSA
  m_desc.mode = isf::descriptor::VSA;
  m_desc.description = "Vertex Shader Art";
  m_desc.categories.push_back("VSA");
  m_desc.default_vertex_shader = false;

  // Add standard VSA inputs to descriptor, at the end so that we can easily replace audioreactive ones
  // Vertex count control
  {
    input vertexCount;
    vertexCount.name = "vertexCount";
    vertexCount.label = "Vertex Count";
    float_input vc;
    vc.min = 3.0;
    vc.max = 100000.0;
    vc.def = 10000.0;
    if(m_desc.point_count > 0)
      vc.def = m_desc.point_count;
    vertexCount.data = vc;
    m_desc.inputs.push_back(vertexCount);
  }

  // Primitive type
  {
    input primitiveType;
    primitiveType.name = "primitiveType";
    primitiveType.label = "Primitive Type";
    long_input vc;
    vc.values.push_back("POINTS");
    vc.values.push_back("LINE_STRIP");
    vc.values.push_back("LINE_LOOP");
    vc.values.push_back("LINES");
    vc.values.push_back("TRI_STRIP");
    vc.values.push_back("TRI_FAN");
    vc.values.push_back("TRIANGLES");
    vc.def = 6;
    if(!m_desc.primitive_mode.empty())
    {
      for(int i = 0; i < 7; i++)
        if(m_desc.primitive_mode == ossia::get<std::string>(vc.values[i]))
        {
          vc.def = i;
          break;
        }
    }
    primitiveType.data = vc;
    m_desc.inputs.push_back(primitiveType);
  }

  // Generate GLSL 4.5 vertex shader
  m_vertex = GLSL45.versionPrelude;

  // Add uniforms
  m_vertex += GLSL45.defaultUniforms;
  m_vertex += GLSL45.defaultFunctions;

  {
    int sampler_binding = 3;
    auto& d = m_desc;
    std::string material_ubos;
    std::string samplers;
    std::string globalvars;
    material_ubos += "layout(std140, binding = 2) uniform material_t {\n";
    for(const isf::input& val : d.inputs)
    {
      auto [type, isSampler] = ossia::visit(create_val_visitor_450{}, val.data);

      if(isSampler)
      {
        samplers += "layout(binding = ";
        samplers += std::to_string(sampler_binding);
        samplers += ") ";
        samplers += type;
        samplers += ' ';
        samplers += val.name;
        samplers += ";\n";

        sampler_binding++;

        if(auto* img = ossia::get_if<isf::image_input>(&val.data))
        {
          if(img->depth)
          {
            samplers += "layout(binding = ";
            samplers += std::to_string(sampler_binding);
            samplers += ") uniform sampler2D ";
            samplers += val.name;
            samplers += "_depth;\n";
            sampler_binding++;
          }
        }
        else if(auto* cube = ossia::get_if<isf::cubemap_input>(&val.data))
        {
          if(cube->depth)
          {
            samplers += "layout(binding = ";
            samplers += std::to_string(sampler_binding);
            samplers += ") uniform samplerCube ";
            samplers += val.name;
            samplers += "_depth;\n";
            sampler_binding++;
          }
        }
      }
      else
      {
        material_ubos += type;
        material_ubos += ' ';
        material_ubos += val.name;
        material_ubos += ";\n";

        // See comment above regarding little dance to make spirv-cross happy
        globalvars += type;
        globalvars += ' ';
        globalvars += val.name;
        globalvars += " = isf_material_uniforms.";
        globalvars += val.name;
        globalvars += ";\n";
      }
    }

    material_ubos += "} isf_material_uniforms;\n";
    material_ubos += "\n";
    material_ubos += globalvars;
    material_ubos += "\n";

    material_ubos += samplers;
    m_vertex += material_ubos;
  }

  // Add material uniforms for VSA
  m_vertex += R"_(
// Compatibility defines for VSA
float time = TIME;
vec2 resolution = RENDERSIZE;
)_";

  // Add vertex input - using gl_VertexIndex for simplicity
  m_vertex += R"_(
// VSA Vertex Shader inputs
float vertexId = float(gl_VertexIndex);

layout(location = 0) out vec4 v_color;
)_";
  // Add the processed VSA code
  m_vertex += vsaSource;

  m_vertex += R"_(

void main() {
  main__vsa_ossia();
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

  m_fragment = GLSL45.versionPrelude;
  m_fragment += R"_(
layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 isf_FragColor;

void main() {
  isf_FragColor = v_color;
}
)_";
}

void parser::parse_csf()
{
  using namespace std::literals;

  auto [end, desc] = parse_isf_header(m_sourceFragment);
  m_desc = std::move(desc);
  m_desc.mode = descriptor::CSF;

  std::string& compWithoutCSF = m_sourceFragment;
  compWithoutCSF.erase(0, end + 2);

  // Generate compute shader
  m_fragment.clear();

  // Add version
  m_fragment += "#version 450\n\n";

  // User-declared GLSL EXTENSIONS must come right after #version.
  m_fragment += isf_emit_user_extensions(m_desc.extensions);

  // Add standard ProcessUBO uniforms (same as ISF/VSA)
  m_fragment += GLSL45.defaultUniforms;
  m_fragment += "\n";

  // Add local_size placeholder — substituted per-pass at pipeline creation time
  // to support different local_size per pass.
  m_fragment += "layout(local_size_x = ISF_LOCAL_SIZE_X"
                ", local_size_y = ISF_LOCAL_SIZE_Y"
                ", local_size_z = ISF_LOCAL_SIZE_Z) in;\n\n";

  // Generate struct definitions from TYPES section.
  //
  // No auto-padding: GLSL+std430 handles alignment based on actual member
  // types (vec4 16B-aligned, float/uint 4B-aligned, struct rounds to its
  // largest member). The previous "(4 - field_count % 4) % 4 trailing
  // floats" heuristic was based on the field count modulo 4, completely
  // unrelated to real alignment, and silently grew the struct stride
  // when field_count wasn't a multiple of 4. RawLight (7 fields) became
  // 68B → 80B std430-stride here while every rasterizer (graphics-path
  // TYPES emitter has no such heuristic) and ScenePreprocessor's
  // RawLight arena both use 64B stride — pack_lights_from_points writes
  // landed at 80B intervals while the consumer rasterizer read at 64B
  // intervals, garbling every slot past index 0 (the user's symptom:
  // procedural light positions acting like colours, all lights piled up
  // at the constant light_color value). Mirror the graphics-path
  // emitter (isf_emit_types_struct) verbatim instead.
  if(!m_desc.types.empty())
  {
    m_fragment += "// Struct definitions from TYPES section\n";
    for(const auto& type_def : m_desc.types)
    {
      m_fragment += "struct " + type_def.name + " {\n";
      for(const auto& field : type_def.layout)
      {
        auto bracket = field.type.find('[');
        if(bracket != std::string::npos)
          m_fragment += "    " + field.type.substr(0, bracket) + " " + field.name
                        + field.type.substr(bracket) + ";\n";
        else
          m_fragment += "    " + field.type + " " + field.name + ";\n";
      }
      m_fragment += "};\n\n";
    }
  }

  // Generate uniform buffer for ISF-style inputs
  bool has_uniforms = false;

  // Count ISF-style inputs (non-resource types)
  for(const auto& inp : m_desc.inputs)
  {
    auto storage = ossia::get_if<storage_input>(&inp.data);
    if(storage && storage->access.contains("write"))
    {
      has_uniforms = true;
      break;
    }
    auto image = ossia::get_if<csf_image_input>(&inp.data);
    if(image && image->access.contains("write"))
    {
      has_uniforms = true;
      break;
    }

    // Geometry inputs with $USER specs contribute int uniforms to the material UBO
    auto geo = ossia::get_if<geometry_input>(&inp.data);
    if(geo)
    {
      if(geo->vertex_count.find("$USER") != std::string::npos
         || geo->instance_count.find("$USER") != std::string::npos)
      {
        has_uniforms = true;
        break;
      }
      for(const auto& aux : geo->auxiliary)
      {
        if(aux.size.find("$USER") != std::string::npos)
        {
          has_uniforms = true;
          break;
        }
      }
      if(has_uniforms)
        break;
      continue;
    }

    if(!storage && !image && !ossia::get_if<texture_input>(&inp.data)
       && !geo)
    {
      has_uniforms = true;
      break;
    }
  }

  int binding = 2; // Start at 2 since ProcessUBO takes slots 0 and 1

  if(has_uniforms)
  {
    std::string material_block;
    material_block += "// Automatically generated uniform block from INPUTS\n";
    material_block += "layout(std140, binding = 2) uniform Params {\n";

    int k = 0;
    for(const auto& inp : m_desc.inputs)
    {
      // Generate uniform declarations for ISF-style inputs
      if(ossia::get_if<float_input>(&inp.data))
      {
        k++;
        material_block += "    float " + inp.name + ";\n";
      }
      else if(ossia::get_if<long_input>(&inp.data))
      {
        k++;
        material_block += "    int " + inp.name + ";\n";
      }
      else if(ossia::get_if<bool_input>(&inp.data))
      {
        k++;
        material_block += "    bool " + inp.name + ";\n";
      }
      else if(ossia::get_if<point2d_input>(&inp.data))
      {
        k++;
        material_block += "    vec2 " + inp.name + ";\n";
      }
      else if(ossia::get_if<point3d_input>(&inp.data))
      {
        k++;
        material_block += "    vec3 " + inp.name + ";\n";
      }
      else if(ossia::get_if<color_input>(&inp.data))
      {
        k++;
        material_block += "    vec4 " + inp.name + ";\n";
      }
      else if(ossia::get_if<event_input>(&inp.data))
      {
        k++;
        material_block += "    bool " + inp.name + ";\n";
      }
      else if(auto* geo = ossia::get_if<geometry_input>(&inp.data))
      {
        // $USER vertex_count/instance_count/aux sizes are int uniforms
        if(geo->vertex_count.find("$USER") != std::string::npos)
        {
          k++;
          material_block += "    int " + inp.name + "_vertex_count;\n";
        }
        if(geo->instance_count.find("$USER") != std::string::npos)
        {
          k++;
          material_block += "    int " + inp.name + "_instance_count;\n";
        }
        for(const auto& aux : geo->auxiliary)
        {
          if(aux.size.find("$USER") != std::string::npos)
          {
            k++;
            material_block += "    int " + inp.name + "_" + aux.name + "_size;\n";
          }
        }
      }
      else if(auto* storage = ossia::get_if<storage_input>(&inp.data))
      {
        // A writable storage buffer whose LAYOUT ends in a flexible-array
        // member gets a synthesized host-side size int (see ISFVisitors /
        // RenderedCSFNode). Declare it here so this std140 block matches the
        // packed material blob; otherwise every uniform after it reads shifted.
        if(storage->access.find("write") != std::string::npos
           && !storage->layout.empty()
           && storage->layout.back().type.find("[]") != std::string::npos)
        {
          k++;
          material_block += "    int " + inp.name + "_size;\n";
        }
      }
    }

    material_block += "};\n\n";

    if(k > 0)
      m_fragment += material_block;
    binding++;
  }

  // Helper: derive GLSL image/sampler prefix from format string.
  // Unsigned integer formats (R32UI, RGBA16UI, ...) → "u"
  // Signed integer formats (R32I, RGBA16I, ...) → "i"
  // Float formats (R32F, RGBA8, ...) → ""
  auto glsl_type_prefix = [](const std::string& format) -> std::string {
    if(format.empty())
      return "";
    // Uppercase copy for matching
    std::string fmt = format;
    for(auto& c : fmt) c = toupper(c);
    // Check for unsigned int formats (end with UI or contain UI before digits)
    if(fmt.find("UI") != std::string::npos)
      return "u";
    // Check for signed int: ends with I (but not UI, F, or SNORM/UNORM)
    // Integer formats: R8I, R16I, R32I, RG8I, RG16I, RG32I, RGBA8I, RGBA16I, RGBA32I
    if(fmt.size() >= 2 && fmt.back() == 'I' && fmt[fmt.size()-2] != 'U')
      return "i";
    return "";
  };

  // Detect aux name collisions across multiple geometry_inputs.
  // When the same aux name appears in more than one geometry input, the GLSL
  // interface block instance names would collide, so we prefix them with the
  // geometry input name. Single-geometry shaders keep the legacy unprefixed
  // instance name for backwards compatibility.
  std::set<std::string> colliding_aux_names;
  {
    std::map<std::string, int> aux_name_counts;
    for(const auto& inp : m_desc.inputs)
    {
      if(auto* g = ossia::get_if<geometry_input>(&inp.data))
      {
        for(const auto& aux : g->auxiliary)
        {
          if(aux.forward)
            continue;
          aux_name_counts[aux.name]++;
        }
      }
    }
    for(const auto& [name, count] : aux_name_counts)
    {
      if(count > 1)
        colliding_aux_names.insert(name);
    }
  }

  // Generate resource bindings
  m_fragment += "// From RESOURCES - bindings assigned automatically\n";
  bool emitted_indirect_struct = false;
  for(const auto& inp : m_desc.inputs)
  {
    if(auto* storage_ptr = ossia::get_if<storage_input>(&inp.data))
    {
      const auto& storage = *storage_ptr;

      m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) ";

      if(storage.access == "read_only")
        m_fragment += "readonly ";
      else if(storage.access == "write_only")
        m_fragment += "writeonly ";
      else
        m_fragment += "restrict ";

      m_fragment += "buffer " + inp.name + "_buf {\n";

      // Add struct members based on layout
      for(const auto& field : storage.layout)
      {
        auto bracket = field.type.find('[');
        if(bracket != std::string::npos)
          m_fragment += "    " + field.type.substr(0, bracket) + " " + field.name
                        + field.type.substr(bracket) + ";\n";
        else
          m_fragment += "    " + field.type + " " + field.name + ";\n";
      }

      m_fragment += "} " + inp.name + ";\n\n";

      binding++;
    }
    else if(auto* img_ptr = ossia::get_if<csf_image_input>(&inp.data))
    {
      const auto& img = *img_ptr;

      // Emit the primary image binding, then — if persistent — emit a
      // readonly `<name>_prev` alias at the following slot. The runtime
      // ping-pongs between two textures and swaps pointers each frame so
      // the shader sees current-frame writes on `<name>` and the previous
      // frame's state on `<name>_prev`.
      auto emit_image = [&](int b, const std::string& decl_name, bool alias_prev) {
        m_fragment += "layout(binding = " + std::to_string(b);

        if(!img.format.empty())
        {
          std::string format = img.format;
          boost::algorithm::to_lower(format);
          m_fragment += ", " + format;
        }
        else
        {
          m_fragment += ", rgba8"; // Default format
        }

        m_fragment += ") ";

        if(alias_prev || img.access == "read_only")
          m_fragment += "readonly ";
        else if(img.access == "write_only")
          m_fragment += "writeonly ";
        else
          m_fragment += "restrict ";

        auto prefix = glsl_type_prefix(img.format);
        const char* shape = "image2D";
        if(img.isCube())      shape = "imageCube";
        else if(img.is3D())   shape = "image3D";
        else if(img.is_array) shape = "image2DArray";
        m_fragment += "uniform " + prefix + shape + " ";
        m_fragment += decl_name + ";\n";
      };

      emit_image(binding, inp.name, /*alias_prev=*/false);
      binding++;
      if(img.persistent)
      {
        emit_image(binding, inp.name + "_prev", /*alias_prev=*/true);
        binding++;
      }
    }
    else if(auto* tex_ptr = ossia::get_if<texture_input>(&inp.data))
    {
      m_fragment += "layout(binding = " + std::to_string(binding) + ") ";
      m_fragment += tex_ptr->dimensions == 3
          ? "uniform sampler3D " : "uniform sampler2D ";
      m_fragment += inp.name + ";\n";
      binding++;
    }
    else if(auto* uni_ptr = ossia::get_if<uniform_input>(&inp.data))
    {
      m_fragment += isf_emit_ubo_decl(binding, inp.name, *uni_ptr);
      binding++;
    }
    else if(auto* geo_ptr = ossia::get_if<geometry_input>(&inp.data))
    {
      const auto& geo = *geo_ptr;

      m_fragment += "// Geometry input \"" + inp.name + "\" — SoA: one SSBO per attribute\n";
      m_fragment += "#define ISF_READ(geo, attr) geo ## _ ## attr ## _in\n";
      m_fragment += "#define ISF_WRITE(geo, attr) geo ## _ ## attr ## _out\n";
      // Nested-aux structured-SSBO/UBO instance access. Resolves to the
      // instance name emitted by the SSBO/UBO block below — bare aux name
      // when there's no cross-geometry collision, prefixed otherwise.
      // Use this instead of writing `scene_cluster_aabbs.data[...]` by
      // hand: the macro keeps shaders working if the same aux name later
      // appears in another geometry input and forces a name collision
      // (the SSBO emitter switches to the prefixed instance name then).
      m_fragment += "#define ISF_AUX(geo, name) geo ## _ ## name\n";
      // Nested-aux image access (storage images: read_only / write_only /
      // read_write). For images there's no _in / _out distinction at the
      // GLSL level — the same identifier carries the full access mode
      // determined by the layout qualifier. Same one-name-per-image
      // contract applies via the alias #define emitted in the texture
      // block below.
      m_fragment += "#define ISF_IMG(geo, name) geo ## _ ## name\n";
      // Nested-aux sampler access (read-only sampled textures with
      // texture()/textureLod()/etc.). Symmetric to ISF_IMG — separate
      // macro because the GLSL type differs (samplerXY vs imageXY) and
      // future shaders may want to grep for usage independently.
      m_fragment += "#define ISF_TEX(geo, name) geo ## _ ## name\n";

      for(const auto& attr : geo.attributes)
      {
        // "none" access: forwarded via COPY_FROM, no SSBO needed
        if(attr.access == "none")
          continue;

        const std::string prefix = inp.name + "_" + attr.name;

        if(attr.access == "read_only")
        {
          // Single readonly SSBO, 1 binding
          m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) ";
          m_fragment += "readonly buffer " + prefix + "_in_buf { ";
          m_fragment += attr.type + " " + prefix + "_in[]; };\n";
          binding++;
        }
        else if(attr.access == "write_only")
        {
          // Single writeonly SSBO, 1 binding
          m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) ";
          m_fragment += "writeonly buffer " + prefix + "_out_buf { ";
          m_fragment += attr.type + " " + prefix + "_out[]; };\n";
          binding++;
        }
        else // read_write
        {
          // Two SSBOs: _in (readonly) and _out (read-write), 2 bindings
          m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) ";
          m_fragment += "readonly buffer " + prefix + "_in_buf { ";
          m_fragment += attr.type + " " + prefix + "_in[]; };\n";
          binding++;

          m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) ";
          m_fragment += "restrict buffer " + prefix + "_out_buf { ";
          m_fragment += attr.type + " " + prefix + "_out[]; };\n";
          binding++;
        }
        // No shorthand alias — users must use ISF_READ(geo, attr) / ISF_WRITE(geo, attr)
      }

      // Auxiliary structured SSBOs (travel with the geometry)
      for(const auto& aux : geo.auxiliary)
      {
        // COPY_FROM auxiliaries are forwarded in pushOutputGeometry, no SSBO needed
        if(aux.forward)
          continue;

        const std::string aux_prefix = inp.name + "_" + aux.name;

        // Use a prefixed instance name when the same aux name appears in
        // multiple geometry inputs (otherwise GLSL would reject the duplicate
        // interface block instance name). Single-geometry shaders keep the
        // legacy unprefixed instance name for backwards compatibility.
        const bool collides = colliding_aux_names.count(aux.name) > 0;
        const std::string instance_name = collides ? aux_prefix : aux.name;

        if(aux.is_uniform)
        {
          // std140 UBO: no access qualifier, `uniform` not `buffer`.
          m_fragment += "layout(std140, binding = " + std::to_string(binding) + ") uniform ";
        }
        else
        {
          m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) ";
          if(aux.access == "read_only")
            m_fragment += "readonly ";
          else if(aux.access == "write_only")
            m_fragment += "writeonly ";
          else
            m_fragment += "restrict ";
          m_fragment += "buffer ";
        }

        m_fragment += aux_prefix + "_buf {\n";
        for(const auto& field : aux.layout)
        {
          // Handle array types: "vec4[512]" → "vec4 entries[512];"
          auto bracket = field.type.find('[');
          if(bracket != std::string::npos)
          {
            m_fragment += "    " + field.type.substr(0, bracket) + " " + field.name
                          + field.type.substr(bracket) + ";\n";
          }
          else
          {
            m_fragment += "    " + field.type + " " + field.name + ";\n";
          }
        }
        m_fragment += "} " + instance_name + ";\n";

        // Generate ISF_READ/ISF_WRITE-compatible aliases. UBOs are always
        // read-only from GLSL's perspective (the `access` field is ignored
        // for UBO kind), so only the `_in` / unqualified aliases exist.
        const std::string eff_access = aux.is_uniform ? "read_only" : aux.access;
        if(eff_access == "read_only")
        {
          m_fragment += "#define " + aux_prefix + "_in " + instance_name + "\n";
          if(!collides)
            m_fragment += "#define " + aux_prefix + " " + instance_name + "\n";
        }
        else if(eff_access == "write_only")
        {
          m_fragment += "#define " + aux_prefix + "_out " + instance_name + "\n";
          if(!collides)
            m_fragment += "#define " + aux_prefix + " " + instance_name + "\n";
        }
        else // read_write
        {
          m_fragment += "#define " + aux_prefix + "_in " + instance_name + "\n";
          m_fragment += "#define " + aux_prefix + "_out " + instance_name + "\n";
          if(!collides)
            m_fragment += "#define " + aux_prefix + " " + instance_name + "\n";
        }
        m_fragment += "\n";

        binding++;
      }

      // Auxiliary textures (travel with the geometry; resolved by the
      // renderer from ossia::geometry::auxiliary_textures by name).
      // RenderedCSFNode binds them right after aux SSBOs in the compute
      // SRB build loop — order here must match that order.
      //
      // Each texture is emitted under its bare aux name (e.g.
      // `voxel_grid`) — same convention as the structured-SSBO/UBO block
      // above when there's no name collision. A `#define
      // <geo>_<aux> <aux>` alias is also emitted so author shaders can
      // use either the prefixed form directly OR the ISF_IMG /
      // ISF_TEX macros (which expand to `geo ## _ ## aux`). Keeps
      // image-aux access symmetric with SSBO/UBO-aux access.
      for(const auto& atx : geo.auxiliary_textures)
      {
        const std::string aux_prefix = inp.name + "_" + atx.name;
        const bool aliased = (aux_prefix != atx.name);

        if(atx.is_storage)
        {
          // Cube-arrays are parser-rejected so no imageCubeArray branch.
          const char* image_type = "image2D";
          if(atx.is_cubemap)                 image_type = "imageCube";
          else if(atx.dimensions == 3)       image_type = "image3D";
          else if(atx.is_array)              image_type = "image2DArray";

          const char* access_q =
              (atx.access == "read_only") ? "readonly " :
              (atx.access == "write_only") ? "writeonly " : "";

          // Integer formats (r32ui, r32i, …) require uimage*/iimage*.
          std::string scalar_prefix = isf_glsl_type_prefix(atx.format);

          m_fragment += "layout(binding = " + std::to_string(binding)
                        + ", " + atx.format + ") uniform " + access_q
                        + scalar_prefix + image_type + " "
                        + atx.name + ";\n";
          if(aliased)
            m_fragment += "#define " + aux_prefix + " " + atx.name + "\n";
          binding++;
        }
        else
        {
          const bool cmp = isf_is_comparison_sampler(atx.sampler);
          const char* sampler_type = "sampler2D";
          // Cube-arrays (samplerCubeArray) are parser-rejected — no QRhi
          // backend plumbs CubeMap|TextureArray views correctly.
          if(atx.is_cubemap)
            sampler_type = cmp ? "samplerCubeShadow" : "samplerCube";
          else if(atx.dimensions == 3)
            sampler_type = "sampler3D";
          else if(atx.is_array)
            sampler_type = cmp ? "sampler2DArrayShadow" : "sampler2DArray";
          else
            sampler_type = cmp ? "sampler2DShadow" : "sampler2D";

          m_fragment += "layout(binding = " + std::to_string(binding)
                        + ") uniform " + sampler_type + " " + atx.name + ";\n";
          if(aliased)
            m_fragment += "#define " + aux_prefix + " " + atx.name + "\n";
          binding++;

          if(atx.is_depth && !atx.is_cubemap && atx.dimensions != 3 && !atx.is_array)
          {
            m_fragment += "layout(binding = " + std::to_string(binding)
                          + ") uniform sampler2D " + atx.name + "_depth;\n";
            if(aliased)
              m_fragment += "#define " + aux_prefix + "_depth "
                            + atx.name + "_depth\n";
            binding++;
          }
        }
      }

      // Indirect draw command buffer (user-writable SSBO)
      if(geo.indirect)
      {
        if(!emitted_indirect_struct)
        {
          m_fragment += "struct DrawIndirectCommand {\n"
                        "    uint vertexCount;\n"
                        "    uint instanceCount;\n"
                        "    uint firstVertex;\n"
                        "    int  baseVertex;\n"
                        "    uint firstInstance;\n"
                        "};\n\n";
          emitted_indirect_struct = true;
        }
        const std::string buf_name = inp.name + "_indirect";
        m_fragment += "layout(binding = " + std::to_string(binding) + ", std430) "
                      "restrict buffer " + buf_name + "_buf {\n"
                      "    DrawIndirectCommand " + buf_name + "[];\n"
                      "};\n";
        m_fragment += "#define ISF_INDIRECT(" + inp.name + ") " + buf_name + "\n\n";
        binding++;
      }

      // Element count uniform (packed into the material UBO or standalone)
      m_fragment += "// Element count for geometry input \"" + inp.name + "\"\n";
      m_fragment += "// (set by the renderer from ossia::geometry::vertices)\n";
      m_fragment += "// Access via the ProcessUBO or a dedicated uniform.\n";

      // #define guards for attribute presence
      for(const auto& attr : geo.attributes)
      {
        std::string upper_name = attr.name;
        for(auto& c : upper_name)
          c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        m_fragment += "#define HAS_" + upper_name + " 1\n";
      }
      m_fragment += "\n";
    }
  }

  m_fragment += "\n";

  // Add the user's compute shader code (without the JSON header)
  boost::algorithm::trim(compWithoutCSF);
  m_fragment += compWithoutCSF;

  // Sanity-check: every ATTRIBUTES.TYPE references a real GLSL built-in
  // or a TYPES entry. Throws invalid_file with the offending name on
  // miss — surfaces typos at parse time.
  validate_attribute_types(m_desc);
}

descriptor::Mode parser::mode() const
{
  return m_desc.mode;
}

std::string parser::compute_shader() const
{
  return m_fragment;
}

}
