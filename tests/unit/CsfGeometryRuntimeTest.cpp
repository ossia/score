// RUNTIME on a CSF geometry input: true adds the two runtime bindings and the
// ISF_*_COUNT / ISF_*_INDEX / ISF_HAS_ATTRIBUTE accessors right before the
// geometry's attributes, and the generated compute shader bakes for every
// backend. A shader without RUNTIME gets none of it. RUNTIME is refused on a
// geometry that reads no upstream, as a non-boolean, and in a render pipeline.
//
// Pure CPU: the parser and ShaderCache, no device.
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
std::string csf(const std::string& geometry, const std::string& body)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "result", "TYPE": "storage", "ACCESS": "write_only", "LAYOUT": [ { "NAME": "v", "TYPE": "uint[]" } ] },
    )_" + geometry + R"_(
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
)_" + body + R"_(
}
)_";
}

const std::string k_runtime = R"_({ "NAME": "mesh", "TYPE": "geometry", "RUNTIME": true,
      "ATTRIBUTES": [
        { "NAME": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "weight", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": 1 } ] })_";

const std::string k_body = R"_(
  uint n = ISF_VERTEX_COUNT(mesh) + ISF_ELEMENT_COUNT(mesh) + ISF_INSTANCE_COUNT(mesh)
         + ISF_TRIANGLE_COUNT(mesh);
  uint e = ISF_INDEX(mesh, i) ^ ISF_VERTEX_INDEX(mesh, i) ^ ISF_INSTANCE_INDEX(mesh, i)
         ^ ISF_TRIANGLE_INDEX(mesh, i, 2u);
  bool w = ISF_HAS_ATTRIBUTE(mesh, weight) && ISF_HAS_ATTRIBUTE(mesh, position);
  if(i < uint(result.v.length()))
    result.v[i] = n + e + (w ? uint(ISF_READ(mesh, weight)[0]) : 0u);
)_";

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

QString compute(const std::string& source)
{
  ::isf::parser p{{}, source, 450, ::isf::parser::ShaderType::CSF};
  auto cs = QString::fromStdString(p.compute_shader());
  cs.replace("ISF_LOCAL_SIZE_X", "64");
  cs.replace("ISF_LOCAL_SIZE_Y", "1");
  cs.replace("ISF_LOCAL_SIZE_Z", "1");
  return cs;
}

struct backend
{
  score::gfx::GraphicsApi api;
  QShaderVersion version;
};
}

TEST_CASE("RUNTIME accessors bake on every backend", "[isf][csf][runtime]")
{
  const auto b = GENERATE(
      backend{score::gfx::OpenGL, QShaderVersion(450)},
      backend{score::gfx::Vulkan, QShaderVersion(100)},
      backend{score::gfx::D3D11, QShaderVersion(50)},
      backend{score::gfx::D3D12, QShaderVersion(50)},
      backend{score::gfx::Metal, QShaderVersion(12)});
  const QString cs = compute(csf(k_runtime, k_body));
  const auto& [shader, error] = score::gfx::ShaderCache::get(
      b.api, b.version, cs.toUtf8(), QShader::ComputeStage);
  INFO("api=" << int(b.api) << "\n" << error.toStdString() << "\n" << cs.toStdString());
  CHECK(error.isEmpty());
  CHECK(shader.isValid());
}

// The binding of the block named `block`, -1 when it is not declared.
static int bindingOf(const QString& cs, const QString& block)
{
  const int at = cs.indexOf(" buffer " + block + " ");
  if(at < 0)
    return -1;
  const int open = cs.lastIndexOf("layout(binding = ", at);
  if(open < 0)
    return -1;
  return cs.mid(open + 17, 6).section(',', 0, 0).trimmed().toInt();
}

