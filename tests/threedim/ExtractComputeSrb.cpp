// L3 regression guard — split/threedim finding #13 (compute extraction rebuilds
// its SRB with a pipeline-incompatible layout).
//
// ComputeExtractionStrategy::createPipeline() builds the compute pipeline
// against an SRB whose layout is uniform@0 / src@1 / out@2. Its update() —
// invoked every frame by ExtractBuffer2 once the strategy exists — rebuilt
// m_srb with a binding set that OMITTED the Params uniform buffer and shifted
// every index down by one (src@0 / out@1). The rebound SRB is then
// layout-INCOMPATIBLE with the pipeline: on a validation backend the dispatch
// is flagged (device-lost); on a release backend the shader reads its Params
// block from binding 0 — now bound to the source storage buffer — yielding a
// garbage vertexCount/stride/offset and corrupt extracted output. The fix
// rebinds using the exact indices createPipeline() used (uniform@0/src@1/out@2).
//
// This drives the strategy end-to-end on a real offscreen QRhi: init(),
// update() (the frame-2 rebind that carries the bug), then a compute dispatch,
// and reads the extracted buffer back. The mesh is INTERLEAVED (position+normal
// in one buffer, so the normal attribute has byte_offset 12 != 0) — exactly the
// shape that selects the compute path in production. Post-fix the extracted
// floats equal the source normals; pre-fix the dispatch runs against the
// wrong-layout SRB and the readback does not match (RED — or the validation
// layer aborts the dispatch, which, as its own isolated executable, is equally
// a RED flip).
//
// Runs per available RHI backend; SKIPs where compute / non-uniform readback is
// unavailable (needs DISPLAY for a real GL/Vulkan device).

#include <score_test/App.hpp>

#include <Threedim/GeometryToBufferStrategies.hpp>

#include <Gfx/Graph/RenderState.hpp>

#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <cstring>
#include <vector>

// The backend list / probe helpers live in the gfx fixture.
#include <score_test/Gfx.hpp>

using namespace score::test::gfx;

namespace
{
struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string backend;
  bool ran = false;
  std::vector<float> extracted; // read back from the compute output buffer
};

// 4 interleaved vertices: {px,py,pz, nx,ny,nz}. Normals are distinctive so a
// wrong Params (reading vertexCount/offset from the src buffer) cannot
// accidentally reproduce them.
constexpr int kVerts = 4;
std::vector<float> makeInterleaved()
{
  std::vector<float> v;
  v.reserve(kVerts * 6);
  for(int i = 0; i < kVerts; ++i)
  {
    v.push_back(float(i));        // px
    v.push_back(float(i) + 0.5f); // py
    v.push_back(float(i) + 0.9f); // pz
    v.push_back(float(i) + 1.f);  // nx
    v.push_back(float(i) + 10.f); // ny
    v.push_back(float(i) + 100.f);// nz
  }
  return v;
}
} // namespace

TEST_CASE(
    "Compute extraction SRB rebuild stays layout-compatible with the pipeline",
    "[threedim][extract][f13]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Outcome out;
  out.backend = backend_name(backend);

  const std::vector<float> src = makeInterleaved();

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
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

    // Source storage buffer with the interleaved vertex data.
    const quint32 srcBytes = quint32(src.size() * sizeof(float));
    auto* srcBuffer = rhi.newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, srcBytes);
    if(!srcBuffer->create())
    {
      out.skipped = true;
      out.skip_reason = "source buffer create failed";
      st->destroy();
      return;
    }

    // Interleaved geometry: one buffer, stride 24, normal attribute at
    // byte_offset 12 (canDirectReference() == false -> the compute path).
    halp::dynamic_gpu_geometry mesh;
    mesh.vertices = kVerts;
    mesh.buffers.resize(1);
    mesh.buffers[0].handle = srcBuffer;
    mesh.buffers[0].byte_size = srcBytes;
    mesh.bindings.resize(1);
    mesh.bindings[0].stride = 6 * sizeof(float);
    mesh.input.resize(1);
    mesh.input[0].buffer = 0;
    mesh.input[0].byte_offset = 0;
    mesh.attributes.resize(1);
    mesh.attributes[0].binding = 0;
    mesh.attributes[0].semantic = halp::attribute_semantic::normal;
    mesh.attributes[0].format = halp::attribute_format::float3;
    mesh.attributes[0].byte_offset = 3 * sizeof(float);

    auto lookup
        = Threedim::findAttribute(mesh, halp::attribute_semantic::normal);
    if(!lookup)
    {
      out.skip_reason = "findAttribute failed";
      st->destroy();
      return;
    }
    REQUIRE_FALSE(lookup->canDirectReference()); // genuinely the compute path

    Threedim::ComputeExtractionStrategy strategy;
    if(!strategy.init(*st, rhi, mesh, *lookup, /*padToVec4*/ false))
    {
      out.skip_reason = "strategy init failed";
      st->destroy();
      return;
    }

    // Frame-2 rebind: this is the update() that reconstructed m_srb with the
    // (pre-fix) wrong layout. Exercising it is what makes the bug live.
    strategy.update(rhi, mesh, *lookup, /*padToVec4*/ false);

    const auto view = strategy.output();
    const auto outBytes = quint32(view.size);

    // One offscreen compute frame: upload src, dispatch the extraction, read
    // the output storage buffer back.
    QRhiCommandBuffer* cb{};
    if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    {
      out.skip_reason = "beginOffscreenFrame failed";
      delete srcBuffer;
      st->destroy();
      return;
    }

    QRhiResourceUpdateBatch* res = rhi.nextResourceUpdateBatch();
    res->uploadStaticBuffer(srcBuffer, src.data());

    // runCompute consumes `res` (submits the upload + dispatch) and hands back
    // a fresh batch in `res` for post-pass work (the readback).
    strategy.runCompute(rhi, *cb, res);

    QRhiBufferReadbackResult rb;
    res->readBackBuffer(view.buffer, 0, outBytes, &rb);
    cb->resourceUpdate(res);
    rhi.endOffscreenFrame();

    if(!rb.data.isEmpty())
    {
      const int n = int(rb.data.size() / sizeof(float));
      out.extracted.resize(n);
      std::memcpy(out.extracted.data(), rb.data.constData(), rb.data.size());
    }
    out.ran = true;

    strategy.release();
    delete srcBuffer;
    st->destroy();
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " reason=" << out.skip_reason);
  REQUIRE(out.ran);

  // Expected: the 3 normal floats of each of the 4 vertices, tightly packed.
  std::vector<float> expected;
  for(int i = 0; i < kVerts; ++i)
  {
    expected.push_back(src[i * 6 + 3]);
    expected.push_back(src[i * 6 + 4]);
    expected.push_back(src[i * 6 + 5]);
  }

  REQUIRE(out.extracted.size() == expected.size());
  for(std::size_t i = 0; i < expected.size(); ++i)
    CHECK(std::abs(out.extracted[i] - expected[i]) < 1e-4f);
}
