// Academy Texture Loader: each cubemap face holds its own direction.
//
// Equirectangular (EquirectToCubemap): the synthetic 2:1 panorama is coloured
// by direction, with |latitude| > 30 degrees giving +Y (top rows) / -Y (bottom
// rows) and the equator split into four longitude quadrants centred on
// -Z (u = 0.5), +X (u = 0.75), +Z (u = 0 / 1) and -X (u = 0.25), from the
// mapping the Cubemap Loader and cubemap_view share,
// u = atan2(x, -z) / 2pi + 0.5, v = 0.5 - asin(y) / pi.
// Each face centre must read its own colour. Before the fix all six faces read
// the -Z colour (one Dynamic UBO written six times in one frame), and +Y / -Y
// were swapped (the panorama was sampled upside down). The top and bottom rows
// of each side face must also read +Y / -Y: row 0 of a side face is up.
//
// Cross / strip layouts: every face region of the source image is a solid
// colour with a white texel at its top-left corner. Every face must come back
// with its own colour and the white corner at texel (0, 0), at the source face
// size (Cube face size 0, the default) and at a requested size. Before the fix
// the raw image was uploaded as six consecutive chunks, reading past the end
// of the decoded buffer. The vertical cross stores its -Z cell rotated by 180
// degrees, as the Cubemap Loader reads it, so that cell's white marker sits at
// its bottom-right corner and still has to come back at texel (0, 0).
//
// Registration: see the test_gfx_academy_texture_loader_cube_fixa target.
#include <score_test/Gfx.hpp>

#include <Academy/Asset/TextureLoader.hpp>

#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
using RGBA = std::array<uint8_t, 4>;

constexpr RGBA kFace[6]{
    {255, 0, 0, 255},   // +X
    {0, 255, 255, 255}, // -X
    {0, 255, 0, 255},   // +Y
    {255, 0, 255, 255}, // -Y
    {0, 0, 255, 255},   // +Z
    {255, 255, 0, 255}, // -Z
};
constexpr RGBA kWhite{255, 255, 255, 255};
const char* const kFaceName[6]{"+X", "-X", "+Y", "-Y", "+Z", "-Z"};

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

struct Faces
{
  bool skipped = false;
  std::string backend;
  std::string error;
  bool isCube = false;
  int faceSize = 0;
  std::array<QByteArray, 6> data;

  RGBA at(int face, int x, int y) const
  {
    const auto* p = reinterpret_cast<const uint8_t*>(data[face].constData())
                    + (size_t(y) * faceSize + x) * 4;
    return {p[0], p[1], p[2], p[3]};
  }
};

struct LoaderSetup
{
  Academy::TextureKind kind = Academy::TextureKind::Cubemap;
  Academy::CubemapLayoutHint layout = Academy::CubemapLayoutHint::Auto;
  int cubeFaceSize = 0;
};

Faces run_loader(
    score::gfx::GraphicsApi api, const QImage& source, const LoaderSetup& setup)
{
  Faces f;
  f.backend = backend_name(api);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(api, probed))
    {
      f.skipped = true;
      return;
    }
    auto st = score::gfx::createRenderState(api, QSize{64, 64}, nullptr);
    if(!st || !st->rhi)
    {
      f.skipped = true;
      return;
    }
    QRhi& rhi = *st->rhi;

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("source.png"));
    if(!source.convertToFormat(QImage::Format_RGBA8888).save(path))
    {
      f.error = "could not write the source image";
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
      loader.inputs.kind.value = setup.kind;
      loader.inputs.cubemapLayout.value = setup.layout;
      loader.inputs.encoding.value = Academy::ColorEncoding::Linear;
      loader.inputs.mips.value = false;
      loader.inputs.cubeFaceSize.value = setup.cubeFaceSize;

      std::array<QRhiReadbackResult, 6> rb;
      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      {
        f.error = "beginOffscreenFrame failed";
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
          f.isCube = tex->flags().testFlag(QRhiTexture::CubeMap);
          f.faceSize = tex->pixelSize().width();
          for(int face = 0; face < 6 && f.isCube; ++face)
          {
            QRhiReadbackDescription desc{tex};
            desc.setLayer(face);
            res->readBackTexture(desc, &rb[face]);
          }
        }
        cb->resourceUpdate(res);
        rhi.endOffscreenFrame();

        if(!tex)
          f.error = "the loader published no texture";
        else if(f.isCube)
          for(int face = 0; face < 6; ++face)
            f.data[face] = rb[face].data;
      }
      loader.release(rl);
    }
    st->destroy();
  });
  return f;
}

// The converter's mapping: u = atan2(x, -z) / 2pi + 0.5, v = 0.5 - lat / pi.
QImage make_equirect(int w, int h)
{
  QImage img(w, h, QImage::Format_RGBA8888);
  for(int y = 0; y < h; ++y)
  {
    const double v = (y + 0.5) / h;
    for(int x = 0; x < w; ++x)
    {
      const double u = (x + 0.5) / w;
      int face;
      if(v < 1. / 3.)
        face = 2;
      else if(v > 2. / 3.)
        face = 3;
      else if(u < 0.125 || u >= 0.875)
        face = 4;
      else if(u < 0.375)
        face = 1;
      else if(u < 0.625)
        face = 5;
      else
        face = 0;
      const auto& c = kFace[face];
      img.setPixelColor(x, y, QColor(c[0], c[1], c[2], c[3]));
    }
  }
  return img;
}