TEST_CASE("RUNTIME binds two buffers before the geometry's attributes", "[isf][csf][runtime]")
{
  const QString cs = compute(csf(k_runtime, k_body)).simplified();
  INFO(cs.toStdString());
  const int result = bindingOf(cs, "result_buf");
  const int info = bindingOf(cs, "mesh_isf_rt_buf");
  CHECK(result >= 2);
  CHECK(info == result + 1);
  CHECK(bindingOf(cs, "mesh_isf_rt_index_buf") == info + 1);
  CHECK(bindingOf(cs, "mesh_position_in_buf") == info + 2);
  CHECK(bindingOf(cs, "mesh_weight_in_buf") == info + 3);
}

TEST_CASE("a geometry without RUNTIME gets no runtime binding", "[isf][csf][runtime]")
{
  const std::string plain = R"_({ "NAME": "mesh", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "position", "TYPE": "vec4", "ACCESS": "read_only" } ] })_";
  const QString cs
      = compute(csf(plain, "if(i < uint(result.v.length())) result.v[i] = uint(ISF_READ(mesh, position)[0].x);"))
            .simplified();
  INFO(cs.toStdString());
  CHECK(!cs.contains("isf_rt"));
  CHECK(bindingOf(cs, "mesh_position_in_buf") == bindingOf(cs, "result_buf") + 1);
}

TEST_CASE("RUNTIME is refused where it describes nothing", "[isf][csf][runtime]")
{
  using Catch::Matchers::ContainsSubstring;
  const std::string generator = R"_({ "NAME": "mesh", "TYPE": "geometry", "RUNTIME": true,
      "VERTEX_COUNT": 4,
      "ATTRIBUTES": [ { "NAME": "position", "TYPE": "vec4", "ACCESS": "write_only" } ] })_";
  CHECK_THAT(refusal(csf(generator, "")), ContainsSubstring("reads none"));

  const std::string notBool = R"_({ "NAME": "mesh", "TYPE": "geometry", "RUNTIME": 1,
      "ATTRIBUTES": [ { "NAME": "position", "TYPE": "vec4", "ACCESS": "read_only" } ] })_";
  CHECK_THAT(refusal(csf(notBool, "")), ContainsSubstring("true or false"));
}

TEST_CASE("RUNTIME false is the default", "[isf][csf][runtime]")
{
  const std::string off = R"_({ "NAME": "mesh", "TYPE": "geometry", "RUNTIME": false,
      "ATTRIBUTES": [ { "NAME": "position", "TYPE": "vec4", "ACCESS": "read_only" } ] })_";
  ::isf::parser p{{}, csf(off, ""), 450, ::isf::parser::ShaderType::CSF};
  CHECK(!ossia::get<isf::geometry_input>(p.data().inputs.at(1).data).runtime);
}

TEST_CASE("write_isf writes RUNTIME back", "[isf][csf][runtime]")
{
  ::isf::parser p{{}, csf(k_runtime, ""), 450, ::isf::parser::ShaderType::CSF};
  CHECK_THAT(p.write_isf(), Catch::Matchers::ContainsSubstring(R"_("RUNTIME": true)_"));
}

TEST_CASE("a render pipeline refuses RUNTIME", "[isf][runtime]")
{
  const std::string fs = R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [ { "NAME": "geo", "TYPE": "geometry", "RUNTIME": true, "ATTRIBUTES": [
    { "NAME": "w", "TYPE": "float", "ACCESS": "read_only" } ] } ]
}*/
void main() { color = vec4(1.0); }
)_";
  const std::string vs = "void main() { isf_vertShaderInit(); gl_Position = vec4(position, 1.0); isf_vertShaderFinish(); }\n";
  std::string why = "accepted";
  try
  {
    ::isf::parser p{vs, fs, 450, ::isf::parser::ShaderType::RawRasterPipeline};
  }
  catch(const isf::invalid_file& e)
  {
    why = e.what();
  }
  CHECK_THAT(why, Catch::Matchers::ContainsSubstring("only supported in compute shaders"));
}
