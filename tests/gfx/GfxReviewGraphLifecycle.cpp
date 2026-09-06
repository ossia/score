// G1-G4 from the 2026-09 graphics review (section 5): graph reconciliation and
// incremental node/edge changes.
//
// Measured out of tree by that review and never registered, so nothing in the
// suite covered them. Each case carries its own corrective experiment behind
// SCORE_REVIEW_FIX, which is what turns "the pixels are wrong" into "this
// specific piece of state was consumed and not restored" -- set it and the case
// passes, which localises the defect without patching the engine.
#include <score_test/Gfx.hpp>
#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <cstring>
#include <string>
using namespace score::test::gfx;
namespace {
QString corpus(const char* name) { return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + name; }
bool correction() { return qEnvironmentVariableIsSet("SCORE_REVIEW_FIX"); }
struct Result { bool skip{}; std::string reason, error; };
bool create(GfxPipeline& p, score::gfx::GraphicsApi api, Result& result)
{
  if(p.create(api)) return true;
  result.skip = p.skipped(); result.reason = p.skipReason(); result.error = p.error();
  return false;
}
}

TEST_CASE("GraphLifecycle-1 retained renderer does not lose a pending RT specification", "[GraphLifecycle][spec]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  Result result; QSize size; ReadbackImage image;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int a=p.addIsf(corpus("isf-solid-color.fs"));
    int b=p.addIsf(corpus("isf-passthrough-plain.fs"));
    int s=p.addSink({64,64});
    p.wire(p.imageOut(a),p.imageIn(b)); p.wire(p.imageOut(b),p.sinkInput(s));
    if(!create(p,api,result)) return;
    p.render(2);
    ossia::render_target_spec spec; spec.size=ossia::texture_size{23,17};
    setRenderTargetSpec(*p.isf(b),first_image_input(*p.isf(b)),spec);
    int c=p.addIsf(corpus("isf-solid-color.fs"));
    p.addEdgeIncremental(p.imageOut(c),p.sinkInput(s));
    // Minimal causal experiment: restore dirtiness that reconciliation consumed.
    if(correction()) for(auto& [rl,rn]:p.isf(b)->renderedNodes) rn->renderTargetSpecsChanged=true;
    p.render(2);
    auto* rl=p.sink(s)->renderer();
    auto rt=rl->renderTargetForInputPort(*p.imageIn(b));
    if(rt.texture) size=rt.texture->pixelSize();
    image=p.readback(s);
  });
  if(result.skip) SKIP(result.reason);
  REQUIRE(result.error.empty()); REQUIRE(image.valid());
  INFO("actual=" << size.width() << 'x' << size.height());
  CHECK(size == QSize(23,17));
}

TEST_CASE(
    "GraphLifecycle-2 unreachable input render targets are reclaimed",
    "[GraphLifecycle][retention]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  Result result; bool allocated{}, retained{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int a=p.addIsf(corpus("isf-solid-color.fs"));
    int b=p.addIsf(corpus("isf-passthrough-plain.fs"));
    int s=p.addSink({64,64});
    // B has an unconnected image input. It nevertheless receives a centralized RT.
    p.wire(p.imageOut(a),p.sinkInput(s)); p.wire(p.imageOut(b),p.sinkInput(s));
    if(!create(p,api,result)) return;
    p.render(2);
    auto* rl=p.sink(s)->renderer(); auto* port=p.imageIn(b);
    allocated=rl->renderTargetForInputPort(*port).texture!=nullptr;
    p.removeNodeIncremental(b);
    // Fixture retains B's C++ node object, so querying this key is not itself a UAF.
    if(correction()) rl->removeInputRenderTarget(port);
    retained=rl->renderTargetForInputPort(*port).texture!=nullptr;
    p.render(2);
  });
  if(result.skip) SKIP(result.reason);
  REQUIRE(result.error.empty()); REQUIRE(allocated); CHECK_FALSE(retained);
}

