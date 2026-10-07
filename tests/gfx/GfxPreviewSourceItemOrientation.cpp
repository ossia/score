// A PreviewNode's host texture must have the same row order on every backend.
//
// It renders into a texture it does not own -- a QQuickRhiItem's colour buffer --
// and that texture has two consumers Qt treats differently:
//   * the 2D item picks its UV transform from isYUpInFramebuffer()
//     (qquickrhiitem.cpp), so it normalises whatever row order it is given;
//   * Qt Quick 3D's Texture.sourceItem takes the texture-provider branch
//     (qquick3dtexture.cpp), with no QSGLayer and no blit, so the raw texture
//     reaches the material.
// Hence the 3D consumer needs one fixed row order and nothing downstream will
// correct it, while the 2D consumer cannot reveal a mismatch.
//
// Three spaces appear below and must not be conflated:
//   * ISF author space  -- isf_FragNormCoord, origin bottom-left, an output
//     position; isf-gradient-y.fs writes green = y, so green is 1.0 at the top.
//   * delivered-image space -- a BackgroundNode readback, origin top-left,
//     normalised by InvertYRenderer. Used here only as a negative control.
//   * texture memory -- the raw QRhi readback, normalised by nothing. Row 0 is
//     what a sampler reaches at v = 0 on every backend. This is the space
//     Quick3D samples in and the only one the assertion is meaningful in.
//
// The assertion is the sign of the green difference between first and last row,
// not an RMSE: that is the geometry itself. Means are reported too, since an
// all-black run scores a difference of zero and must not read as consistent.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/PreviewNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
constexpr int kSize = 64;

QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}

QString apiSettingName(score::gfx::GraphicsApi api)
{
  const Gfx::Settings::GraphicsApis apis{};
  switch(api)
  {
    case score::gfx::Vulkan:
      return apis.Vulkan;
    case score::gfx::Metal:
      return apis.Metal;
    case score::gfx::D3D11:
      return apis.D3D11;
    case score::gfx::D3D12:
      return apis.D3D12;
    default:
      return apis.OpenGL;
  }
}

struct Band
{
  double firstRow = -1.; // mean green of the first texture row
  double lastRow = -1.;  // mean green of the last texture row
  double mean = -1.;     // mean green of the whole image
  double meanAlpha = -1.; // mean alpha of the whole image
  int width = 0;
  int height = 0;

  bool valid() const noexcept { return width > 0 && height > 0; }
  double delta() const noexcept { return lastRow - firstRow; }
};

double rowMeanChannel(const score::test::gfx::ReadbackImage& img, int y, int c)
{
  double acc = 0.;
  for(int x = 0; x < img.width; ++x)
    acc += img.at(x, y)[c];
  return acc / img.width;
}

Band bandOf(const score::test::gfx::ReadbackImage& img)
{
  Band b;
  if(!img.valid())
    return b;
  b.width = img.width;
  b.height = img.height;
  // One row in from each edge: the ISF triangle's own edge pixels can be
  // partially covered, and this test is about which END of the image is
  // bright, not about the extreme texel.
  b.firstRow = rowMeanChannel(img, 1, 1);
  b.lastRow = rowMeanChannel(img, img.height - 2, 1);
  double green = 0., alpha = 0.;
  for(int y = 0; y < img.height; ++y)
  {
    green += rowMeanChannel(img, y, 1);
    alpha += rowMeanChannel(img, y, 3);
  }
  b.mean = green / img.height;
  b.meanAlpha = alpha / img.height;
  return b;
}

/// Raw readback of a texture we own, with NO orientation normalisation: row 0
/// is the row a sampler reaches at v = 0. endOffscreenFrame() waits for the
/// GPU, so the result is complete when it returns.
score::test::gfx::ReadbackImage readTextureRaw(QRhi& rhi, QRhiTexture& tex)
{
  score::test::gfx::ReadbackImage img;
  QRhiReadbackResult res;
  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return img;
  auto* batch = rhi.nextResourceUpdateBatch();
  batch->readBackTexture(QRhiReadbackDescription{&tex}, &res);
  cb->resourceUpdate(batch);
  rhi.endOffscreenFrame();

  img.width = res.pixelSize.width();
  img.height = res.pixelSize.height();
  img.bytes = res.data;
  return img;
}

struct Outcome
{
  std::string skip;
  std::string error;
  bool built{};
  bool yUpInFramebuffer{};
  // What score::gfx::previewFirstRowIsPictureBottom() claims for this backend,
  // to be held against what was measured.
  bool firstRowIsPictureBottomSaysEngine{};
  Band preview;  // raw host texture: the space Quick3D samples in
  Band delivered; // BackgroundNode sink: origin top-left, the reference
};

