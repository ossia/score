#include "isf.hpp"

#include "sajson.h"

#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/string_map.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <array>
#include <iostream>
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
  gl_Position = clipSpaceCorrMatrix * vec4( position, 0.0, 1.0 );
  isf_FragNormCoord = vec2((gl_Position.x+1.0)/2.0, (gl_Position.y+1.0)/2.0);
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

  static constexpr auto vertexDefaultMain = R"_(
void main()
{
  isf_vertShaderInit();
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
} isf_renderer_uniforms;

// This dance is needed because otherwise
// spirv-cross may generate different struct names in the vertex & fragment, causing crashes..
// but we have to keep compat with ISF
#define clipSpaceCorrMatrix isf_renderer_uniforms.clipSpaceCorrMatrix_

// Time-dependent uniforms, only relevant during execution
layout(std140, binding = 1) uniform process_t {
  float TIME_;
  float TIMEDELTA_;
  float PROGRESS_;

  int PASSINDEX_;
  int FRAMEINDEX_;

  vec2 RENDERSIZE_;
  vec4 DATE_;
  vec4 MOUSE_;
  vec4 CHANNEL_TIME_;
  float SAMPLERATE_;
} isf_process_uniforms;
 
#define TIME isf_process_uniforms.TIME_
#define TIMEDELTA isf_process_uniforms.TIMEDELTA_
#define PROGRESS isf_process_uniforms.PROGRESS_
#define PASSINDEX isf_process_uniforms.PASSINDEX_
#define FRAMEINDEX isf_process_uniforms.FRAMEINDEX_
#define RENDERSIZE isf_process_uniforms.RENDERSIZE_
#define MOUSE isf_process_uniforms.MOUSE_
#define DATE isf_process_uniforms.DATE_
)_";

  static constexpr auto defaultFunctions =
      R"_(
#define TEX_DIMENSIONS(tex) isf_material_uniforms._ ## tex ## _imgRect.zw
#define IMG_SIZE(tex) isf_material_uniforms._ ## tex ## _imgRect.zw

#if defined(QSHADER_SPIRV)
#define isf_FragCoord vec2(gl_FragCoord.x, RENDERSIZE.y - gl_FragCoord.y)
#define ISF_FIXUP_TEXCOORD(coord) vec2((coord).x, 1. - (coord).y)
#define IMG_THIS_PIXEL(tex) texture(tex, ISF_FIXUP_TEXCOORD(isf_FragNormCoord))
#define IMG_THIS_NORM_PIXEL(tex) texture(tex, ISF_FIXUP_TEXCOORD(isf_FragNormCoord))
#define IMG_PIXEL(tex, coord) texture(tex, ISF_FIXUP_TEXCOORD(coord / RENDERSIZE))
#define IMG_NORM_PIXEL(tex, coord) texture(tex, ISF_FIXUP_TEXCOORD(coord))
#else
#define isf_FragCoord gl_FragCoord
#define IMG_THIS_PIXEL(tex) texture(tex, isf_FragNormCoord)
#define IMG_THIS_NORM_PIXEL(tex) texture(tex, isf_FragNormCoord)
#define IMG_PIXEL(tex, coord) texture(tex, (coord) / RENDERSIZE)
#define IMG_NORM_PIXEL(tex, coord) texture(tex, coord)
#endif
)_";

} GLSL45;
}