QImage make_layout(Academy::CubemapLayoutHint layout, int F)
{
  int cols = 0, rows = 0;
  std::array<int, 6> cx{}, cy{};
  switch(layout)
  {
    case Academy::CubemapLayoutHint::HorizontalCross:
      cols = 4, rows = 3, cx = {2, 0, 1, 1, 1, 3}, cy = {1, 1, 0, 2, 1, 1};
      break;
    case Academy::CubemapLayoutHint::VerticalCross:
      cols = 3, rows = 4, cx = {2, 0, 1, 1, 1, 1}, cy = {1, 1, 0, 2, 1, 3};
      break;
    case Academy::CubemapLayoutHint::HorizontalStrip:
      cols = 6, rows = 1, cx = {0, 1, 2, 3, 4, 5};
      break;
    case Academy::CubemapLayoutHint::VerticalStrip:
      cols = 1, rows = 6, cy = {0, 1, 2, 3, 4, 5};
      break;
    default:
      return {};
  }
  QImage img(cols * F, rows * F, QImage::Format_RGBA8888);
  img.fill(QColor(0, 0, 0, 255));
  for(int face = 0; face < 6; ++face)
  {
    const auto& c = kFace[face];
    for(int y = 0; y < F; ++y)
      for(int x = 0; x < F; ++x)
        img.setPixelColor(
            cx[face] * F + x, cy[face] * F + y, QColor(c[0], c[1], c[2], c[3]));
    const bool rotated = layout == Academy::CubemapLayoutHint::VerticalCross && face == 5;
    const int corner = rotated ? F - 1 : 0;
    img.setPixelColor(
        cx[face] * F + corner, cy[face] * F + corner, QColor(255, 255, 255, 255));
  }
  return img;
}

void require_faces(const Faces& f, int expectedSize)
{
  if(f.skipped)
    SKIP(f.backend + ": backend unavailable");
  INFO("backend=" << f.backend << " error='" << f.error << "'");
  REQUIRE(f.error.empty());
  REQUIRE(f.isCube);
  REQUIRE(f.faceSize == expectedSize);
  for(int face = 0; face < 6; ++face)
    REQUIRE(f.data[face].size() == qsizetype(expectedSize) * expectedSize * 4);
}
}

TEST_CASE(
    "academy equirect-to-cubemap renders every face from its own direction",
    "[gfx][academy][cubemap][equirect]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  LoaderSetup setup;
  setup.layout = GENERATE(
      Academy::CubemapLayoutHint::Equirectangular, Academy::CubemapLayoutHint::Auto);
  setup.cubeFaceSize = 32;
  CAPTURE(int(setup.layout));

  const Faces f = run_loader(api, make_equirect(256, 128), setup);
  require_faces(f, 32);

  const int F = f.faceSize;
  for(int face = 0; face < 6; ++face)
  {
    INFO("face " << kFaceName[face]);
    CHECK(near(f.at(face, F / 2, F / 2), kFace[face], 2));
    CHECK(near(f.at(face, F / 2 - 1, F / 2 - 1), kFace[face], 2));
  }
  for(int face : {0, 1, 4, 5})
  {
    INFO("side face " << kFaceName[face]);
    CHECK(near(f.at(face, F / 2, 0), kFace[2], 2));
    CHECK(near(f.at(face, F / 2, F - 1), kFace[3], 2));
  }
}

TEST_CASE(
    "academy cross and strip cubemaps are sliced into their six faces",
    "[gfx][academy][cubemap][cross]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto layout = GENERATE(
      Academy::CubemapLayoutHint::HorizontalCross,
      Academy::CubemapLayoutHint::VerticalCross,
      Academy::CubemapLayoutHint::HorizontalStrip,
      Academy::CubemapLayoutHint::VerticalStrip);
  const bool autoDetect = GENERATE(false, true);
  CAPTURE(int(layout), autoDetect);

  constexpr int F = 16;
  LoaderSetup setup;
  setup.layout = autoDetect ? Academy::CubemapLayoutHint::Auto : layout;
  setup.cubeFaceSize = 0;

  const Faces f = run_loader(api, make_layout(layout, F), setup);
  require_faces(f, F);

  for(int face = 0; face < 6; ++face)
  {
    INFO("face " << kFaceName[face]);
    CHECK(near(f.at(face, 0, 0), kWhite, 0));
    CHECK(near(f.at(face, 1, 0), kFace[face], 0));
    CHECK(near(f.at(face, F / 2, F / 2), kFace[face], 0));
    CHECK(near(f.at(face, F - 1, F - 1), kFace[face], 0));
  }
}

TEST_CASE(
    "academy cross cubemap is resampled to a requested face size",
    "[gfx][academy][cubemap][cross]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  LoaderSetup setup;
  setup.layout = Academy::CubemapLayoutHint::HorizontalCross;
  setup.cubeFaceSize = 32;

  const Faces f = run_loader(
      api, make_layout(Academy::CubemapLayoutHint::HorizontalCross, 16), setup);
  require_faces(f, 32);

  for(int face = 0; face < 6; ++face)
  {
    INFO("face " << kFaceName[face]);
    CHECK(near(f.at(face, 16, 16), kFace[face], 2));
    CHECK(near(f.at(face, 31, 31), kFace[face], 2));
  }
}

TEST_CASE(
    "academy cubemap with an unrecognised layout publishes nothing",
    "[gfx][academy][cubemap][cross]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  LoaderSetup setup;
  setup.layout = Academy::CubemapLayoutHint::Auto;

  QImage square(48, 48, QImage::Format_RGBA8888);
  square.fill(QColor(255, 0, 0, 255));
  const Faces f = run_loader(api, square, setup);
  if(f.skipped)
    SKIP(f.backend + ": backend unavailable");
  INFO("backend=" << f.backend);
  CHECK(f.error == "the loader published no texture");
}
