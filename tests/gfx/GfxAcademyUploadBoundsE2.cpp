// Academy TextureResource::upload and EquirectToCubemap orientation.
//
// upload(): a TextureCpuData whose pixel buffer is shorter than the slices its
// kind needs (six cube faces, depth slices, array layers, one 2D image) must
// yield no texture instead of handing QRhi pointers past the end of the
// buffer. The shortfall cases include a 16-byte buffer for 2048x2048 faces, so
// an unchecked upload copies tens of megabytes past a tiny heap block. Exact
// sizes still upload.
//
// Equirect orientation: the vertex stage of EquirectToCubemap mirrors
// gl_Position.y on the HLSL and MSL targets only (D3D and Metal put NDC y = +1
// at framebuffer row 0, Vulkan and OpenGL put y = -1 there). The baked HLSL and
// MSL must carry the negation, the GLSL must not. On every available backend,
// a panorama whose upper half is red and lower half blue must give a red +Y
// face, a blue -Y face, and side faces whose row 0 is red and last row blue.
//
// Registration: see the test_gfx_academy_upload_bounds_e2 target.
#include <score_test/Gfx.hpp>

#include <Academy/Resources/EquirectToCubemap.hpp>
#include <Academy/Resources/TextureResource.hpp>

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/ShaderCache.hpp>

#include <QFile>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif
#include <QImage>
#include <QRegularExpression>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstring>
#include <memory>
#include <string>

using namespace score::test::gfx;
using score::academy::LoadedTexture;
using score::academy::TextureCpuData;
using score::academy::TextureLoadOptions;
using score::academy::TextureResource;

namespace
{
struct UploadCase
{
  const char* name;
  TextureLoadOptions::Kind kind;
  QSize size;
  int depth;
  int layers;
  int cubeFaceSize;
  size_t bytes;
  bool expectValid;
};

bool upload_on_null(const UploadCase& c, std::string& error)
{
  QRhiNullInitParams params;
  std::unique_ptr<QRhi> rhi{QRhi::create(QRhi::Null, &params)};
  if(!rhi)
  {
    error = "no Null QRhi";
    return false;
  }

  TextureCpuData cpu;
  cpu.size = c.size;
  cpu.depth = c.depth;
  cpu.layers = c.layers;
  cpu.bytesPerPixel = 4;
  cpu.format = QRhiTexture::RGBA8;
  cpu.detectedKind = c.kind;
  cpu.pixels.assign(c.bytes, std::byte{0x7f});

  TextureLoadOptions opts;
  opts.kind = c.kind;
  opts.generateMips = false;
  opts.sRGB = false;
  opts.cubeFaceSize = c.cubeFaceSize;

  auto* batch = rhi->nextResourceUpdateBatch();
  const LoadedTexture loaded = TextureResource::upload(*rhi, *batch, cpu, opts);
  const bool valid = loaded.valid();

  QRhiCommandBuffer* cb{};
  if(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess)
  {
    cb->resourceUpdate(batch);
    rhi->endOffscreenFrame();
  }
  else
  {
    batch->release();
  }
  delete loaded.texture;
  return valid;
}

constexpr size_t face(int n) noexcept
{
  return size_t(n) * size_t(n) * 4;
}

const UploadCase kUploadCases[]{
    {"cube, six faces", TextureLoadOptions::Kind::Cubemap, {32, 32}, 1, 6, 0,
     6 * face(32), true},
    {"cube, five faces", TextureLoadOptions::Kind::Cubemap, {32, 32}, 1, 6, 0,
     5 * face(32), false},
    {"cube, 16 bytes for 2048px faces", TextureLoadOptions::Kind::Cubemap,
     {2, 2}, 1, 6, 2048, 16, false},
    {"cube, requested face size larger than the data",
     TextureLoadOptions::Kind::Cubemap, {32, 32}, 1, 6, 64, 6 * face(32), false},
    {"3D, eight slices", TextureLoadOptions::Kind::Texture3D, {16, 16}, 8, 1, 0,
     8 * face(16), true},
    {"3D, seven slices for depth 8", TextureLoadOptions::Kind::Texture3D,
     {16, 16}, 8, 1, 0, 7 * face(16), false},
    {"array, four layers", TextureLoadOptions::Kind::TextureArray, {16, 16}, 1, 4,
     0, 4 * face(16), true},
    {"array, one layer for four", TextureLoadOptions::Kind::TextureArray,
     {16, 16}, 1, 4, 0, face(16), false},
    {"2D, whole image", TextureLoadOptions::Kind::Texture2D, {16, 16}, 1, 1, 0,
     face(16), true},
    {"2D, half an image", TextureLoadOptions::Kind::Texture2D, {16, 16}, 1, 1, 0,
     face(16) / 2, false},
};

QByteArray equirect_vertex_source()
{
  QFile f(QStringLiteral(ACADEMY_EQUIRECT_CPP));
  if(!f.open(QIODevice::ReadOnly))
    return {};
  const QByteArray src = f.readAll();
  const QByteArray open = "k_equirect_vs = R\"_(";
  const qsizetype b = src.indexOf(open);
  if(b < 0)
    return {};
  const qsizetype start = b + open.size();
  const qsizetype end = src.indexOf(")_\"", start);
  if(end < 0)
    return {};
  return src.mid(start, end - start);
}

bool mirrors_y(const QByteArray& generated)
{
  static const QRegularExpression re{
      QStringLiteral(R"(gl_Position\.y\s*=\s*-\s*(?:\w+\.)?gl_Position\.y\s*;)")};
  return re.match(QString::fromUtf8(generated)).hasMatch();
}

using RGBA = std::array<uint8_t, 4>;
constexpr RGBA kRed{255, 0, 0, 255};
constexpr RGBA kBlue{0, 0, 255, 255};

struct CubeFaces
{
  bool skipped = false;
  std::string backend;
  std::string error;
  int faceSize = 0;
  std::array<QByteArray, 6> data;

