// Every pass of a COMPUTE_SHADER is a compute pass. One without LOCAL_SIZE
// was read as a raster (ISF) pass: it vanished from the dispatch list and the
// passes after it got another PASSINDEX (shader board C30). It is a compute
// pass with the default local size [16, 16, 1], in its authored position.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <string>

namespace
{
std::string csf(const std::string& passes)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "buf", "TYPE": "storage", "ACCESS": "read_write", "LAYOUT": [ { "NAME": "v", "TYPE": "float[]" } ] }
  ],
  "PASSES": [ )_" + passes + R"_( ]
}*/
void main() { }
)_";
}
}

TEST_CASE("a CSF pass without LOCAL_SIZE is a compute pass", "[gfx][csf][C30]")
{
  const std::string with = R"_({ "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } })_";
  const std::string without = R"_({ "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [2, 1, 1] } })_";

  SECTION("second pass")
  {
    ::isf::parser p{{}, csf(with + ", " + without), 450, ::isf::parser::ShaderType::CSF};
    const auto& d = p.data();
    REQUIRE(d.csf_passes.size() == 2);
    CHECK(d.passes.empty());
    CHECK(d.csf_passes[0].local_size == std::array<int, 3>{64, 1, 1});
    CHECK(d.csf_passes[1].local_size == std::array<int, 3>{16, 16, 1});
    CHECK(d.csf_passes[1].workgroups == std::array<int, 3>{2, 1, 1});
  }
  SECTION("first pass keeps PASSINDEX 0")
  {
    ::isf::parser p{{}, csf(without + ", " + with), 450, ::isf::parser::ShaderType::CSF};
    const auto& d = p.data();
    REQUIRE(d.csf_passes.size() == 2);
    CHECK(d.passes.empty());
    CHECK(d.csf_passes[0].workgroups == std::array<int, 3>{2, 1, 1});
    CHECK(d.csf_passes[1].local_size == std::array<int, 3>{64, 1, 1});
  }
  SECTION("no pass has one")
  {
    ::isf::parser p{{}, csf(without), 450, ::isf::parser::ShaderType::CSF};
    REQUIRE(p.data().csf_passes.size() == 1);
    CHECK(p.data().passes.empty());
    CHECK(p.compute_shader().find("ISF_LOCAL_SIZE_X") != std::string::npos);
  }
}

TEST_CASE("a CSF LOCAL_SIZE below 1 is rejected", "[gfx][csf][C30]")
{
  CHECK_THROWS_WITH(
      (::isf::parser{
          {}, csf(R"_({ "LOCAL_SIZE": [0, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } })_"),
          450, ::isf::parser::ShaderType::CSF}),
      Catch::Matchers::ContainsSubstring("LOCAL_SIZE must be"));
  CHECK_THROWS_WITH(
      (::isf::parser{
          {}, csf(R"_({ "LOCAL_SIZE": [8, -1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } })_"),
          450, ::isf::parser::ShaderType::CSF}),
      Catch::Matchers::ContainsSubstring("LOCAL_SIZE must be"));
}