Outcome run(score::gfx::GraphicsApi backend, const char* shader)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto& settings = app.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setRate(60.);
    settings.setGraphicsApi(apiSettingName(backend));

    std::string probed;
    if(!score::test::gfx::probe_api(backend, probed))
    {
      out.skip = std::string("RHI backend '") + score::test::gfx::backend_name(backend)
                 + "' unavailable";
      return;
    }

    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto isf = score::test::gfx::make_isf_node(corpus(shader));
    if(!isf.node)
    {
      out.error = "ISF node did not build: " + isf.error;
      return;
    }
    auto sinkOwned = std::make_unique<score::gfx::BackgroundNode>();
    sinkOwned->shared_readback = std::make_shared<QRhiReadbackResult>();
    sinkOwned->setSize(QSize{kSize, kSize});
    auto* sink = sinkOwned.get();

    const int32_t a = g.register_node(std::move(isf.node));
    const int32_t s = g.register_node(std::move(sinkOwned));
    {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      exec.setEdge(
          Gfx::port_index{a, 0}, Gfx::port_index{s, 0},
          Process::CableType::ImmediateGlutton);
      exec.endTick(st);
    }
    g.updateGraph();
    g.renderFrames(2);

    auto* sinkRl = sink->renderer();
    if(!sinkRl || !sink->renderState() || !sink->renderState()->rhi)
    {
      out.error = "sink render list never came up";
      return;
    }
    QRhi* rhi = sink->renderState()->rhi;
    out.yUpInFramebuffer = rhi->isYUpInFramebuffer();
    out.firstRowIsPictureBottomSaysEngine
        = score::gfx::previewFirstRowIsPictureBottom(*rhi);

    // The host texture, created exactly as QQuickRhiItemNode::sync() creates
    // its colour buffer: RenderTarget | UsedAsTransferSource.
    std::unique_ptr<QRhiTexture> tex{rhi->newTexture(
        QRhiTexture::RGBA8, QSize{kSize, kSize}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource)};
    REQUIRE(tex->create());
    std::unique_ptr<QRhiTextureRenderTarget> rt{
        rhi->newTextureRenderTarget({QRhiColorAttachment{tex.get()}})};
    std::unique_ptr<QRhiRenderPassDescriptor> rp{
        rt->newCompatibleRenderPassDescriptor()};
    rt->setRenderPassDescriptor(rp.get());
    REQUIRE(rt->create());

    Gfx::SharedOutputSettings set;
    set.width = kSize;
    set.height = kSize;
    set.rate = 60;
    auto previewOwned = std::make_unique<score::gfx::PreviewNode>(
        set, rhi, rt.get(), tex.get());
    auto* preview = previewOwned.get();
    const int32_t pv = g.register_preview_node(std::move(previewOwned));
    g.connect_preview_node(Gfx::EdgeSpec{{a, 0}, {pv, 0}});
    g.updateGraph();

    auto* previewRl = preview->renderer();
    if(!previewRl || previewRl->renderers.size() < 2)
    {
      out.error = "preview render list never came up";
      g.unregister_preview_node(pv);
      g.updateGraph();
      return;
    }
    out.built = true;

    // The host drives the preview, exactly as TextureSourceRenderer::render
    // does; renderFrames keeps the offscreen sink going.
    for(int i = 0; i < 4; i++)
    {
      g.renderFrames(1);
      QRhiCommandBuffer* cb{};
      if(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess)
      {
        previewRl->render(*cb);
        rhi->endOffscreenFrame();
      }
    }

    out.preview = bandOf(readTextureRaw(*rhi, *tex));
    {
      score::test::gfx::ReadbackImage img;
      const auto& rb = *sink->shared_readback;
      img.width = rb.pixelSize.width();
      img.height = rb.pixelSize.height();
      img.bytes = rb.data;
      out.delivered = bandOf(img);
    }

    g.unregister_preview_node(pv);
    g.updateGraph();
    g.unregister_node(s);
    g.unregister_node(a);
    g.updateGraph();
  });
  return out;
}

void report(const char* tag, const char* name, const Outcome& o)
{
  std::fprintf(
      stderr,
      "PREVIEW-ORIENT %-14s %-7s yUpInFramebuffer=%d engineSaysBottomUp=%d | "
      "raw preview texture: "
      "row0=%.1f rowN=%.1f delta=%+.1f meanG=%.1f meanA=%.1f | delivered sink: "
      "row0=%.1f rowN=%.1f delta=%+.1f meanG=%.1f meanA=%.1f\n",
      tag, name, int(o.yUpInFramebuffer),
      int(o.firstRowIsPictureBottomSaysEngine), o.preview.firstRow, o.preview.lastRow,
      o.preview.delta(), o.preview.mean, o.preview.meanAlpha, o.delivered.firstRow,
      o.delivered.lastRow, o.delivered.delta(), o.delivered.mean,
      o.delivered.meanAlpha);
}

// A green ramp spans the full 0..255 range, so a real measurement separates the
// two ends by far more than this. Anything smaller is a flat image, not a
// gradient, and must not be read as "the two backends agree".
constexpr double kMinSpread = 64.;
}

