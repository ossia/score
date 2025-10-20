#pragma once
#include <Crousti/GppCoroutines.hpp>
#include <avnd/concepts/all.hpp>
#include <fmt/format.h>
#include <string_view>

namespace gpp::qrhi
{
struct generate_shaders
{
  template <typename T, int N>
  using vec = T[N];

  static constexpr std::string_view field_type(float) { return "float"; }
  static constexpr std::string_view field_type(const float (&)[2]) { return "vec2"; }
  static constexpr std::string_view field_type(const float (&)[3]) { return "vec3"; }
  static constexpr std::string_view field_type(const float (&)[4]) { return "vec4"; }

  static constexpr std::string_view field_type(float*) { return "float"; }
  static constexpr std::string_view field_type(vec<float, 2>*) { return "vec2"; }
  static constexpr std::string_view field_type(vec<float, 3>*) { return "vec3"; }
  static constexpr std::string_view field_type(vec<float, 4>*) { return "vec4"; }

  static constexpr std::string_view field_type(int) { return "int"; }
  static constexpr std::string_view field_type(const int (&)[2]) { return "ivec2"; }
  static constexpr std::string_view field_type(const int (&)[3]) { return "ivec3"; }
  static constexpr std::string_view field_type(const int (&)[4]) { return "ivec4"; }

  static constexpr std::string_view field_type(int*) { return "int"; }
  static constexpr std::string_view field_type(vec<int, 2>*) { return "ivec2"; }
  static constexpr std::string_view field_type(vec<int, 3>*) { return "ivec3"; }
  static constexpr std::string_view field_type(vec<int, 4>*) { return "ivec4"; }

  static constexpr std::string_view field_type(uint32_t) { return "uint"; }
  static constexpr std::string_view field_type(const uint32_t (&)[2]) { return "uvec2"; }
  static constexpr std::string_view field_type(const uint32_t (&)[3]) { return "uvec3"; }
  static constexpr std::string_view field_type(const uint32_t (&)[4]) { return "uvec4"; }

  static constexpr std::string_view field_type(uint32_t*) { return "uint"; }
  static constexpr std::string_view field_type(vec<uint32_t, 2>*) { return "uvec2"; }
  static constexpr std::string_view field_type(vec<uint32_t, 3>*) { return "uvec3"; }
  static constexpr std::string_view field_type(vec<uint32_t, 4>*) { return "uvec4"; }

  static constexpr std::string_view field_type(avnd::xy_value auto) { return "vec2"; }

  static constexpr bool field_array(float) { return false; }
  static constexpr bool field_array(const float (&)[2]) { return false; }
  static constexpr bool field_array(const float (&)[3]) { return false; }
  static constexpr bool field_array(const float (&)[4]) { return false; }

  static constexpr bool field_array(float*) { return true; }
  static constexpr bool field_array(vec<float, 2>*) { return true; }
  static constexpr bool field_array(vec<float, 3>*) { return true; }
  static constexpr bool field_array(vec<float, 4>*) { return true; }

  static constexpr bool field_array(int) { return false; }
  static constexpr bool field_array(const int (&)[2]) { return false; }
  static constexpr bool field_array(const int (&)[3]) { return false; }
  static constexpr bool field_array(const int (&)[4]) { return false; }

  static constexpr bool field_array(int*) { return true; }
  static constexpr bool field_array(vec<int, 2>*) { return true; }
  static constexpr bool field_array(vec<int, 3>*) { return true; }
  static constexpr bool field_array(vec<int, 4>*) { return true; }

  static constexpr bool field_array(uint32_t) { return false; }
  static constexpr bool field_array(const uint32_t (&)[2]) { return false; }
  static constexpr bool field_array(const uint32_t (&)[3]) { return false; }
  static constexpr bool field_array(const uint32_t (&)[4]) { return false; }

  static constexpr bool field_array(uint32_t*) { return true; }
  static constexpr bool field_array(vec<uint32_t, 2>*) { return true; }
  static constexpr bool field_array(vec<uint32_t, 3>*) { return true; }
  static constexpr bool field_array(vec<uint32_t, 4>*) { return true; }

  static constexpr bool field_array(avnd::xy_value auto) { return false; }

  template <typename T>
  static constexpr std::string_view image_qualifier()
  {
    if constexpr(requires { T::readonly; })
      return "readonly";
    else if constexpr(requires { T::writeonly; })
      return "writeonly";
    else
      static_assert(T::readonly || T::writeonly);
  }

  struct write_input
  {
    std::string& shader;

    template <typename T>
    void operator()(const T& field)
    {
      shader += fmt::format(
          "layout(location = {}) in {} {};\n", T::location(), field_type(field.data),
          T::name());
    }
  };

