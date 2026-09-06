// A one-element vertex fallback must reach EVERY instance.
//
// The fallback pool hands out a single constant element. Utils.cpp binds it as
// a PerInstance stream with the element's own stride, so instance 1 steps one
// stride past the only element it has.
//
// Two phases through the SAME pipeline, so the only variable is the buffer:
//   baseline  - the real VertexFallbackPool entry (one element)
//   corrected - a two-element buffer holding the constant twice
// Both triangles must be white. If the fallback only reaches instance 0, the
// right-hand triangle is not.
#include <score_test/Gfx.hpp>
#include <Gfx/Graph/VertexFallbackDefaults.hpp>
#include <Gfx/Graph/VertexFallbackPool.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

TEST_CASE(
    "SceneResources fallback must reach every instance",
    "[SceneResources][gpu]")
{
  using namespace score::gfx;
  using namespace score::test::gfx;
  bool ran = false, baseline = false, corrected = false;
  std::string error;
  const auto api = platform_backends().front();
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(64, 32), nullptr);
    if(!state || !state->rhi) { error = "RHI unavailable"; if(state) state->destroy(); return; }
    auto& rhi = *state->rhi;
    {
      VertexFallbackPool pool;
      auto* batch = rhi.nextResourceUpdateBatch();
      auto spec = resolveVertexFallback(ossia::attribute_semantic::color0, "vec4", {});
      if(!spec) { error="fallback unresolved"; batch->release(); state->destroy(); return; }
      auto entry = pool.acquire(rhi, *batch, *spec);
      std::unique_ptr<QRhiBuffer> repeated(rhi.newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 32));
      repeated->create();
      const float white[8]{1,1,1,1,1,1,1,1};
      batch->uploadStaticBuffer(repeated.get(), 0, sizeof(white), white);
      std::unique_ptr<QRhiTexture> tex(rhi.newTexture(QRhiTexture::RGBA8, QSize(64,32), 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
      tex->create();
      std::unique_ptr<QRhiTextureRenderTarget> rt(rhi.newTextureRenderTarget({tex.get()}));
      std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
      rt->setRenderPassDescriptor(rp.get()); rt->create();
      std::unique_ptr<QRhiShaderResourceBindings> srb(rhi.newShaderResourceBindings());
      srb->setBindings({}); srb->create();
      const auto shaders = makeShaders(*state, QStringLiteral(R"_(#version 450
layout(location=0) in vec4 constantColor;
layout(location=0) out vec4 color;
void main() {
  vec2 p[3] = vec2[3](vec2(-.45,-.8), vec2(.45,-.8), vec2(0,.8));
  gl_Position = vec4(p[gl_VertexIndex] + vec2(gl_InstanceIndex == 0 ? -.5 : .5, 0),0,1);
  color = constantColor;
})_"), QStringLiteral(R"_(#version 450
layout(location=0) in vec4 color;
layout(location=0) out vec4 frag;
void main() { frag=color; }
)_"));
      std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi.newGraphicsPipeline());
      pipeline->setShaderStages({{QRhiShaderStage::Vertex,shaders.first}, {QRhiShaderStage::Fragment,shaders.second}});
      QRhiVertexInputLayout layout;
      layout.setBindings({{entry.stride, QRhiVertexInputBinding::PerInstance, 1}});
      layout.setAttributes({{0,0,QRhiVertexInputAttribute::Float4,0}});
      pipeline->setVertexInputLayout(layout); pipeline->setShaderResourceBindings(srb.get());
      pipeline->setRenderPassDescriptor(rp.get());
      if(!entry.buffer || !pipeline->create()) { error="pipeline failed"; batch->release(); }
      else {
        for(int phase=0;phase<2;++phase) {
          QRhiCommandBuffer* cb{};
          if(rhi.beginOffscreenFrame(&cb)!=QRhi::FrameOpSuccess) {error="frame failed";break;}
          cb->beginPass(rt.get(), Qt::black, {1.f,0}, batch); batch=nullptr;
          cb->setGraphicsPipeline(pipeline.get()); cb->setViewport({0,0,64,32}); cb->setShaderResources();
          QRhiCommandBuffer::VertexInput binding{phase ? repeated.get() : entry.buffer,0};
          cb->setVertexInput(0,1,&binding); cb->draw(3,2);
          QRhiReadbackResult result;
          auto* read = rhi.nextResourceUpdateBatch(); read->readBackTexture({tex.get()}, &result);
          cb->endPass(read); rhi.endOffscreenFrame(); rhi.finish();
          if(result.data.size()<64*32*4) {error="readback incomplete";break;}
          const auto* p = reinterpret_cast<const unsigned char*>(result.data.constData());
          const int left=p[(16*64+16)*4], right=p[(16*64+48)*4];
          std::printf("SceneResources SR01 %s left=%d right=%d expected=255,255\n", phase?"corrected":"baseline",left,right);
          if(phase) corrected=left>240 && right>240;
          else baseline=left>240 && right>240;
          ran=true;
        }
      }
      pool.release();
    }
    state->destroy();
  });
  INFO(error); REQUIRE(error.empty()); REQUIRE(ran);
  CHECK(corrected);
  CHECK(baseline);
}