TEST_CASE(
    "a preview host texture has one row order on every backend",
    "[gfx][preview][orientation]")
{
  // Both backends in ONE case on purpose: the claim is a RELATION between them,
  // and a GENERATE-per-backend test can only ever assert on one at a time.
  const Outcome gl = run(score::gfx::OpenGL, "isf-gradient-y.fs");
  const Outcome vk = run(score::gfx::Vulkan, "isf-gradient-y.fs");

  report("gradient-y", "OpenGL", gl);
  report("gradient-y", "Vulkan", vk);

  if(!gl.skip.empty() || !vk.skip.empty())
    SKIP(
        "needs OpenGL and Vulkan side by side: " + gl.skip + " " + vk.skip);
  INFO("OpenGL error: " << gl.error << " / Vulkan error: " << vk.error);
  REQUIRE(gl.error.empty());
  REQUIRE(vk.error.empty());
  REQUIRE(gl.built);
  REQUIRE(vk.built);

  // ---------------------------------------------------------------------------
  // Negative controls FIRST. An all-black run (software stack with no DRI, a
  // pipeline that failed to build) produces delta == 0 on both backends, which
  // would otherwise satisfy the equality gate below while proving nothing.
  // ---------------------------------------------------------------------------
  REQUIRE(gl.preview.valid());
  REQUIRE(vk.preview.valid());
  REQUIRE(gl.delivered.valid());
  REQUIRE(vk.delivered.valid());
  INFO(
      "preview means: GL " << gl.preview.mean << " VK " << vk.preview.mean
                           << "; delivered means: GL " << gl.delivered.mean
                           << " VK " << vk.delivered.mean);
  REQUIRE(gl.preview.mean > 16.);
  REQUIRE(vk.preview.mean > 16.);
  REQUIRE(std::abs(gl.preview.delta()) > kMinSpread);
  REQUIRE(std::abs(vk.preview.delta()) > kMinSpread);

  // The upstream chain itself must already be backend-neutral in
  // delivered-image space, or the preview numbers below are measuring the wrong
  // bug. isf-gradient-y.fs is green=1 at the top of the picture, and a
  // BackgroundNode readback has its origin at the top left, so row 0 is bright
  // and the delta is negative on every backend.
  INFO(
      "delivered deltas: GL " << gl.delivered.delta() << " VK "
                              << vk.delivered.delta());
  REQUIRE(std::abs(gl.delivered.delta()) > kMinSpread);
  REQUIRE(std::abs(vk.delivered.delta()) > kMinSpread);
  CHECK(gl.delivered.delta() < 0.);
  CHECK(vk.delivered.delta() < 0.);

  // ---------------------------------------------------------------------------
  // THE GATE. Same sign => same row order in sampling space => the Quick3D
  // sourceItem consumer sees the same picture on both backends.
  // ---------------------------------------------------------------------------
  INFO(
      "raw preview texture delta: OpenGL " << gl.preview.delta() << ", Vulkan "
                                           << vk.preview.delta()
                                           << " (same sign required)");
  CHECK((gl.preview.delta() > 0.) == (vk.preview.delta() > 0.));

  // Direction anchor. OpenGL is the backend the Quick3D dome path was
  // calibrated against, and on OpenGL `isYUpInFramebuffer()` is true, so the
  // host texture's row 0 is the bottom of the picture -- where
  // isf_FragNormCoord.y, and therefore green, is 0. Pinning the absolute
  // direction stops a future "fix" that flips OpenGL to match a wrong Vulkan
  // from passing the gate above.
  CHECK(gl.preview.delta() > 0.);
  CHECK(vk.preview.delta() > 0.);

  // The row order is a decision two places have to agree on: the renderer
  // PreviewNode::createRenderer picks, and the `mirrorVertically` the item sets
  // so its 2D node re-flips where the backend's own order differs.
  // previewFirstRowIsPictureBottom() is the one statement of it; check it
  // against what was actually measured, on both backends, so the exported
  // answer cannot drift from the code it describes.
  //
  // There is no measurement of the Metal or D3D entries here: neither backend
  // can be created on this platform. Running this test on macOS or Windows is
  // what checks them.
  CHECK(gl.firstRowIsPictureBottomSaysEngine == (gl.preview.delta() > 0.));
  CHECK(vk.firstRowIsPictureBottomSaysEngine == (vk.preview.delta() > 0.));

  // No alpha case here on purpose. alpha-straight-red.fs, which asks for alpha
  // 0.5, reads back at 255 through BOTH the pass-through and the blit (see the
  // meanA fields printed above), because the ISF output pass is already opaque
  // by the time it reaches the host texture -- which is also why
  // GfxAlphaComposite has to observe alpha through a reader shader rather than
  // from a readback. A GL-vs-Vulkan alpha comparison on this path therefore
  // passes whatever the blit's clear colour is, and a test that cannot fail is
  // worse than no test.
}
