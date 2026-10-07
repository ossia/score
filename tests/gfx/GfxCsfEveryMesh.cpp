// A CSF on a multi-mesh geometry (a multi-part Geometry Loader, Merge
// Geometries) processes every mesh, and outputs as many meshes as it receives.
//
// Two meshes of 3 and 4 points, each with its own index buffer, go through a
// CSF that adds 100 to position.x and stores its position array's length in w.
// Each output mesh carries its own offset positions, the length of its own
// mesh, its vertex count and the index buffer of the matching input mesh.
// Positions come as float4, or as packed float3 that each mesh repacks on
// the GPU before its passes.
#include "GfxMeshListSource.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/GfxBufferSink.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
// The modifier works on position in place; the generator variant reads it and
// writes a separate attribute, so its output buffers are sized per mesh too.
constexpr const char* kReadWrite = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  uint n = uint(ISF_READ(geo, position).length());
  if(i >= n) return;
  vec4 p = ISF_READ(geo, position)[i];
  ISF_WRITE(geo, position)[i] = vec4(p.x + 100.0, p.y, p.z, float(n));
}
)";

constexpr const char* kReadOnly = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "moved", "SEMANTIC": "custom", "TYPE": "vec4", "ACCESS": "write_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  uint n = uint(ISF_READ(geo, position).length());
  if(i >= n) return;
  vec4 p = ISF_READ(geo, position)[i];
  ISF_WRITE(geo, moved)[i] = vec4(p.x + 100.0, p.y, p.z, float(n));
}
)";

struct Mesh
{
  int vertices{};
  int indices{};
  std::vector<float> values; // the written attribute, xyzw per element
  std::vector<uint32_t> index;
};

struct Result
{
  bool skipped = false;
  std::string error;
  std::vector<Mesh> meshes;
};

// packed: float3 at stride 12, which the CSF repacks before its passes.
SourceMesh points(std::vector<float> xs, std::vector<uint32_t> index, bool packed)
{
  SourceMesh m;
  m.vertices = int(xs.size());
  m.stride = packed ? 12 : 16;
  for(float x : xs)
  {
    m.data.insert(m.data.end(), {x, 0.f, 0.f});
    if(!packed)
      m.data.push_back(1.f);
  }
  m.attributes.push_back(
      {ossia::attribute_semantic::position,
       packed ? ossia::geometry::attribute::float3 : ossia::geometry::attribute::float4, 0});
  m.index = std::move(index);
  return m;
}

Result run(score::gfx::GraphicsApi api, bool gpu, bool packed, bool readOnly)
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
    f.write(readOnly ? kReadOnly : kReadWrite);
  }
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addNode(std::make_unique<MeshListSourceNode>(
        std::vector<SourceMesh>{
            points({1.f, 2.f, 3.f}, {0, 1, 2}, packed),
            points({10.f, 11.f, 12.f, 13.f}, {3, 2, 1, 0, 3, 1}, packed)},
        gpu));
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
    for(const auto& mr : sink.node->harvest->meshes)
    {
      Mesh m{.vertices = mr.vertices, .indices = mr.indices};
      for(const auto& a : mr.attributes)
      {
        if(a.name == (readOnly ? "moved" : "position"))
          m.values = as_floats(a.rb.data);
        else if(a.name == "index")
        {
          m.index.resize(std::size_t(a.rb.data.size()) / sizeof(uint32_t));
          std::memcpy(m.index.data(), a.rb.data.constData(), m.index.size() * 4);
        }
      }
      r.meshes.push_back(std::move(m));
    }
  });
  return r;
}

void checkMesh(const Mesh& m, const std::vector<float>& xs, const std::vector<uint32_t>& index)
{
  CHECK(m.vertices == int(xs.size()));
  CHECK(m.indices == int(index.size()));
  REQUIRE(m.values.size() >= xs.size() * 4);
  for(std::size_t i = 0; i < xs.size(); i++)
  {
    CAPTURE(i);
    CHECK(m.values[i * 4] == xs[i] + 100.f);
    CHECK(m.values[i * 4 + 3] == float(xs.size()));
  }
  REQUIRE(m.index.size() >= index.size());
  CHECK(std::vector<uint32_t>(m.index.begin(), m.index.begin() + index.size()) == index);
}
}

TEST_CASE("a CSF processes every mesh of its input geometry", "[gfx][csf][geometry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool gpu = GENERATE(false, true);
  const bool packed = GENERATE(false, true);
  const bool readOnly = GENERATE(false, true);
  CAPTURE(backend_name(api), gpu, packed, readOnly);

  const auto r = run(api, gpu, packed, readOnly);
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.meshes.empty());
  {
    INFO("mesh 0");
    checkMesh(r.meshes[0], {1.f, 2.f, 3.f}, {0, 1, 2});
  }
  REQUIRE(r.meshes.size() == 2);
  {
    INFO("mesh 1");
    checkMesh(r.meshes[1], {10.f, 11.f, 12.f, 13.f}, {3, 2, 1, 0, 3, 1});
  }
}
