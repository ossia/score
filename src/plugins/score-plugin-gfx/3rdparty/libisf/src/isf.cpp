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
  static const auto is_shadertoy = [](const std::string& str) {
    return str.find("void mainImage(") != std::string::npos;
  };
  static const auto is_glslsandbox = [](const std::string& str) {
    return str.find("uniform float time;") != std::string::npos
           || str.find("glslsandbox") != std::string::npos;
  };

  switch(t)
  {
    case ShaderType::Autodetect: {
      if(is_shadertoy(m_sourceFragment))
        parse_shadertoy();
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
      parse_shadertoy();
      break;
    }
    case ShaderType::GLSLSandBox: {
      parse_glsl_sandbox();
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
            inp.values.push_back(arr_value.get_integer_value());
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

template <
    typename Input_T, typename std::enable_if_t<Input_T::has_minmax::value>* = nullptr>
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

template <
    typename Input_T, typename std::enable_if_t<Input_T::has_default::value>* = nullptr>
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

  static const std::unordered_map<std::string, input_fun>& input_parse{[] {
    static std::unordered_map<std::string, input_fun> i;
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
  /* isf uniforms:
  uniform vec3 iResolution;
  uniform float iGlobalTime;
  uniform float iGlobalDelta;
  uniform float iGlobalFrame;
  uniform float iChannelTime[4];
  uniform vec4 iMouse;
  uniform vec4 iDate;
  uniform float iSampleRate;
  uniform vec3 iChannelResolution[4];
  uniform samplerXX iChanneli;
  */

  m_fragment += "uniform int PASSINDEX;\n";
  m_fragment += "uniform vec2 RENDERSIZE;\n";
  m_fragment += "uniform float TIME;\n";
  m_fragment += "uniform float TIMEDELTA;\n";
  m_fragment += "uniform float SAMPLERATE;\n";
  m_fragment += "uniform vec4 DATE;\n";
  m_fragment += "uniform vec2 MOUSE;\n";
  m_fragment += "uniform vec4 CHANNELTIME;\n";
  m_fragment += "uniform vec4 CHANNELRESOLUTION;\n";
  m_fragment += "out vec2 isf_FragNormCoord;\n";

  m_fragment += m_sourceFragment;

  boost::replace_all(m_fragment, "iGlobalTime", "TIME");        // float
  boost::replace_all(m_fragment, "iGlobalDelta", "TIMEDELTA");  // float
  boost::replace_all(m_fragment, "iGlobalFrame", "FRAMEINDEX"); // float -> int
  boost::replace_all(m_fragment, "iDate", "DATE");              // vec4
  boost::replace_all(m_fragment, "iSampleRate", "SAMPLERATE");  // float
  boost::replace_all(m_fragment, "iResolution", "RENDERSIZE");  // vec3 -> vec2

  boost::replace_all(m_fragment, "iMouse", "MOUSE");                         // vec4
  boost::replace_all(m_fragment, "iChannelTime", "CHANNELTIME");             // float[4]
  boost::replace_all(m_fragment, "iChannelResolution", "CHANNELRESOLUTION"); // vec3[4]
  boost::replace_all(m_fragment, "iChannel0", "CHANNEL"); // sampler2D / 3D

  m_fragment +=
      R"_(
            void main(void)
            {
              mainImage(gl_FragColor, gl_FragCoord.xy);
            }
            )_";
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

}
