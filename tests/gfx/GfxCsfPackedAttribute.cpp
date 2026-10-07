// A CSF reads a geometry attribute whatever its upstream layout: tightly
// packed float3 (stride 12, Pointcloud to mesh XYZ, Buffers to geometry), or
// interleaved (stride 24, XYZ_RGB), on the CPU or the GPU, declared vec3 or
// vec4, read_write or read_only.
//
// Five distinct points go through a CSF that writes
// position + (100, 200, 300) + 1000 * color; the colour is absent (zero) in the
// XYZ layouts. A vec4 declaration widens float3 with w = 1.
//
// A GPU attribute the repack cannot convert (bytes, at a stride that is not
// the std430 one) reads as zeros, one per vertex: neither misaligned bytes nor
// a buffer of another length.
#include "GfxMeshListSource.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/GfxBufferSink.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
constexpr int kPoints = 5;

QByteArray shader(const std::string& type, bool readOnly)
{
  const std::string out = readOnly ? "moved" : "position";
  std::string s = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "T", "ACCESS": "POS_ACCESS" },
        { "NAME": "color", "SEMANTIC": "color", "TYPE": "T", "ACCESS": "read_only", "REQUIRED": false }MOVED ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(i >= uint(ISF_READ(geo, position).length())) return;
  T p = ISF_READ(geo, position)[i];
  T c = ISF_READ(geo, color)[i];
  vec3 q = p.xyz + vec3(100.0, 200.0, 300.0) + 1000.0 * c.xyz;
  ISF_WRITE(geo, OUT)[i] = VALUE;
}
)";
  const auto replace = [&](const std::string& from, const std::string& to) {
    for(auto pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size()))
      s.replace(pos, from.size(), to);
  };
  replace(
      "MOVED", readOnly ? std::string(",\n        { \"NAME\": \"moved\", \"SEMANTIC\": "
                                      "\"custom\", \"TYPE\": \"T\", \"ACCESS\": \"write_only\" }")
                        : std::string());
  replace("POS_ACCESS", readOnly ? "read_only" : "read_write");
  replace("OUT", out);
  replace("VALUE", type == "vec4" ? "vec4(q, p.w)" : "q");
  replace("\"T\"", "\"" + type + "\"");
  replace("T p", type + " p");
  replace("T c", type + " c");
  return QByteArray::fromStdString(s);
}

struct Layout
{
  const char* name;
  uint32_t stride;
  bool color;
  bool wide; // xyzw positions
};

float pos(int i, int axis) { return float(i + 1) * (axis == 0 ? 1.f : axis == 1 ? 10.f : 0.5f); }
float col(int i, int axis) { return 0.01f * float((i + 1) * (axis + 1)); }

SourceMesh source(const Layout& l)
{
  SourceMesh m;
  m.vertices = kPoints;
  m.stride = l.stride;
  for(int i = 0; i < kPoints; i++)
  {
    for(int a = 0; a < 3; a++)
      m.data.push_back(pos(i, a));
    if(l.wide)
      m.data.push_back(1.f);
    if(l.color)
      for(int a = 0; a < 3; a++)
        m.data.push_back(col(i, a));
  }
  m.attributes.push_back(
      {ossia::attribute_semantic::position,
       l.wide ? ossia::geometry::attribute::float4 : ossia::geometry::attribute::float3,
       0});
  if(l.color)
    m.attributes.push_back(
        {ossia::attribute_semantic::color0, ossia::geometry::attribute::float3, 12});
  return m;
}

struct Result
{
  bool skipped = false;
  std::string error;
  std::vector<float> values;
};