  struct write_output
  {
    std::string& shader;

    template <typename T>
    void operator()(const T& field)
    {
      if constexpr(requires { field.location(); })
      {
        shader += fmt::format(
            "layout(location = {}) out {} {};\n", T::location(), field_type(field.data),
            T::name());
      }
    }
  };

  struct write_binding
  {
    std::string& shader;

    template <typename T>
    void operator()(const T& field)
    {
      shader += fmt::format(
          "  {} {}{};\n", field_type(field.value), T::name(),
          field_array(field.value) ? "[]" : "");
    }
  };

  struct write_bindings
  {
    std::string& shader;

    template <typename C>
    void operator()(const C& field)
    {
      if constexpr(requires { C::sampler2D; })
      {
        shader += fmt::format(
            "layout(binding = {}) uniform sampler2D {};\n\n", C::binding(), C::name());
      }
      else if constexpr(requires { C::image2D; })
      {
        shader += fmt::format(
            "layout(binding = {}, {}) {} uniform image2D {};\n\n", C::binding(),
            C::format(), image_qualifier<C>(), C::name());
      }
      else if constexpr(requires { C::ubo; })
      {
        shader += fmt::format(
            "layout({}, binding = {}) uniform {}\n{{\n",
            "std140" // TODO
            ,
            C::binding(), C::name());

        boost::pfr::for_each_field(field, write_binding{shader});

        shader += fmt::format("}};\n\n");
      }
      else if constexpr(requires { C::buffer; })
      {
        shader += fmt::format(
            "layout({}, binding = {}) buffer {}\n{{\n",
            "std140" // TODO
            ,
            C::binding(), C::name());

        boost::pfr::for_each_field(field, write_binding{shader});

        shader += fmt::format("}};\n\n");
      }
    }
  };

  template <typename T>
  std::string vertex_shader(const T& lay)
  {
    using namespace gpp::qrhi;
    std::string shader = "#version 450\n\n";

    if constexpr(requires { lay.vertex_input; })
      boost::pfr::for_each_field(lay.vertex_input, write_input{shader});
    else if constexpr(requires { typename T::vertex_input; })
      boost::pfr::for_each_field(typename T::vertex_input{}, write_input{shader});
    else
      boost::pfr::for_each_field(
          DefaultPipeline::layout::vertex_input{}, write_input{shader});

    if constexpr(requires { lay.vertex_output; })
      boost::pfr::for_each_field(lay.vertex_output, write_output{shader});
    else if constexpr(requires { typename T::vertex_output; })
      boost::pfr::for_each_field(typename T::vertex_output{}, write_output{shader});

    shader += "\n";

    if constexpr(requires { lay.bindings; })
      boost::pfr::for_each_field(lay.bindings, write_bindings{shader});
    else if constexpr(requires { typename T::bindings; })
      boost::pfr::for_each_field(typename T::bindings{}, write_bindings{shader});

    return shader;
  }

  template <typename T>
  std::string fragment_shader(const T& lay)
  {
    std::string shader = "#version 450\n\n";

    if constexpr(requires { lay.fragment_input; })
      boost::pfr::for_each_field(lay.fragment_input, write_input{shader});
    else if constexpr(requires { typename T::fragment_input; })
      boost::pfr::for_each_field(typename T::fragment_input{}, write_input{shader});

    if constexpr(requires { lay.fragment_output; })
      boost::pfr::for_each_field(lay.fragment_output, write_output{shader});
    else if constexpr(requires { typename T::fragment_output; })
      boost::pfr::for_each_field(typename T::fragment_output{}, write_output{shader});

    shader += "\n";

    if constexpr(requires { lay.bindings; })
      boost::pfr::for_each_field(lay.bindings, write_bindings{shader});
    else if constexpr(requires { typename T::bindings; })
      boost::pfr::for_each_field(typename T::bindings{}, write_bindings{shader});

    return shader;
  }

  template <typename T>
  std::string compute_shader(const T& lay)
  {
    std::string fstr = "#version 450\n\n";

    fstr += "layout(";
    if constexpr(requires { T::local_size_x(); })
    {
      fstr += fmt::format("local_size_x = {}, ", T::local_size_x());
    }
    if constexpr(requires { T::local_size_y(); })
    {
      fstr += fmt::format("local_size_y = {}, ", T::local_size_y());
    }
    if constexpr(requires { T::local_size_z(); })
    {
      fstr += fmt::format("local_size_z = {}, ", T::local_size_z());
    }

    // Remove the last ", "
    fstr.resize(fstr.size() - 2);
    fstr += ") in;\n\n";

    boost::pfr::for_each_field(lay.bindings, write_bindings{fstr});

    return fstr;
  }
};
}
