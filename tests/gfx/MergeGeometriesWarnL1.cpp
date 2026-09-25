// Merge Geometries names each attribute it leaves untransformed, once.
//
// An input with a transform gets its float3/float4 per-vertex position, normal,
// tangent and bitangent baked; other formats (half, int, float2), per-instance
// attributes, non-storage GPU buffers and unaligned GPU layouts pass through
// as they are. That used to be silent. Each (attribute, format, reason) must
// now produce one warning per renderer, however often the node rebuilds, and
// attributes that are transformed must produce none.
//
// CPU cases drive the renderer without a GPU (inert RenderList storage, as
// MergeGeometriesTransformFixF.cpp). The GPU case feeds a CSF triangle with
// vec2 positions through a real pipeline.

#include "IsfTestCommon.hpp"

#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMutex>
#include <QStringList>

#include <cstring>
#include <memory>

using namespace score::gfx;
using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
QMutex g_mutex;
QStringList g_warnings;
QtMessageHandler g_previous{};

void capture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(msg.startsWith("Merge Geometries:"))
  {
    QMutexLocker l{&g_mutex};
    g_warnings.push_back(msg);
    return;
  }
  if(g_previous)
    g_previous(type, ctx, msg);
}

struct Capture
{
  Capture()
  {
    QMutexLocker l{&g_mutex};
    g_warnings.clear();
    g_previous = qInstallMessageHandler(capture);
  }
  ~Capture() { qInstallMessageHandler(g_previous); }
  QStringList warnings() const
  {
    QMutexLocker l{&g_mutex};
    return g_warnings;
  }
};

struct SinkRenderer final : NodeRenderer
{
  using NodeRenderer::NodeRenderer;
  void init(RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void release(RenderList&) override { }
  void removeOutputPass(RenderList&, Edge&) override { }
};

struct SinkNode final : Node
{
  SinkNode() { input.push_back(new Port{this, {}, Types::Geometry, {}}); }
  NodeRenderer* createRenderer(RenderList&) const noexcept override { return nullptr; }
};

ossia::geometry_spec makeCpuTriangle(
    decltype(ossia::geometry::attribute::format) posFormat,
    decltype(ossia::geometry::binding::classification) posClass)
{
  auto* bytes = new unsigned char[256]{};
  ossia::geometry g;
  g.buffers.push_back(
      {ossia::geometry::cpu_buffer{
           std::shared_ptr<void>(bytes, [](void* p) { delete[] (unsigned char*)p; }),
           256},
       true});
  g.bindings.push_back({16, posClass, 1});
  g.bindings.push_back({12, ossia::geometry::binding::per_vertex, 0});
  ossia::geometry::attribute pos;
  pos.binding = 0;
  pos.location = 0;
  pos.format = posFormat;
  pos.semantic = ossia::attribute_semantic::position;
  ossia::geometry::attribute nrm;
  nrm.binding = 1;
  nrm.location = 1;
  nrm.format = ossia::geometry::attribute::float3;
  nrm.semantic = ossia::attribute_semantic::normal;
  g.attributes.push_back(pos);
  g.attributes.push_back(nrm);
  g.input.push_back({0, 0});
  g.input.push_back({0, 64});
  g.vertices = 3;
  g.topology = ossia::geometry::triangles;

  ossia::geometry_spec spec;
  spec.meshes = std::make_shared<ossia::mesh_list>();
  spec.meshes->meshes.push_back(std::move(g));
  spec.filters = std::make_shared<ossia::geometry_filter_list>();
  return spec;
}

ossia::transform3d shifted(float tx)
{
  ossia::transform3d t;
  t.matrix[12] = tx;
  return t;
}

QStringList mergeCpu(
    decltype(ossia::geometry::attribute::format) posFormat,
    decltype(ossia::geometry::binding::classification) posClass, bool identity)
{
  alignas(64) static unsigned char rl_storage[16384]{};
  alignas(64) static unsigned char misc_storage[4096]{};
  auto& rl = *reinterpret_cast<RenderList*>(rl_storage);
  auto& batch = *reinterpret_cast<QRhiResourceUpdateBatch*>(misc_storage);
  auto& cb = *reinterpret_cast<QRhiCommandBuffer*>(misc_storage);
  QRhiResourceUpdateBatch* batch_ptr = &batch;

  Capture cap;
  MergeGeometriesNode merge;
  std::unique_ptr<NodeRenderer> r{merge.createRenderer(rl)};
  REQUIRE(r);
  SinkNode sinkNode;
  SinkRenderer sink{sinkNode};
  sinkNode.renderedNodes[&rl] = &sink;
  Edge edge{merge.output[0], sinkNode.input[0], Process::CableType::ImmediateGlutton};

  const auto spec = makeCpuTriangle(posFormat, posClass);
  const char key{};
  r->process(0, spec, &key);
  for(int i = 0; i < 3; i++)
  {
    r->process(0, identity ? ossia::transform3d{} : shifted(1.f + i));
    r->update(rl, batch, nullptr);
    r->runInitialPasses(rl, cb, batch_ptr, edge);
  }
  REQUIRE(sink.findGeometryByPort(0));
  r->release(rl);
  sinkNode.renderedNodes.clear();
  return cap.warnings();
}
}