  RGBA at(int f, int x, int y) const
  {
    const auto* p = reinterpret_cast<const uint8_t*>(data[f].constData())
                    + (size_t(y) * faceSize + x) * 4;
    return {p[0], p[1], p[2], p[3]};
  }
};

CubeFaces run_equirect(score::gfx::GraphicsApi api)
{
  CubeFaces out;
  out.backend = backend_name(api);
  out.faceSize = 32;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(api, probed))
    {
      out.skipped = true;
      return;
    }
    auto st = score::gfx::createRenderState(api, QSize{64, 64}, nullptr);
    if(!st || !st->rhi)
    {
      out.skipped = true;
      return;
    }
    QRhi& rhi = *st->rhi;

    constexpr int W = 64, H = 32;
    QByteArray pano(W * H * 4, Qt::Uninitialized);
    for(int y = 0; y < H; ++y)
      for(int x = 0; x < W; ++x)
      {
        const RGBA& c = y < H / 2 ? kRed : kBlue;
        std::memcpy(pano.data() + (size_t(y) * W + x) * 4, c.data(), 4);
      }

    QRhiTexture* src = rhi.newTexture(QRhiTexture::RGBA8, QSize{W, H});
    QRhiTexture* cube = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{out.faceSize, out.faceSize}, 1,
        QRhiTexture::CubeMap | QRhiTexture::RenderTarget
            | QRhiTexture::UsedAsTransferSource);
    if(!src->create() || !cube->create())
    {
      out.error = "texture creation failed";
    }
    else
    {
      score::academy::EquirectToCubemap conv;
      if(!conv.init(rhi, src, cube, out.faceSize, *st))
      {
        out.error = "EquirectToCubemap::init failed";
      }
      else
      {
        std::array<QRhiReadbackResult, 6> rb;
        QRhiCommandBuffer* cb{};
        if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        {
          out.error = "beginOffscreenFrame failed";
        }
        else
        {
          auto* up = rhi.nextResourceUpdateBatch();
          up->uploadTexture(src, QImage(
              reinterpret_cast<const uchar*>(pano.constData()), W, H,
              QImage::Format_RGBA8888));
          cb->resourceUpdate(up);
          conv.run(*cb);
          auto* rbBatch = rhi.nextResourceUpdateBatch();
          for(int f = 0; f < 6; ++f)
          {
            QRhiReadbackDescription desc{cube};
            desc.setLayer(f);
            rbBatch->readBackTexture(desc, &rb[f]);
          }
          cb->resourceUpdate(rbBatch);
          rhi.endOffscreenFrame();
          for(int f = 0; f < 6; ++f)
            out.data[f] = rb[f].data;
        }
      }
      conv.release();
    }
    delete cube;
    delete src;
    st->destroy();
  });
  return out;
}
}

TEST_CASE(
    "academy texture upload refuses pixel data shorter than its slices",
    "[gfx][academy][texture][bounds]")
{
  const auto& c = GENERATE(from_range(kUploadCases));
  CAPTURE(c.name);
  std::string error;
  const bool valid = upload_on_null(c, error);
  REQUIRE(error.empty());
  CHECK(valid == c.expectValid);
}

TEST_CASE(
    "academy equirect vertex stage mirrors Y on the HLSL and MSL targets only",
    "[gfx][academy][cubemap][equirect][shader]")
{
  const QByteArray vs = equirect_vertex_source();
  REQUIRE(!vs.isEmpty());

  struct Target
  {
    score::gfx::GraphicsApi api;
    QShaderVersion version;
    QShader::Source source;
    bool mirrored;
  };
  const Target targets[]{
      {score::gfx::Metal, QShaderVersion(24), QShader::MslShader, true},
      {score::gfx::D3D11, QShaderVersion(50), QShader::HlslShader, true},
      {score::gfx::OpenGL, QShaderVersion(330), QShader::GlslShader, false},
  };
  for(const auto& t : targets)
  {
    CAPTURE(backend_name(t.api));
    const auto& [shader, err] = score::gfx::ShaderCache::get(
        t.api, t.version, vs, QShader::VertexStage);
    INFO("bake error: " << err.toStdString());
    REQUIRE(shader.isValid());
    const QShaderCode code = shader.shader({t.source, t.version});
    REQUIRE(!code.shader().isEmpty());
    INFO(code.shader().toStdString());
    CHECK(mirrors_y(code.shader()) == t.mirrored);
  }
}

TEST_CASE(
    "academy equirect-to-cubemap keeps the panorama's top on +Y and on row 0",
    "[gfx][academy][cubemap][equirect]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const CubeFaces f = run_equirect(api);
  if(f.skipped)
    SKIP(f.backend + ": backend unavailable");
  INFO("backend=" << f.backend << " error='" << f.error << "'");
  REQUIRE(f.error.empty());
  const int F = f.faceSize;
  for(int i = 0; i < 6; ++i)
    REQUIRE(f.data[i].size() == qsizetype(F) * F * 4);

  CHECK(near(f.at(2, F / 2, F / 2), kRed, 2));
  CHECK(near(f.at(3, F / 2, F / 2), kBlue, 2));
  for(int side : {0, 1, 4, 5})
  {
    CAPTURE(side);
    CHECK(near(f.at(side, F / 2, 0), kRed, 2));
    CHECK(near(f.at(side, F / 2, F - 1), kBlue, 2));
  }
}
