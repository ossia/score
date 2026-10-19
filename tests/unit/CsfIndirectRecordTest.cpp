// A CSF geometry with INDIRECT gets a generated command buffer, ISF_INDIRECT(geo),
// whose records are the native draw commands the GPU reads. The indexed record
// named its index count and first index `vertexCount` and `firstVertex`, the
// non-indexed spellings (shader board C50). It names them indexCount and
// firstIndex now, at the same word offsets; the non-indexed record keeps
// vertexCount and firstVertex.
#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QString>
#include <rhi/qshader.h>

#include <map>
#include <string>

namespace
{
std::string csf(const std::string& resources, const std::string& body)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ )_" + resources + R"_( ],
  "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main()
{
)_" + body + R"_(
}
)_";
}

std::string geometry(const std::string& name, bool indexed)
{
  return R"_({ "NAME": ")_" + name + R"_(", "TYPE": "geometry", "VERTEX_COUNT": "3",
      "INDIRECT": { "COUNT": 2)_" + (indexed ? std::string{R"_(, "INDEXED": true)_"} : std::string{}) + R"_( },
      "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" } ] })_";
}

struct Baked
{
  std::string error;
  // Command record member -> byte offset, per command buffer.
  std::map<std::string, std::map<std::string, int>> records;
};

Baked bake(const std::string& src)
{
  Baked r;
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
      r.error = error.toStdString() + "\n" + cs.toStdString();
      return r;
    }
    for(const auto& b : shader.description().storageBlocks())
      for(const auto& m : b.members)
        for(const auto& f : m.structMembers)
          r.records[m.name.toStdString()][f.name.toStdString()] = f.offset;
  }
  catch(const std::exception& e)
  {
    r.error = std::string{"parse: "} + e.what();
  }
  return r;
}
}

TEST_CASE(
    "an indexed INDIRECT record names its index fields", "[gfx][csf][indirect][C50]")
{
  const auto r = bake(csf(
      geometry("geo", true),
      "  ISF_INDIRECT(geo)[0].indexCount = 3u;\n"
      "  ISF_INDIRECT(geo)[0].instanceCount = 1u;\n"
      "  ISF_INDIRECT(geo)[0].firstIndex = 0u;\n"
      "  ISF_INDIRECT(geo)[0].baseVertex = -1;\n"
      "  ISF_INDIRECT(geo)[0].firstInstance = 0u;"));
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.records.count("geo_indirect"));
  const auto& f = r.records.at("geo_indirect");
  // QRhiDrawIndexedIndirectCommand.
  CHECK(f == std::map<std::string, int>{
             {"indexCount", 0},
             {"instanceCount", 4},
             {"firstIndex", 8},
             {"baseVertex", 12},
             {"firstInstance", 16}});
}

TEST_CASE(
    "a non-indexed INDIRECT record keeps its vertex fields", "[gfx][csf][indirect][C50]")
{
  const auto r = bake(csf(
      geometry("geo", false),
      "  ISF_INDIRECT(geo)[0].vertexCount = 3u;\n"
      "  ISF_INDIRECT(geo)[0].instanceCount = 1u;\n"
      "  ISF_INDIRECT(geo)[0].firstVertex = 0u;\n"
      "  ISF_INDIRECT(geo)[0].firstInstance = 0u;"));
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.records.count("geo_indirect"));
  // QRhiDrawIndirectCommand, then the unused fifth word.
  CHECK(r.records.at("geo_indirect") == std::map<std::string, int>{
             {"vertexCount", 0},
             {"instanceCount", 4},
             {"firstVertex", 8},
             {"firstInstance", 12},
             {"baseVertex", 16}});
}

// Each geometry emitted its own `#define ISF_INDIRECT(<name>) <name>_indirect`
// (and ISF_INDIRECT_COUNT): two INDIRECT geometries redefine the macro and the
// shader does not compile.
TEST_CASE(
    "two INDIRECT geometries each have their command buffer", "[gfx][csf][indirect]")
{
  const auto r = bake(csf(
      geometry("a", false) + ", " + geometry("b", true),
      "  ISF_INDIRECT(a)[0].vertexCount = 3u;\n"
      "  ISF_INDIRECT(b)[0].indexCount = 6u;"));
  INFO(r.error);
  REQUIRE(r.error.empty());
  CHECK(r.records.count("a_indirect"));
  CHECK(r.records.count("b_indirect"));
  CHECK(r.records.at("a_indirect").count("vertexCount"));
  CHECK(r.records.at("b_indirect").count("indexCount"));
}
