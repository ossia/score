#pragma once
#include <ossia/detail/variant.hpp>

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace isf
{
class invalid_file : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

struct event_input
{
};

struct bool_input
{
  using value_type = bool;
  using has_default = std::true_type;
  bool def{};
};

struct long_input
{
  using value_type = int64_t;
  using has_minmax = std::true_type;
  std::vector<ossia::variant<int64_t, double, std::string>> values;
  std::vector<std::string> labels;
  std::size_t def{}; // index of default value
};
struct float_input
{
  using value_type = double;
  using has_minmax = std::true_type;
  double min{0.};
  double max{1.};
  double def{0.5};
};

struct point2d_input
{
  using value_type = std::array<double, 2>;
  using has_minmax = std::true_type;
  std::optional<value_type> def{};
  std::optional<value_type> min{};
  std::optional<value_type> max{};
};

struct point3d_input
{
  using value_type = std::array<double, 3>;
  using has_minmax = std::true_type;
  std::optional<value_type> def{};
  std::optional<value_type> min{};
  std::optional<value_type> max{};
};

struct color_input
{
  using value_type = std::array<double, 4>;
  using has_minmax = std::true_type;
  std::optional<value_type> def{};
  std::optional<value_type> min{};
  std::optional<value_type> max{};
};

struct image_input
{
};

struct audio_input
{
  int max{};
};

struct audioFFT_input
{
  int max{};
};

struct audioHist_input
{
  int max{};
};

// CSF-specific input types
struct storage_input
{
  std::string access; // "read_only", "write_only", "read_write"

  struct layout_field
  {
    std::string name;
    std::string type;
  };

  std::vector<layout_field> layout;
};

struct texture_input
{
  // For sampled textures in CSF
};

struct csf_image_input
{
  std::string access; // "read_only", "write_only", "read_write"
  std::string format; // "RGBA8", "R32F", etc.
};

struct input
{
  using input_impl = ossia::variant<
      float_input, long_input, event_input, bool_input, color_input, point2d_input,
      point3d_input, image_input, audio_input, audioFFT_input, audioHist_input,
      storage_input, texture_input, csf_image_input>;

  std::string name;
  std::string label;

  input_impl data;
};

struct pass
{
  std::string target;
  bool persistent{};
  bool float_storage{};
  bool nearest_filter{};
  std::string width_expression{};
  std::string height_expression{};
};

struct descriptor
{
  enum Mode
  {
    ISF,
    VSA,
    CSF
  } mode{ISF};
  std::string description;
  std::string credits;
  std::vector<std::string> categories;
  std::vector<input> inputs;
  std::vector<pass> passes;
  std::vector<std::string> pass_targets;
  bool default_vertex_shader{};

  // For VSA
  int point_count{};
  std::string primitive_mode;
  std::string line_size;
  std::array<double, 4> background_color;

  // For CSF
  struct type_definition
  {
    std::string name;
    std::vector<storage_input::layout_field> layout;
  };
  std::vector<type_definition> types;

  struct dispatch_info
  {
    std::array<int, 3> local_size{16, 16, 1};
    std::string execution_type{"2D_IMAGE"}; // "2D_IMAGE", "1D_BUFFER", "MANUAL", etc.
    std::string target_resource;
    std::array<int, 3> workgroups{1, 1, 1}; // For MANUAL mode
  };
  std::vector<dispatch_info> csf_passes;
};

class parser
{
  std::string m_sourceVertex;
  std::string m_sourceFragment;
  std::string m_source_geometry_filter;
  int m_version{450};

  std::string m_vertex;
  std::string m_fragment;
  std::string m_geometry_filter;

  descriptor m_desc;

public:
  enum class ShaderType
  {
    Autodetect,
    ISF,
    ShaderToy,
    GLSLSandBox,
    GeometryFilter,
    VertexShaderArt,
    CSF
  };
  parser(
      std::string vert, std::string frag, int glslVersion = 450,
      ShaderType = ShaderType::Autodetect);
  explicit parser(std::string isf_geom_filter, ShaderType t);

  descriptor data() const;
  descriptor::Mode mode() const;
  std::string vertex() const;
  std::string fragment() const;
  std::string geometry_filter() const;
  std::string compute_shader() const;
  static std::pair<int, descriptor> parse_isf_header(std::string_view source);
  void parse_shadertoy_json(const std::string& json);

  std::string write_isf() const;

private:
  void parse_isf();
  void parse_shadertoy();
  void parse_glsl_sandbox();
  void parse_geometry_filter();
  void parse_vsa();
  void parse_csf();
};
}
