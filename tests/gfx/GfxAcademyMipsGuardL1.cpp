// Academy Texture Loader: no mip generation is queued for a 1x1 texture.
//
// Metal's generateMipmapsForTexture: aborts under API validation on a texture
// with a single level; score's own generateMips sites go through
// score::gfx::generateMipsIfAny since 5489f138a1, the academy's did not. The
// Metal abort cannot run here, so the batch the loader fills is inspected
// instead: it must hold no GenMips op for a 1x1 image (TextureResource path)
// or a 1x1 cube face (EquirectToCubemap path), and exactly one for a larger
// one (the positive control that the count sees the call at all).
//
// Registration: see the test_gfx_academy_mips_guard_l1 target.
#include <score_test/Gfx.hpp>

#include <Academy/Asset/TextureLoader.hpp>

#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <QImage>
#include <QTemporaryDir>
#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test::gfx;

namespace
{
struct StubOutput final : score::gfx::OutputNode
{
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return nullptr;
  }
  void setRenderer(std::shared_ptr<score::gfx::RenderList>) override { }
  score::gfx::RenderList* renderer() const override { return nullptr; }
  void startRendering() override { }
  void render() override { }
  void stopRendering() override { }
  bool canRender() const override { return false; }
  void onRendererChange() override { }
  void createOutput(score::gfx::OutputConfiguration) override { }
  void destroyOutput() override { }
  std::shared_ptr<score::gfx::RenderState> renderState() const override { return {}; }
  Configuration configuration() const noexcept override { return {}; }
};

struct Result
{
  bool skipped = false;
  std::string backend, error;
  int genMips = -1;
  int levels = 0;
  QSize size;
};

int countGenMips(QRhiResourceUpdateBatch* res, QRhiTexture* tex)
{
  auto* d = QRhiResourceUpdateBatchPrivate::get(res);
  int n = 0;
  for(int i = 0; i < d->activeTextureOpCount; ++i)
    if(d->textureOps[i].type == QRhiResourceUpdateBatchPrivate::TextureOp::GenMips
       && d->textureOps[i].dst == tex)
      ++n;
  return n;
}

Result run_loader(
    score::gfx::GraphicsApi api, const QImage& source, Academy::TextureKind kind,
    Academy::CubemapLayoutHint layout, int cubeFaceSize)
{
  Result r;
  r.backend = backend_name(api);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(api, probed))
    {
      r.skipped = true;
      return;
    }
    auto st = score::gfx::createRenderState(api, QSize{64, 64}, nullptr);
    if(!st || !st->rhi)
    {
      r.skipped = true;
      return;
    }
    QRhi& rhi = *st->rhi;

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("source.png"));
    if(!source.convertToFormat(QImage::Format_RGBA8888).save(path))
    {
      r.error = "could not write the source image";
      st->destroy();
      return;
    }

    {
      StubOutput sink;
      score::gfx::RenderList rl{sink, st};
      score::gfx::Port src{}, dst{};
      score::gfx::Edge edge{&src, &dst, Process::CableType::ImmediateGlutton};

      Academy::TextureLoader loader;
      loader.inputs.path.value = path.toStdString();
      loader.inputs.kind.value = kind;
      loader.inputs.cubemapLayout.value = layout;
      loader.inputs.encoding.value = Academy::ColorEncoding::Linear;
      loader.inputs.mips.value = true;
      loader.inputs.cubeFaceSize.value = cubeFaceSize;

      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      {
        r.error = "beginOffscreenFrame failed";
      }
      else
      {
        auto* res = rhi.nextResourceUpdateBatch();
        loader.init(rl, *res);
        loader.update(rl, *res, nullptr);
        loader.runInitialPasses(rl, *cb, res, edge);

        auto* tex = static_cast<QRhiTexture*>(loader.outputs.texture.texture.handle);
        if(tex)
        {
          r.genMips = countGenMips(res, tex);
          r.size = tex->pixelSize();
          r.levels = rhi.mipLevelsForSize(tex->pixelSize());
        }
        else
          r.error = "the loader published no texture";
        cb->resourceUpdate(res);
        rhi.endOffscreenFrame();
      }
      loader.release(rl);
    }
    st->destroy();
  });
  return r;
}

QImage solid(int w, int h)
{
  QImage img(w, h, QImage::Format_RGBA8888);
  img.fill(QColor(200, 100, 50, 255));
  return img;
}

void check(const Result& r, int expectedGenMips, QSize expectedSize)
{
  if(r.skipped)
    SKIP(r.backend + ": backend unavailable");
  INFO("backend=" << r.backend << " error='" << r.error << "' size=" << r.size.width()
                  << "x" << r.size.height() << " levels=" << r.levels);
  REQUIRE(r.error.empty());
  CHECK(r.size == expectedSize);
  CHECK(r.genMips == expectedGenMips);
}
}

TEST_CASE("Academy Texture Loader queues no mips for a 1x1 texture", "[gfx][academy][mips][l1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  using K = Academy::TextureKind;
  using L = Academy::CubemapLayoutHint;

  SECTION("1x1 image")
  {
    check(run_loader(api, solid(1, 1), K::Texture2D, L::Auto, 0), 0, {1, 1});
  }
  SECTION("4x4 image")
  {
    check(run_loader(api, solid(4, 4), K::Texture2D, L::Auto, 0), 1, {4, 4});
  }
  SECTION("equirectangular to 1x1 cube faces")
  {
    check(run_loader(api, solid(8, 4), K::Cubemap, L::Equirectangular, 1), 0, {1, 1});
  }
  SECTION("equirectangular to 4x4 cube faces")
  {
    check(run_loader(api, solid(8, 4), K::Cubemap, L::Equirectangular, 4), 1, {4, 4});
  }
}