Result runShader(
    score::gfx::GraphicsApi api, const SourceMesh& mesh, bool gpu, const QByteArray& code,
    const char* output)
{
  Result r;
  QTemporaryDir dir;
  const QString cs = dir.filePath("modifier.cs");
  {
    QFile f(cs);
    if(!dir.isValid() || !f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      r.error = "cannot write the shader";
      return r;
    }
    f.write(code);
  }
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addNode(
        std::make_unique<MeshListSourceNode>(std::vector<SourceMesh>{mesh}, gpu));
    const int csf = p.addCsf(cs);
    auto sink = attach_buffer_sink(p);
    if(src < 0 || csf < 0 || !sink.node)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(src, 0), p.geometryIn(csf, 0));
    p.wire(p.geometryOut(csf, 0), sink.node->sinkInput());
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    render_with_buffer_sink(p, *sink.node, 3);
    const auto& meshes = sink.node->harvest->meshes;
    if(meshes.empty())
    {
      r.error = "no geometry reached the sink";
      return;
    }
    for(const auto& a : meshes.front().attributes)
      if(a.name == output)
        r.values = as_floats(a.rb.data);
  });
  return r;
}

Result
run(score::gfx::GraphicsApi api, const Layout& l, bool gpu, const std::string& type,
    bool readOnly)
{
  return runShader(
      api, source(l), gpu, shader(type, readOnly), readOnly ? "moved" : "position");
}

// Writes, for each vertex, the length of the colour array it sees and the sum
// of its components.
const QByteArray kColorProbe = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color", "SEMANTIC": "color", "TYPE": "vec4", "ACCESS": "read_only", "REQUIRED": false } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(i >= uint(ISF_READ(geo, position).length())) return;
  uint n = uint(ISF_READ(geo, color).length());
  vec4 c = i < n ? ISF_READ(geo, color)[i] : vec4(-1.0);
  ISF_WRITE(geo, position)[i] = vec4(float(n), c.x + c.y + c.z + c.w, 0.0, 1.0);
}
)";
}

TEST_CASE(
    "a CSF reads packed and interleaved attributes at their own stride",
    "[gfx][csf][geometry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto layout = GENERATE(
      Layout{"XYZ", 12, false, false}, Layout{"XYZ_RGB", 24, true, false},
      Layout{"XYZW", 16, false, true});
  const bool gpu = GENERATE(false, true);
  const std::string type = GENERATE("vec3", "vec4");
  const bool readOnly = GENERATE(false, true);
  CAPTURE(backend_name(api), layout.name, gpu, type, readOnly);

  const auto r = run(api, layout, gpu, type, readOnly);
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  // vec3 and vec4 both have a 16-byte std430 array stride.
  REQUIRE(r.values.size() >= std::size_t(kPoints * 4));
  for(int i = 0; i < kPoints; i++)
  {
    CAPTURE(i);
    for(int a = 0; a < 3; a++)
    {
      CAPTURE(a);
      const float offset = a == 0 ? 100.f : a == 1 ? 200.f : 300.f;
      const float expected = pos(i, a) + offset + (layout.color ? 1000.f * col(i, a) : 0.f);
      CHECK(std::abs(r.values[i * 4 + a] - expected) < 1e-3f);
    }
    if(type == "vec4")
      CHECK(r.values[i * 4 + 3] == 1.f);
  }
}

TEST_CASE(
    "a GPU attribute the CSF cannot repack reads as zeros, one per vertex",
    "[gfx][csf][geometry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // position float3, then a byte colour, 20 bytes per vertex: not the 16-byte
  // std430 stride of the vec4 the shader declares, and not float data.
  SourceMesh m;
  m.vertices = kPoints;
  m.stride = 20;
  for(int i = 0; i < kPoints; i++)
  {
    for(int a = 0; a < 3; a++)
      m.data.push_back(pos(i, a));
    m.data.push_back(1.f); // the colour's bytes: 00 00 80 3f
    m.data.push_back(0.f);
  }
  m.attributes.push_back(
      {ossia::attribute_semantic::position, ossia::geometry::attribute::float3, 0});
  m.attributes.push_back(
      {ossia::attribute_semantic::color0, ossia::geometry::attribute::unormbyte4, 12});

  const auto r = runShader(api, m, true, kColorProbe, "position");
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.values.size() >= std::size_t(kPoints * 4));
  for(int i = 0; i < kPoints; i++)
  {
    CAPTURE(i);
    CHECK(r.values[i * 4 + 0] == float(kPoints));
    CHECK(r.values[i * 4 + 1] == 0.f);
  }
}
