// The engine allocates a CSF storage buffer from its LAYOUT with
// calculateStorageBufferSize. That size must cover what the compiled shader
// addresses, or the shader writes past the end of the buffer (shader board
// C09). The reference is the std430 size the compiler reflects for the block
// (QShaderDescription::StorageBlock::knownSize, the fixed part: a trailing
// flexible array counts for nothing in both).
//
// Layout types that parse but used to be mis-sized:
//   * a two-dimensional array, vec4[2][3]: the inner [2] was read as part of
//     an unknown type name and guessed at 16 bytes;
//   * a constant-expression size, float[4 * 8]: read as one element;
// and LAYOUTs it cannot size at all, which the parser now rejects: a
// BUFFER_USAGE buffer with no LAYOUT (emitted as `buffer { };`), a size that is
// not an integer constant expression.
#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QString>
#include <QtGui/private/qshader_p.h>

#include <string>
#include <vector>

namespace
{
std::string csf(const std::string& layoutOrUsage)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "buf", "TYPE": "storage", "ACCESS": "read_write", )_"
         + layoutOrUsage + R"_( }
  ],
  "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main() { }
)_";
}

struct Sizes
{
  std::string error;
  int64_t engine{-1};
  int reflected{-1};
};

Sizes sizes(const std::string& src)
{
  Sizes s;
  try
  {
    ::isf::parser p{{}, src, 450, ::isf::parser::ShaderType::CSF};
    auto cs = QString::fromStdString(p.compute_shader());
    cs.replace("ISF_LOCAL_SIZE_X", "1");
    cs.replace("ISF_LOCAL_SIZE_Y", "1");
    cs.replace("ISF_LOCAL_SIZE_Z", "1");
    const auto& [shader, error] = score::gfx::ShaderCache::get(
        score::gfx::Vulkan, QShaderVersion(100), cs.toUtf8(), QShader::ComputeStage);
    if(!error.isEmpty())
    {
      s.error = error.toStdString() + "\n" + cs.toStdString();
      return s;
    }
    const auto desc = p.data();
    for(const auto& in : desc.inputs)
      if(auto* st = ossia::get_if<isf::storage_input>(&in.data))
        s.engine = score::gfx::calculateStorageBufferSize(st->layout, 0, desc);
    for(const auto& b : shader.description().storageBlocks())
      if(b.instanceName == "buf" || b.blockName.startsWith("buf"))
        s.reflected = b.knownSize;
  }
  catch(const std::exception& e)
  {
    s.error = std::string{"parse: "} + e.what();
  }
  return s;
}
}

TEST_CASE(
    "a CSF storage LAYOUT is allocated at least the size the shader addresses",
    "[gfx][csf][storage][C09]")
{
  const auto field = GENERATE(
      std::string{"vec4[2][3]"}, std::string{"float[4 * 8]"},
      std::string{"vec3[5]"}, std::string{"mat3[2]"});
  CAPTURE(field);
  const auto s = sizes(csf(R"_("LAYOUT": [ { "NAME": "a", "TYPE": "float" }, { "NAME": "v", "TYPE": ")_"
                           + field + R"_(" } ])_"));
  INFO(s.error);
  REQUIRE(s.error.empty());
  REQUIRE(s.reflected > 0);
  CHECK(s.engine >= s.reflected);
}

TEST_CASE(
    "a BUFFER_USAGE storage buffer is sized from its LAYOUT", "[gfx][csf][storage][C09]")
{
  const auto s = sizes(csf(
      R"_("BUFFER_USAGE": "dispatch_args", "LAYOUT": [ { "NAME": "xyz", "TYPE": "uint[3]" } ])_"));
  INFO(s.error);
  REQUIRE(s.error.empty());
  CHECK(s.engine >= 12);
  CHECK(s.engine >= s.reflected);
}

TEST_CASE(
    "a CSF storage LAYOUT the engine cannot size is rejected", "[gfx][csf][storage][C09]")
{
  const auto layout = GENERATE(
      // No LAYOUT: `buffer { };` and a zero-byte buffer.
      std::string{R"_("BUFFER_USAGE": "dispatch_args")_"},
      std::string{R"_("BUFFER_USAGE": "indirect_draw")_"},
      // Not integer constants, or a runtime-sized inner array.
      std::string{R"_("LAYOUT": [ { "NAME": "v", "TYPE": "float[N]" } ])_"},
      std::string{R"_("LAYOUT": [ { "NAME": "v", "TYPE": "float[4 / 0]" } ])_"},
      std::string{R"_("LAYOUT": [ { "NAME": "v", "TYPE": "float[0]" } ])_"},
      std::string{R"_("LAYOUT": [ { "NAME": "v", "TYPE": "float[4][]" } ])_"});
  CAPTURE(layout);
  const auto s = sizes(csf(layout));
  CHECK(s.error.starts_with("parse: "));
  const bool names_it = s.error.find("buf") != std::string::npos
                        || s.error.find("\"v\"") != std::string::npos;
  CHECK(names_it);
}

TEST_CASE("isf::parse_array_type", "[gfx][isf][storage][C09]")
{
  using V = std::vector<int64_t>;
  CHECK(isf::parse_array_type("float").extents.empty());
  CHECK(isf::parse_array_type("vec4[2][3]").base == "vec4");
  CHECK(isf::parse_array_type("vec4[2][3]").extents == V{2, 3});
  CHECK(isf::parse_array_type("float[4 * 8]").extents == V{32});
  CHECK(isf::parse_array_type("float[(1 + 2) * 4u]").extents == V{12});
  CHECK(isf::parse_array_type("uint[]").extents == V{0});
  CHECK(isf::parse_array_type("float[N]").extents == V{-1});
  CHECK(isf::parse_array_type("float[2 - 2]").extents == V{-1});
}