TEST_CASE(
    "GraphLifecycle-3 rebuild commits persistent registry initialization",
    "[GraphLifecycle][registry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  Result result; QByteArray bytes; bool ready{}, sameBuffer{}; int frameError{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p; int s=p.addSink({16,16});
    if(!create(p,api,result)) return;
    auto* old=p.sink(s)->renderer(); auto* rhi=old->state.rhi;
    if(!rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer)) { result.skip=true; result.reason="storage-buffer readback unavailable"; return; }
    auto* material=old->registry().buffer(score::gfx::GpuResourceRegistry::Arena::Material);
    ready=material && old->initialBatch(); if(!ready) return;
    // Establish deterministic initial bytes without submitting the pending seed.
    // This removes dependence on whether a driver happens to zero new VRAM.
    const float poison[4]={-7.f,-7.f,-7.f,-7.f};
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb)!=QRhi::FrameOpSuccess) { frameError=1; return; }
    auto* u=rhi->nextResourceUpdateBatch(); u->uploadStaticBuffer(material,0,sizeof poison,poison);
    cb->resourceUpdate(u); rhi->endOffscreenFrame(); rhi->finish();
    // Verify the poison actually landed before drawing any conclusion from
    // what comes back later. On macOS OpenGL this readback path returns zeros
    // even though ReadBackNonUniformBuffer is reported supported: the probe
    // below saw 0,0,0,0 -- neither the poison nor the seed -- so the case was
    // measuring the readback, not the registry seeding it is about.
    {
      QRhiReadbackResult pr;
      QRhiCommandBuffer* pcb{};
      if(rhi->beginOffscreenFrame(&pcb) != QRhi::FrameOpSuccess)
      {
        frameError = 3;
        return;
      }
      auto* pu = rhi->nextResourceUpdateBatch();
      pu->readBackBuffer(material, 0, sizeof poison, &pr);
      pcb->resourceUpdate(pu);
      rhi->endOffscreenFrame();
      rhi->finish();
      float probe[4]{};
      if(pr.data.size() == 16)
        std::memcpy(probe, pr.data.constData(), 16);
      if(pr.data.size() != 16 || probe[0] != -7.f)
      {
        result.skip = true;
        result.reason
            = "storage-buffer readback does not observe a host write here "
              "(poison wrote -7, read back "
              + (pr.data.size() == 16 ? std::to_string(probe[0])
                                      : std::string("nothing"))
              + "), so this case cannot measure registry seeding";
        return;
      }
    }

    if(correction()) old->flushInitialBatch();
    p.graph().createAllRenderLists(api); // no render between builds
    auto* fresh=p.sink(s)->renderer();
    sameBuffer=fresh->registry().buffer(score::gfx::GpuResourceRegistry::Arena::Material)==material;
    fresh->flushInitialBatch();
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    QRhiBufferReadbackResult rb;
#else
    QRhiReadbackResult rb;
#endif
    if(rhi->beginOffscreenFrame(&cb)!=QRhi::FrameOpSuccess) { frameError=2; return; }
    u=rhi->nextResourceUpdateBatch(); u->readBackBuffer(material,0,sizeof poison,&rb);
    cb->resourceUpdate(u); rhi->endOffscreenFrame(); rhi->finish(); bytes=rb.data;
  });
  if(result.skip) SKIP(result.reason);
  REQUIRE(result.error.empty()); REQUIRE(ready); REQUIRE(frameError==0); REQUIRE(sameBuffer);
  REQUIRE(bytes.size()==16); float rgba[4]{}; std::memcpy(rgba,bytes.constData(),16);
  INFO("material slot0 baseColor=" << rgba[0] << ',' << rgba[1] << ',' << rgba[2] << ',' << rgba[3]);
  for(float component:rgba) CHECK(component==1.f);
}

TEST_CASE(
    "GraphLifecycle-4 incremental input target honors mip allocation",
    "[GraphLifecycle][mips]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  Result result; bool hasMips{}, canGenerate{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int a=p.addIsf(corpus("isf-solid-color.fs")); int s=p.addSink({64,64});
    p.wire(p.imageOut(a),p.sinkInput(s)); if(!create(p,api,result)) return;
    p.render(2);
    int b=p.addIsf(corpus("isf-passthrough-plain.fs"));
    ossia::render_target_spec spec;
    spec.mipmap_mode=static_cast<decltype(spec.mipmap_mode)>(QRhiSampler::Linear);
    setRenderTargetSpec(*p.isf(b),first_image_input(*p.isf(b)),spec);
    p.addEdgeIncremental(p.imageOut(b),p.sinkInput(s));
    if(correction()) p.graph().createAllRenderLists(api); // reference full-build allocation
    auto rt=p.sink(s)->renderer()->renderTargetForInputPort(*p.imageIn(b));
    if(rt.texture) { hasMips=rt.texture->flags().testFlag(QRhiTexture::MipMapped); canGenerate=rt.texture->flags().testFlag(QRhiTexture::UsedWithGenerateMips); }
    p.render(2);
  });
  if(result.skip) SKIP(result.reason);
  REQUIRE(result.error.empty()); CHECK(hasMips); CHECK(canGenerate);
}
