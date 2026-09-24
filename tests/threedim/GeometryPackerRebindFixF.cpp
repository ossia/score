// Repack attributes (GeometryPacker) on a source whose buffers arrive after the
// node was initialised (N81).
//
// A CPU mesh reaches the packer as QRhiBuffers uploaded by the avnd geometry
// input, so on the first frame the source handles are still null. The packer
// then bound its own output buffer as the load placeholder next to the store
// binding ("buffer used with different accesses within the same pass"), and
// the next frames' setBindings() without create() never replaced those
// bindings, so every dispatch read its own output.
//
// Drives PackedExtractionStrategy on a real offscreen QRhi: init() with null
// source handles, update() with the real buffer, one dispatch, readback. The
// packed buffer must interleave position | normal, and no "different accesses"
// warning may be emitted.

#include <score_test/App.hpp>

#include <Threedim/GeometryPacker.hpp>

#include <Gfx/Graph/RenderState.hpp>

#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

#include <score_test/Gfx.hpp>

using namespace score::test::gfx;

namespace
{
constexpr int kVerts = 5;

// Non-interleaved: kVerts positions, then kVerts normals.
std::vector<float> makeSource()
{
  std::vector<float> v;
  for(int i = 0; i < kVerts; ++i)
  {
    v.push_back(float(i));
    v.push_back(float(i) + 0.25f);
    v.push_back(float(i) + 0.5f);
  }
  for(int i = 0; i < kVerts; ++i)
  {
    v.push_back(float(i) + 10.f);
    v.push_back(float(i) + 20.f);
    v.push_back(float(i) + 30.f);
  }
  return v;
}

std::atomic_int g_accessWarnings{0};
QtMessageHandler g_previousHandler{};
void countAccessWarnings(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
{
  if(msg.contains(QStringLiteral("different accesses")))
    g_accessWarnings++;
  if(g_previousHandler)
    g_previousHandler(t, ctx, msg);
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string backend;
  bool ran = false;
  std::vector<float> packed;
  int stride = 0;
};
}

TEST_CASE(
    "Repack attributes packs a source whose buffers arrive after init",
    "[threedim][packer][fixF]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Outcome out;
  out.backend = backend_name(backend);
  const std::vector<float> src = makeSource();

  g_accessWarnings = 0;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    g_previousHandler = qInstallMessageHandler(countAccessWarnings);
    struct RestoreHandler
    {
      ~RestoreHandler() { qInstallMessageHandler(g_previousHandler); }
    } restoreHandler;

    std::string probed;
    if(!probe_api(backend, probed))
    {
      out.skipped = true;
      out.skip_reason = "backend unavailable headless";
      return;
    }

    auto st = score::gfx::createRenderState(backend, QSize{32, 32}, nullptr);
    if(!st || !st->rhi)
    {
      out.skipped = true;
      out.skip_reason = "no QRhi";
      return;
    }
    QRhi& rhi = *st->rhi;

    if(!rhi.isFeatureSupported(QRhi::Compute)
       || !rhi.isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
    {
      out.skipped = true;
      out.skip_reason = "compute / non-uniform readback unsupported";
      st->destroy();
      return;
    }

    const quint32 srcBytes = quint32(src.size() * sizeof(float));
    auto* srcBuffer = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
        srcBytes);
    REQUIRE(srcBuffer->create());

    halp::dynamic_gpu_geometry mesh;
    mesh.vertices = kVerts;
    mesh.buffers.resize(1);
    mesh.buffers[0].handle = nullptr;
    mesh.buffers[0].byte_size = srcBytes;
    mesh.bindings.resize(2);
    mesh.bindings[0].stride = 3 * sizeof(float);
    mesh.bindings[1].stride = 3 * sizeof(float);
    mesh.input.resize(2);
    mesh.input[0].buffer = 0;
    mesh.input[0].byte_offset = 0;
    mesh.input[1].buffer = 0;
    mesh.input[1].byte_offset = kVerts * 3 * sizeof(float);
    mesh.attributes.resize(2);
    mesh.attributes[0].binding = 0;
    mesh.attributes[0].semantic = halp::attribute_semantic::position;
    mesh.attributes[0].format = halp::attribute_format::float3;
    mesh.attributes[1].binding = 1;
    mesh.attributes[1].semantic = halp::attribute_semantic::normal;
    mesh.attributes[1].format = halp::attribute_format::float3;

    const Threedim::packed_attribute_spec specs[2]{
        {.location = halp::attribute_semantic::position, .pad_to_vec4 = false},
        {.location = halp::attribute_semantic::normal, .pad_to_vec4 = true}};

    Threedim::PackedExtractionStrategy strategy;
    REQUIRE(strategy.init(*st, rhi, mesh, specs));

    auto runFrame = [&](bool upload, QRhiBufferReadbackResult* rb) {
      QRhiCommandBuffer* cb{};
      REQUIRE(rhi.beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
      QRhiResourceUpdateBatch* res = rhi.nextResourceUpdateBatch();
      if(upload)
        res->uploadStaticBuffer(srcBuffer, src.data());
      strategy.runCompute(rhi, *cb, res);
      if(rb)
      {
        const auto view = strategy.output();
        res->readBackBuffer(view.buffer, 0, quint32(view.size), rb);
      }
      cb->resourceUpdate(res);
      rhi.endOffscreenFrame();
    };

    // Frame 1: the source buffers are not there yet.
    runFrame(false, nullptr);

    // Frame 2: the avnd input has uploaded them.
    mesh.buffers[0].handle = srcBuffer;
    strategy.update(rhi, mesh, specs);
    QRhiBufferReadbackResult rb;
    runFrame(true, &rb);

    // Frame 3: steady state, same bindings.
    strategy.update(rhi, mesh, specs);
    QRhiBufferReadbackResult rb3;
    runFrame(false, &rb3);

    if(!rb3.data.isEmpty())
    {
      out.packed.resize(rb3.data.size() / sizeof(float));
      std::memcpy(out.packed.data(), rb3.data.constData(), rb3.data.size());
    }
    out.stride = strategy.outputStride();
    out.ran = true;

    strategy.release();
    delete strategy.output().buffer;
    delete srcBuffer;
    st->destroy();
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  REQUIRE(out.ran);

  CHECK(g_accessWarnings == 0);
  REQUIRE(out.stride == 7 * int(sizeof(float)));

  std::vector<float> expected;
  for(int i = 0; i < kVerts; ++i)
  {
    for(int k = 0; k < 3; ++k)
      expected.push_back(src[i * 3 + k]);
    for(int k = 0; k < 3; ++k)
      expected.push_back(src[kVerts * 3 + i * 3 + k]);
    expected.push_back(1.f);
  }
  REQUIRE(out.packed.size() == expected.size());
  for(std::size_t i = 0; i < expected.size(); ++i)
  {
    CAPTURE(i);
    CHECK(std::abs(out.packed[i] - expected[i]) < 1e-5f);
  }
}