parser::parser(std::string geom)
    : m_source_geometry_filter{std::move(geom)}
{
  parse_geometry_filter();
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

    return str[0] == '/' && str[1] == '*';
  };
  static const auto is_shadertoy_json
      = [](const std::string& str) { return str.starts_with("[{\"ver\":\""); };
  static const auto is_shadertoy = [](const std::string& str) {
    return str.find("void mainImage(") != std::string::npos;
  };
  static const auto is_glslsandbox = [](const std::string& str) {
    return str.find("uniform float time;") != std::string::npos
           || str.find("glslsandbox") != std::string::npos;
  };
  static const auto is_vsa = [](const std::string& str) {
    // Check for common VSA patterns
    bool has_vertexId = str.find("vertexId") != std::string::npos;
    bool has_v_color = str.find("v_color") != std::string::npos;
    bool no_frag_color = str.find("_FragColor") == std::string::npos;

    return (has_vertexId || has_v_color) && (no_frag_color);
  };

  switch(t)
  {
    case ShaderType::Autodetect: {
      if(is_shadertoy_json(m_sourceFragment))
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
        inp.name = val.as_string();
    }
    else if(k == "LABEL")
    {
      auto val = v.get_object_value(i);
      if(val.get_type() == sajson::TYPE_STRING)
        inp.label = val.as_string();
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

static void parse_input(image_input& inp, const sajson::value& v) { }

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
          if(arr_value.get_type() == sajson::TYPE_DOUBLE)
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
  }
  auto min_size = std::min(inp.labels.size(), inp.values.size());
  inp.def = std::min(inp.def, min_size - 1);
  if(inp.labels.size() < min_size)
    inp.labels.resize(min_size);
  if(inp.values.size() < min_size)
    inp.values.resize(min_size);
}

template <typename Input_T>
  requires Input_T::has_minmax::value
