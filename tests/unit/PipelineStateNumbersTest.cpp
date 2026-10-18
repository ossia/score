// PIPELINE_STATE counts and masks are non-negative integers. A negative one
// wrapped to ~4 billion and a fractional one was truncated; DEPTH_BIAS, which
// QRhi takes as an int, was truncated too (shader board R12). They are named
// parse errors now. The shaders are raw raster pipelines: classic ISF refuses
// VERTEX_COUNT / INSTANCE_COUNT outright (F16), and a VERTEX_COUNT draw has no
// vertex inputs (R21).
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

namespace
{
constexpr auto k_vert = "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";

std::string raster_with(const std::string& state)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "PIPELINE_STATE": { )_" + state + R"_( },
  "INPUTS": []
}*/
void main() { color = vec4(1.0); }
)_";
}

::isf::parser parse_raster(const std::string& state)
{
  return ::isf::parser{
      k_vert, raster_with(state), 450, ::isf::parser::ShaderType::RawRasterPipeline};
}
}

TEST_CASE("PIPELINE_STATE integers are read as written", "[gfx][isf][pipeline][R12]")
{
  const auto p = parse_raster(
      R"_("VERTEX_COUNT": 6, "INSTANCE_COUNT": 2.0, "STENCIL_WRITE_MASK": 255, "DEPTH_BIAS": -2)_");
  const auto& st = p.data().default_state;
  CHECK(st.vertex_count.value_or(0) == 6u);
  CHECK(st.instance_count.value_or(0) == 2u);
  CHECK(st.stencil_write_mask.value_or(0) == 255u);
  REQUIRE(st.depth_bias.has_value());
  CHECK(*st.depth_bias == -2.f);
}

TEST_CASE(
    "a negative or fractional PIPELINE_STATE count, mask or DEPTH_BIAS is rejected",
    "[gfx][isf][pipeline][R12]")
{
  struct Case
  {
    std::string state, key;
  };
  const auto c = GENERATE(
      Case{R"_("VERTEX_COUNT": -3)_", "VERTEX_COUNT"},
      Case{R"_("VERTEX_COUNT": 2.5)_", "VERTEX_COUNT"},
      Case{R"_("INSTANCE_COUNT": -1)_", "INSTANCE_COUNT"},
      Case{R"_("STENCIL_READ_MASK": -1)_", "STENCIL_READ_MASK"},
      Case{R"_("STENCIL_WRITE_MASK": 0.5)_", "STENCIL_WRITE_MASK"},
      Case{R"_("DEPTH_BIAS": 0.5)_", "DEPTH_BIAS"});
  CAPTURE(c.state);
  CHECK_THROWS_WITH(parse_raster(c.state), Catch::Matchers::ContainsSubstring(c.key));
}

// DEPTH_LAYOUT is a conservative-depth promise; an unknown value emitted no
// qualifier at all (shader board R49).
TEST_CASE("DEPTH_LAYOUT is one of the four qualifiers", "[gfx][isf][R49]")
{
  const auto good = [](const std::string& v) {
    return R"_(/*{ "ISFVSN": "2.0", "DEPTH_LAYOUT": ")_" + v
           + R"_(", "INPUTS": [] }*/
void main() { gl_FragColor = vec4(1.0); gl_FragDepth = 0.5; }
)_";
  };
  for(std::string v : {"greater", "LESS", "Unchanged", "any"})
  {
    CAPTURE(v);
    ::isf::parser p{{}, good(v), 450, ::isf::parser::ShaderType::ISF};
    CHECK(p.fragment().find("out float gl_FragDepth") != std::string::npos);
  }
  for(std::string v : {"greater_equal", "", "depth_greater"})
  {
    CAPTURE(v);
    CHECK_THROWS_WITH(
        (::isf::parser{{}, good(v), 450, ::isf::parser::ShaderType::ISF}),
        Catch::Matchers::ContainsSubstring("DEPTH_LAYOUT"));
  }
}
