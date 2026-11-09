// REPEAT groups in a compute shader's PASSES: their passes are read in place,
// so every pass keeps the PASSINDEX of its authored position, and each group
// records the passes it repeats, its count and the group it is nested in.
// ITERATIONINDEX is defined in compute shaders and bakes on every backend,
// and is reserved as a resource name. Malformed groups are refused.
//
// Pure CPU: the parser and ShaderCache.
#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QString>
#include <QtGui/private/qshader_p.h>

#include <string>

namespace
{
const std::string k_pass
    = R"_({ "LOCAL_SIZE": [8, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } })_";

std::string csf(const std::string& passes, const std::string& resources = {})
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "steps", "TYPE": "long", "DEFAULT": 2 },
    { "NAME": "result", "TYPE": "storage", "ACCESS": "write_only", "LAYOUT": [ { "NAME": "v", "TYPE": "int[]" } ] })_"
         + resources + R"_(
  ],
  "PASSES": [ )_" + passes + R"_( ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(i < uint(result.v.length()))
    result.v[i] = PASSINDEX * 100 + ITERATIONINDEX;
}
)_";
}

std::string refusal(const std::string& source)
{
  try
  {
    ::isf::parser p{{}, source, 450, ::isf::parser::ShaderType::CSF};
  }
  catch(const isf::invalid_file& e)
  {
    return e.what();
  }
  return "accepted";
}

struct backend
{
  score::gfx::GraphicsApi api;
  QShaderVersion version;
};
}

TEST_CASE("REPEAT groups keep the passes in authored order", "[isf][csf][repeat]")
{
  const std::string passes = k_pass + R"_(,
    { "REPEAT": "$steps", "PASSES": [ )_" + k_pass + ", " + k_pass + R"_(,
      { "REPEAT": 3, "PASSES": [ )_" + k_pass + R"_( ] } ] },
    { "REPEAT": 0, "PASSES": [ )_" + k_pass + R"_( ] }, )_" + k_pass;
  ::isf::parser p{{}, csf(passes), 450, ::isf::parser::ShaderType::CSF};
  const auto d = p.data();
  REQUIRE(d.csf_passes.size() == 6);
  REQUIRE(d.csf_repeats.size() == 3);
  CHECK(d.csf_repeats[0].parent == -1);
  CHECK(d.csf_repeats[0].first == 1);
  CHECK(d.csf_repeats[0].end == 4);
  CHECK(d.csf_repeats[0].count == "$steps");
  CHECK(d.csf_repeats[1].parent == 0);
  CHECK(d.csf_repeats[1].first == 3);
  CHECK(d.csf_repeats[1].end == 4);
  CHECK(d.csf_repeats[1].count == "3");
  CHECK(d.csf_repeats[2].parent == -1);
  CHECK(d.csf_repeats[2].first == 4);
  CHECK(d.csf_repeats[2].end == 5);
  CHECK(d.csf_repeats[2].count == "0");
}

TEST_CASE("a shader without REPEAT has no group", "[isf][csf][repeat]")
{
  ::isf::parser p{{}, csf(k_pass + ", " + k_pass), 450, ::isf::parser::ShaderType::CSF};
  CHECK(p.data().csf_passes.size() == 2);
  CHECK(p.data().csf_repeats.empty());
}

TEST_CASE("ITERATIONINDEX bakes on every backend", "[isf][csf][repeat]")
{
  const auto b = GENERATE(
      backend{score::gfx::OpenGL, QShaderVersion(450)},
      backend{score::gfx::Vulkan, QShaderVersion(100)},
      backend{score::gfx::D3D11, QShaderVersion(50)},
      backend{score::gfx::D3D12, QShaderVersion(50)},
      backend{score::gfx::Metal, QShaderVersion(12)});
  ::isf::parser p{
      {}, csf(R"_({ "REPEAT": 4, "PASSES": [ )_" + k_pass + " ] }"), 450,
      ::isf::parser::ShaderType::CSF};
  auto cs = QString::fromStdString(p.compute_shader());
  cs.replace("ISF_LOCAL_SIZE_X", "8");
  cs.replace("ISF_LOCAL_SIZE_Y", "1");
  cs.replace("ISF_LOCAL_SIZE_Z", "1");
  const auto& [shader, error] = score::gfx::ShaderCache::get(
      b.api, b.version, cs.toUtf8(), QShader::ComputeStage);
  INFO("api=" << int(b.api) << "\n" << error.toStdString() << "\n" << cs.toStdString());
  CHECK(error.isEmpty());
  CHECK(shader.isValid());
}

TEST_CASE("malformed REPEAT groups are refused", "[isf][csf][repeat]")
{
  using Catch::Matchers::ContainsSubstring;
  CHECK_THAT(refusal(csf(R"_({ "REPEAT": 2 })_")), ContainsSubstring("non-empty PASSES"));
  CHECK_THAT(
      refusal(csf(R"_({ "REPEAT": 2, "PASSES": [] })_")), ContainsSubstring("non-empty PASSES"));
  CHECK_THAT(
      refusal(csf(R"_({ "REPEAT": 2, "PASSES": [ { "REPEAT": 1, "PASSES": [] } ] })_")),
      ContainsSubstring("non-empty PASSES"));
  CHECK_THAT(
      refusal(csf(R"_({ "REPEAT": -1, "PASSES": [ )_" + k_pass + " ] }")),
      ContainsSubstring("REPEAT is a count"));
  CHECK_THAT(
      refusal(csf(R"_({ "REPEAT": 1.5, "PASSES": [ )_" + k_pass + " ] }")),
      ContainsSubstring("REPEAT is a count"));
  CHECK_THAT(
      refusal(csf(R"_({ "REPEAT": "", "PASSES": [ )_" + k_pass + " ] }")),
      ContainsSubstring("REPEAT is a count"));
  CHECK_THAT(
      refusal(csf(R"_({ "REPEAT": 2, "PASSES": [ { "TARGET": "x" } ] })_")),
      ContainsSubstring("LOCAL_SIZE"));
}

TEST_CASE("ITERATIONINDEX is reserved", "[isf][csf][repeat]")
{
  CHECK_THAT(
      refusal(csf(k_pass, R"_(,
    { "NAME": "ITERATIONINDEX", "TYPE": "float", "DEFAULT": 0 })_")),
      !Catch::Matchers::Equals("accepted"));
}