static void parse_input(Input_T& inp, const sajson::value& v)
{
  std::size_t N = v.get_length();

  for(std::size_t i = 0; i < N; i++)
  {
    auto k = v.get_object_key(i).as_string();
    if(k == "MIN")
    {
      auto val = v.get_object_value(i);
      inp.min = parse_input_impl(val, typename Input_T::value_type{});
    }
    else if(k == "MAX")
    {
      auto val = v.get_object_value(i);
      inp.max = parse_input_impl(val, typename Input_T::value_type{});
    }
    else if(k == "DEFAULT")
    {
      auto val = v.get_object_value(i);
      inp.def = parse_input_impl(val, typename Input_T::value_type{});
    }
  }

  // Some ISF shaders have e.g. "MIN": 0, "MAX": -5 to show them reversed in the ISF editor gui...
  if(inp.min > inp.max)
    std::swap(inp.min, inp.max);
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
                  if(v.get_type() == sajson::TYPE_STRING)
                    d.categories.push_back(v.as_string());
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
    i.insert({"point2D", [](const auto& s) { return parse<point2d_input>(s); }});
    i.insert({"point3D", [](const auto& s) { return parse<point3d_input>(s); }});
    i.insert({"color", [](const auto& s) { return parse<color_input>(s); }});
    i.insert({"audio", [](const auto& s) { return parse<audio_input>(s); }});
    i.insert({"audioFFT", [](const auto& s) { return parse<audioFFT_input>(s); }});
    i.insert(
        {"audioHistogram", [](const auto& s) { return parse<audioHist_input>(s); }});

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
                      auto inp = input_parse.find(obj.get_object_value(k).as_string());
                      if(inp != input_parse.end())
                        d.inputs.push_back((inp->second)(obj));
                    }
                  }
                }
              }
            }});

  p.insert(
      {"PASSES",
       [](descriptor& d, const sajson::value& v) {
    using namespace std::literals;
    if(v.get_type() == sajson::TYPE_ARRAY)
    {
      std::size_t n = v.get_length();
      for(std::size_t i = 0; i < n; i++)
      {
        auto obj = v.get_array_element(i);
        if(obj.get_type() == sajson::TYPE_OBJECT)
        {
          // PASS object
          pass p;
          if(auto target_k = obj.find_object_key_insensitive(sajson::literal("TARGET"));
             target_k != obj.get_length())
          {
            p.target = obj.get_object_value(target_k).as_string();
            if(!p.target.empty())
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

          if(auto height_k = obj.find_object_key_insensitive(sajson::literal("HEIGHT"));
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

          d.passes.push_back(std::move(p));
        }
      }
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

  return p;
}()};

struct create_val_visitor
{
  std::string operator()(const float_input&) { return "uniform float"; }
  std::string operator()(const long_input&) { return "uniform int"; }
  std::string operator()(const event_input&) { return "uniform bool"; }
  std::string operator()(const bool_input&) { return "uniform bool"; }
  std::string operator()(const point2d_input&) { return "uniform vec2"; }
  std::string operator()(const point3d_input&) { return "uniform vec3"; }
  std::string operator()(const color_input&) { return "uniform vec4"; }
  std::string operator()(const image_input&) { return "uniform sampler2D"; }
  std::string operator()(const audio_input&) { return "uniform sampler2D"; }
  std::string operator()(const audioFFT_input&) { return "uniform sampler2D"; }
  std::string operator()(const audioHist_input&) { return "uniform sampler2D"; }
};

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
  return_type operator()(const image_input&) { return {"uniform sampler2D", true}; }
  return_type operator()(const audio_input&) { return {"uniform sampler2D", true}; }
  return_type operator()(const audioFFT_input&) { return {"uniform sampler2D", true}; }
  return_type operator()(const audioHist_input&) { return {"uniform sampler2D", true}; }
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

  auto& d = m_desc;

  // We start from empty strings.

  bool simpleVS = false;
  m_vertex.clear();
  m_fragment.clear();

  // Create the GLSL prelude
  switch(m_version)
  {
    case 450: {
      // Setup vertex shader
      {
        m_vertex = GLSL45.versionPrelude;

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
        m_fragment += GLSL45.fragmentPrelude;
      }

      // Setup the parameters UBOs
      std::string material_ubos = GLSL45.defaultUniforms;

      int sampler_binding = 3;

      if(!d.inputs.empty() || !d.pass_targets.empty())
      {
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

            auto imgRect_varname = "_" + val.name + "_imgRect";
            material_ubos += "vec4 " + imgRect_varname + ";\n";
            // See comment above regarding little dance to make spirv-cross happy
            globalvars += "vec4 ";
            globalvars += imgRect_varname;
            globalvars += " = isf_material_uniforms.";
            globalvars += imgRect_varname;
            globalvars += ";\n";

            sampler_binding++;
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

        for(const std::string& target : d.pass_targets)
        {
          samplers += "layout(binding = ";
          samplers += std::to_string(sampler_binding);
          samplers += ") uniform sampler2D ";
          samplers += target;
          samplers += ";\n";

          material_ubos += "vec4 _" + target + "_imgRect;\n";

          sampler_binding++;
        }

        material_ubos += "} isf_material_uniforms;\n";
        material_ubos += "\n";
        material_ubos += globalvars;
        material_ubos += "\n";

        material_ubos += samplers;
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
#define iMouse MOUSE
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
  for(int i = 0; i < 4; ++i) {
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
  auto doc = sajson::parse(sajson::dynamic_allocation(), sajson::mutable_string_view(json.length(), const_cast<char*>(json.data())));
  if(!doc.is_valid()) {
    throw invalid_file{"Invalid JSON: " + std::string(doc.get_error_message())};
  }
  
  auto root = doc.get_root();
  if(root.get_type() != sajson::TYPE_ARRAY) {
    throw invalid_file{"Expected JSON array at root"};
  }
  
  if(root.get_length() == 0) {
    throw invalid_file{"Empty shader array"};
  }
  
  // Get the first shader
  auto shader = root.get_array_element(0);
  if(shader.get_type() != sajson::TYPE_OBJECT) {
    throw invalid_file{"Expected shader object"};
  }
  
  // Extract shader info
  if(auto info_k = shader.find_object_key(sajson::literal("info"));
     info_k != shader.get_length())
  {
    auto info = shader.get_object_value(info_k);
    if(info.get_type() == sajson::TYPE_OBJECT) {
      // Get shader name
      if(auto name_k = info.find_object_key(sajson::literal("name"));
         name_k != info.get_length())
      {
        auto name = info.get_object_value(name_k);
        if(name.get_type() == sajson::TYPE_STRING) {
          desc.description = "Shadertoy: " + std::string(name.as_string());
        }
      }

      // Get shader description
      if(auto desc_k = info.find_object_key(sajson::literal("description"));
         desc_k != info.get_length())
      {
        auto description = info.get_object_value(desc_k);
        if(description.get_type() == sajson::TYPE_STRING) {
          if(!desc.description.empty()) desc.description += "\n";
          desc.description += description.as_string();
        }
      }

      // Get author info
      if(auto user_k = info.find_object_key(sajson::literal("username"));
         user_k != info.get_length())
      {
        auto username = info.get_object_value(user_k);
        if(username.get_type() == sajson::TYPE_STRING) {
          desc.credits = "By " + std::string(username.as_string()) + " on Shadertoy";
        }
      }

      // Add tags as categories
      if(auto tags_k = info.find_object_key(sajson::literal("tags"));
         tags_k != info.get_length())
      {
        auto tags = info.get_object_value(tags_k);
        if(tags.get_type() == sajson::TYPE_ARRAY) {
          for(std::size_t i = 0; i < tags.get_length(); i++) {
            auto tag = tags.get_array_element(i);
            if(tag.get_type() == sajson::TYPE_STRING) {
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
    if(renderpasses.get_type() == sajson::TYPE_ARRAY) {
      // Count passes and check for special types
      int imagePassCount = 0;
      bool hasSound = false;
      bool hasCubemap = false;
      bool hasBuffer = false;
      
      for(std::size_t i = 0; i < renderpasses.get_length(); i++) {
        auto pass = renderpasses.get_array_element(i);
        if(pass.get_type() == sajson::TYPE_OBJECT) {
          if(auto type_k = pass.find_object_key(sajson::literal("type"));
             type_k != pass.get_length())
          {
            auto type = pass.get_object_value(type_k);
            if(type.get_type() == sajson::TYPE_STRING) {
              std::string typeStr = type.as_string();
              if(typeStr == "image") imagePassCount++;
              else if(typeStr == "sound") hasSound = true;
              else if(typeStr == "cubemap") hasCubemap = true;
              else if(typeStr == "buffer") hasBuffer = true;
            }
          }
        }
      }
      
      // Add appropriate categories
      if(hasSound) desc.categories.push_back("Shadertoy Sound");
      if(hasCubemap) desc.categories.push_back("Shadertoy Cubemap");
      if(hasBuffer || imagePassCount > 1) desc.categories.push_back("Shadertoy Multipass");
      
      // Process inputs from all passes
      ossia::flat_set<std::string> processedChannels;
      
      for(std::size_t pass_i = 0; pass_i < renderpasses.get_length(); pass_i++) {
        auto pass = renderpasses.get_array_element(pass_i);
        if(pass.get_type() == sajson::TYPE_OBJECT) {
          // Get inputs for this pass
          if(auto inputs_k = pass.find_object_key(sajson::literal("inputs"));
             inputs_k != pass.get_length())
          {
            auto inputs = pass.get_object_value(inputs_k);
            if(inputs.get_type() == sajson::TYPE_ARRAY) {
              for(std::size_t inp_i = 0; inp_i < inputs.get_length(); inp_i++) {
                auto input_obj = inputs.get_array_element(inp_i);
                if(input_obj.get_type() == sajson::TYPE_OBJECT) {
                  // Get channel number
                  int channel = -1;
                  if(auto chan_k = input_obj.find_object_key(sajson::literal("channel"));
                     chan_k != input_obj.get_length())
                  {
                    auto chan = input_obj.get_object_value(chan_k);
                    if(chan.get_type() == sajson::TYPE_INTEGER) {
                      channel = chan.get_integer_value();
                    }
                  }

                  if(channel >= 0 && channel < 4)
                  {
                    std::string channelName = "iChannel" + std::to_string(channel);
                    
                    // Only add if not already processed
                    if(processedChannels.find(channelName) == processedChannels.end()) {
                      processedChannels.insert(channelName);
                      
                      // Get input type
                      std::string inputType = "texture";
                      if(auto type_k
                         = input_obj.find_object_key(sajson::literal("ctype"));
                         type_k != input_obj.get_length())
                      {
                        auto ctype = input_obj.get_object_value(type_k);
                        if(ctype.get_type() == sajson::TYPE_STRING) {
                          inputType = ctype.as_string();
                        }
                      }

                      // Create appropriate input based on type
                      input inp;
                      inp.name = channelName;
                      inp.label = "Channel " + std::to_string(channel);
                      
                      if(inputType == "texture" || inputType == "cubemap" || inputType == "buffer" || inputType == "video") {
                        inp.data = image_input{};
                      } else if(inputType == "music" || inputType == "musicstream") {
                        inp.data = audio_input{};
                      } else if(inputType == "mic" || inputType == "webcam") {
                        inp.data = image_input{}; // Treat as image input for now
                        inp.label += " (" + inputType + ")";
                      } else {
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
      if(desc.inputs.empty()) {
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
  if(desc.categories.empty()) {
    desc.categories.push_back("Shadertoy");
  }

  m_desc = desc;
  
  // Extract shader code from the first image pass
  std::string mainImageCode;
  if(auto passes_k = shader.find_object_key(sajson::literal("renderpass"));
     passes_k != shader.get_length())
  {
    auto renderpasses = shader.get_object_value(passes_k);
    if(renderpasses.get_type() == sajson::TYPE_ARRAY) {
      for(std::size_t i = 0; i < renderpasses.get_length(); i++) {
        auto pass = renderpasses.get_array_element(i);
        if(pass.get_type() == sajson::TYPE_OBJECT) {
          // Check if this is an image pass
          bool isImagePass = false;
          if(auto type_k = pass.find_object_key(sajson::literal("type"));
             type_k != pass.get_length())
          {
            auto type = pass.get_object_value(type_k);
            if(type.get_type() == sajson::TYPE_STRING && 
               std::string(type.as_string()) == "image") {
              isImagePass = true;
            }
          }
          
          if(isImagePass) {
            // Extract the shader code
            if(auto code_k = pass.find_object_key(sajson::literal("code"));
               code_k != pass.get_length())
            {
              auto code = pass.get_object_value(code_k);
              if(code.get_type() == sajson::TYPE_STRING) {
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
  if(!mainImageCode.empty()) {
    // Generate fragment shader with ISF compatibility
    {
      // Add Shadertoy compatibility layer
      m_fragment += R"_(
// Shadertoy compatibility uniforms
vec3 iResolution  = vec3(RENDERSIZE, 1.0);
float iTime = TIME;
float iTimeDelta = TIMEDELTA;
int iFrame = FRAMEINDEX;
vec4 iMouse = MOUSE;
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
  } else {
    throw invalid_file{"No valid image shader code found in Shadertoy JSON"};
  }
}

// Helper function to escape JSON strings
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
  if(!m_desc.description.empty()) {
    oss << "  \"DESCRIPTION\": \"" << escape_json(m_desc.description) << "\",\n";
  }
  
  // Add credits if present
  if(!m_desc.credits.empty()) {
    oss << "  \"CREDIT\": \"" << escape_json(m_desc.credits) << "\",\n";
  }
  
  // Add categories if present
  if(!m_desc.categories.empty()) {
    oss << "  \"CATEGORIES\": [\n";
    for(size_t i = 0; i < m_desc.categories.size(); ++i) {
      oss << "    \"" << escape_json(m_desc.categories[i]) << "\"";
      if(i < m_desc.categories.size() - 1)
        oss << ",";
      oss << "\n";
    }
    oss << "  ]";
    if(!m_desc.inputs.empty() || !m_desc.passes.empty())
      oss << ",";
    oss << "\n";
  }
  
  // Add inputs
  if(!m_desc.inputs.empty()) {
    oss << "  \"INPUTS\": [\n";
    
    for(size_t i = 0; i < m_desc.inputs.size(); ++i) {
      const auto& input = m_desc.inputs[i];
      oss << "    {\n";
      oss << "      \"NAME\": \"" << escape_json(input.name) << "\",\n";
      
      if(!input.label.empty()) {
        oss << "      \"LABEL\": \"" << escape_json(input.label) << "\",\n";
      }
      
      // Handle different input types
      struct input_serializer {
        std::ostringstream& oss;
        
        void operator()(const float_input& f) {
          oss << "      \"TYPE\": \"float\",\n";
          oss << "      \"MIN\": " << f.min << ",\n";
          oss << "      \"MAX\": " << f.max << ",\n";
          oss << "      \"DEFAULT\": " << f.def << "\n";
        }
        
        void operator()(const long_input& l) {
          oss << "      \"TYPE\": \"long\",\n";
          if(!l.values.empty()) {
            oss << "      \"VALUES\": [";
            for(size_t i = 0; i < l.values.size(); ++i) {
              oss << l.values[i];
              if(i < l.values.size() - 1) oss << ", ";
            }
            oss << "],\n";
          }
          if(!l.labels.empty()) {
            oss << "      \"LABELS\": [";
            for(size_t i = 0; i < l.labels.size(); ++i) {
              oss << "\"" << escape_json(l.labels[i]) << "\"";
              if(i < l.labels.size() - 1) oss << ", ";
            }
            oss << "],\n";
          }
          oss << "      \"DEFAULT\": " << l.def << "\n";
        }
        
        void operator()(const bool_input& b) {
          oss << "      \"TYPE\": \"bool\",\n";
          oss << "      \"DEFAULT\": " << (b.def ? "true" : "false") << "\n";
        }
        
        void operator()(const event_input&) {
          oss << "      \"TYPE\": \"event\"\n";
        }
        
        void operator()(const point2d_input& p) {
          oss << "      \"TYPE\": \"point2D\"";
          if(p.min) {
            oss << ",\n      \"MIN\": [" << (*p.min)[0] << ", " << (*p.min)[1] << "]";
          }
          if(p.max) {
            oss << ",\n      \"MAX\": [" << (*p.max)[0] << ", " << (*p.max)[1] << "]";
          }
          if(p.def) {
            oss << ",\n      \"DEFAULT\": [" << (*p.def)[0] << ", " << (*p.def)[1] << "]";
          }
          oss << "\n";
        }
        
        void operator()(const point3d_input& p) {
          oss << "      \"TYPE\": \"point3D\"";
          if(p.min) {
            oss << ",\n      \"MIN\": [" << (*p.min)[0] << ", " << (*p.min)[1] << ", " << (*p.min)[2] << "]";
          }
          if(p.max) {
            oss << ",\n      \"MAX\": [" << (*p.max)[0] << ", " << (*p.max)[1] << ", " << (*p.max)[2] << "]";
          }
          if(p.def) {
            oss << ",\n      \"DEFAULT\": [" << (*p.def)[0] << ", " << (*p.def)[1] << ", " << (*p.def)[2] << "]";
          }
          oss << "\n";
        }
        
        void operator()(const color_input& c) {
          oss << "      \"TYPE\": \"color\"";
          if(c.min) {
            oss << ",\n      \"MIN\": [" << (*c.min)[0] << ", " << (*c.min)[1] << ", " << (*c.min)[2] << ", " << (*c.min)[3] << "]";
          }
          if(c.max) {
            oss << ",\n      \"MAX\": [" << (*c.max)[0] << ", " << (*c.max)[1] << ", " << (*c.max)[2] << ", " << (*c.max)[3] << "]";
          }
          if(c.def) {
            oss << ",\n      \"DEFAULT\": [" << (*c.def)[0] << ", " << (*c.def)[1] << ", " << (*c.def)[2] << ", " << (*c.def)[3] << "]";
          }
          oss << "\n";
        }
        
        void operator()(const image_input&) {
          oss << "      \"TYPE\": \"image\"\n";
        }
        
        void operator()(const audio_input& a) {
          oss << "      \"TYPE\": \"audio\"";
          if(a.max > 0) {
            oss << ",\n      \"MAX\": " << a.max;
          }
          oss << "\n";
        }

        void operator()(const audioFFT_input& a)
        {
          oss << "      \"TYPE\": \"audioFFT\"";
          if(a.max > 0)
          {
            oss << ",\n      \"MAX\": " << a.max;
          }
          oss << "\n";
        }

        void operator()(const audioHist_input& a)
        {
          oss << "      \"TYPE\": \"audioHistogram\"";
          if(a.max > 0)
          {
            oss << ",\n      \"MAX\": " << a.max;
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
    if(!m_desc.passes.empty()) {
      oss << ",";
    }
    oss << "\n";
  }
  
  // Add passes if present
  if(!m_desc.passes.empty()) {
    oss << "  \"PASSES\": [\n";
    
    for(size_t i = 0; i < m_desc.passes.size(); ++i) {
      const auto& pass = m_desc.passes[i];
      oss << "    {\n";
      
      if(!pass.target.empty()) {
        oss << "      \"TARGET\": \"" << escape_json(pass.target) << "\",\n";
      }
      
      if(pass.persistent) {
        oss << "      \"PERSISTENT\": true,\n";
      }
      
      if(pass.float_storage) {
        oss << "      \"FLOAT\": true,\n";
      }
      
      if(pass.nearest_filter) {
        oss << "      \"FILTER\": \"NEAREST\",\n";
      }
      
      if(!pass.width_expression.empty()) {
        // Check if it's a numeric value or expression
        try {
          std::stod(pass.width_expression);
          oss << "      \"WIDTH\": " << pass.width_expression << ",\n";
        } catch(...) {
          oss << "      \"WIDTH\": \"" << escape_json(pass.width_expression) << "\",\n";
        }
      }
      
      if(!pass.height_expression.empty()) {
        // Check if it's a numeric value or expression
        try {
          std::stod(pass.height_expression);
          oss << "      \"HEIGHT\": " << pass.height_expression;
        } catch(...) {
          oss << "      \"HEIGHT\": \"" << escape_json(pass.height_expression) << "\"";
        }
      }
      
      // Remove trailing comma if last property
      auto str = oss.str();
      if(str.size() > 2 && str[str.size() - 2] == ',') {
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
  if(!m_fragment.empty()) {
    oss << m_fragment;
  }
  
  return oss.str();
}

void parser::parse_vsa()
{
  // VSA shaders are vertex shaders, so we process the vertex source

  auto [end, desc] = parse_isf_header(m_sourceVertex);
  m_desc = std::move(desc);

  std::string& fragWithoutISF = m_sourceVertex;
  fragWithoutISF.erase(0, end + 2);

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
  
  // Add standard VSA inputs to descriptor
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
    m_desc.inputs.insert(m_desc.inputs.begin(), vertexCount);
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
    m_desc.inputs.insert(m_desc.inputs.begin() + 1, primitiveType);
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

        auto imgRect_varname = "_" + val.name + "_imgRect";
        material_ubos += "vec4 " + imgRect_varname + ";\n";
        // See comment above regarding little dance to make spirv-cross happy
        globalvars += "vec4 ";
        globalvars += imgRect_varname;
        globalvars += " = isf_material_uniforms.";
        globalvars += imgRect_varname;
        globalvars += ";\n";

        sampler_binding++;
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
vec2 mouse = MOUSE.xy;
)_";

  // Add vertex input - using gl_VertexIndex for simplicity
  m_vertex += R"_(
// VSA Vertex Shader inputs
#define vertexId float(gl_VertexIndex)
#define vertexCount isf_material_uniforms.vertexCount

layout(location = 0) out vec4 v_color;
)_";
  // Add the processed VSA code
  m_vertex += vsaSource;

  m_fragment = GLSL45.versionPrelude;
  m_fragment += R"_(
layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 isf_FragColor;

void main() {
  isf_FragColor = v_color;
}
)_";
}

}