TEST_CASE("Merge Geometries warns once per untransformed CPU attribute", "[gfx][merge][l1]")
{
  using A = ossia::geometry::attribute;
  using B = ossia::geometry::binding;

  SECTION("half3 positions")
  {
    const auto w = mergeCpu(A::half3, B::per_vertex, false);
    INFO(w.join("\n").toStdString());
    REQUIRE(w.size() == 1);
    CHECK(w[0].contains("position"));
    CHECK(w[0].contains("half3"));
    CHECK(w[0].contains("untransformed"));
  }

  SECTION("sint4 positions")
  {
    const auto w = mergeCpu(A::sint4, B::per_vertex, false);
    INFO(w.join("\n").toStdString());
    REQUIRE(w.size() == 1);
    CHECK(w[0].contains("sint4"));
  }

  SECTION("per-instance positions")
  {
    const auto w = mergeCpu(A::float3, B::per_instance, false);
    INFO(w.join("\n").toStdString());
    REQUIRE(w.size() == 1);
    CHECK(w[0].contains("position"));
    CHECK(w[0].contains("float3"));
    CHECK(w[0].contains("per-instance"));
  }

  SECTION("transformed attributes stay quiet")
  {
    const auto w = mergeCpu(A::float3, B::per_vertex, false);
    INFO(w.join("\n").toStdString());
    CHECK(w.empty());
  }

  SECTION("an identity transform bakes nothing and stays quiet")
  {
    const auto w = mergeCpu(A::half3, B::per_vertex, true);
    INFO(w.join("\n").toStdString());
    CHECK(w.empty());
  }
}

namespace
{
struct GpuShot
{
  bool skipped{};
  std::string skip_reason;
  std::string backend;
  std::string error;
  QStringList warnings;
  int mergeRenderers{};
};

GpuShot mergeGpu(score::gfx::GraphicsApi api, const char* producerFile)
{
  GpuShot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    Capture cap;
    GfxPipeline p;
    const int producer = p.addIsf(corpus(producerFile));
    const int merge = p.addNode(std::make_unique<score::gfx::MergeGeometriesNode>());
    const int raster
        = p.addRaster(corpus("mergegpu-b2-raster.vs"), corpus("mergegpu-b2-raster.fs"));
    const int sink = p.addSink({64, 64});
    p.wire(p.geometryOut(producer, 0), p.nodeGeometryIn(merge, 0));
    p.wire(p.nodeGeometryOut(merge, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      out.backend = p.backend();
      return;
    }
    out.backend = p.backend();

    auto* mergeNode = p.node(merge);
    for(int f = 0; f < 6; f++)
    {
      for(auto& [rl, r] : mergeNode->renderedNodes)
        r->process(0, shifted(0.1f * (f + 1)));
      p.render(1);
    }
    out.mergeRenderers = int(mergeNode->renderedNodes.size());
    out.error = p.error();
    out.warnings = cap.warnings();
  });
  return out;
}
}

TEST_CASE("Merge Geometries warns once per untransformed GPU attribute", "[gfx][merge][gpu][l1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  SECTION("vec2 positions in a storage buffer")
  {
    const auto shot = mergeGpu(backend, "l1merge-float2-producer.cs");
    if(shot.skipped)
      SKIP(shot.backend + ": " + shot.skip_reason);
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(why);
    INFO("backend=" << shot.backend << " error=" << shot.error);
    INFO(shot.warnings.join("\n").toStdString());
    REQUIRE(shot.mergeRenderers >= 1);
    REQUIRE(shot.warnings.size() == 1);
    CHECK(shot.warnings[0].contains("position"));
    CHECK(shot.warnings[0].contains("float2"));
  }

  SECTION("vec3 positions and normals are baked without a warning")
  {
    const auto shot = mergeGpu(backend, "mergegpu-b2-producer.cs");
    if(shot.skipped)
      SKIP(shot.backend + ": " + shot.skip_reason);
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(why);
    INFO("backend=" << shot.backend << " error=" << shot.error);
    INFO(shot.warnings.join("\n").toStdString());
    REQUIRE(shot.mergeRenderers >= 1);
    CHECK(shot.warnings.empty());
  }
}
