// Runs score_tc -- the sampling seam every video decoder samples through -- on
// a real GPU, and checks the three things it can get wrong.
//
// The seam lives in SCORE_GFX_VIDEO_UNIFORMS, so every decoder gets it whether
// or not it ever sees a field. That makes the FIRST case here the important
// one: with the progressive mode, score_tc must be the exact identity, because
// all 85 sampling calls in the decoders now go through it and a drift of half a
// texel would shift every video in score by half a line.
//
// The other two cases are the reason it exists:
//
//   weave -- output line y reads the field that owns its parity, out of one
//            stacked texture (field 0 in the top half, field 1 in the bottom).
//   bob   -- only the newest field, interpolated to full height, offset by half
//            a line for field 1. Without that offset the picture jitters
//            vertically at the field rate, which is the classic bob bug; it is
//            measured here rather than eyeballed.
//
// Skips rather than fails without a GPU. Backend follows the same convention as
// CaptureAdjustShaderTest.

#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhigles2_p.h>

#include <clocale>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace score::gfx;

namespace
{
int g_fail = 0;

void check(bool ok, const std::string& what)
{
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if(!ok)
    ++g_fail;
}

const char* kVert = R"(#version 450
layout(location = 0) out vec2 v_texcoord;
void main() {
  v_texcoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(v_texcoord * 2.0 - 1.0, 0.0, 1.0);
}
)";

// The seam, sampled exactly as a decoder samples it.
QString fragSource()
{
  return QString(R"(#version 450

)") + SCORE_GFX_VIDEO_UNIFORMS + R"(
layout(binding = 3) uniform sampler2D u_tex;
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() {
  fragColor = texture(u_tex, score_tc(v_texcoord));
}
)";
}

// Mirrors VideoNodeRenderer::Material. Two declarations of one UBO is exactly
// the kind of thing that drifts, so the test carries its own and a mismatch
// shows up as garbage in the first case.
struct MaterialUBO
{
  float scale[2]{1.f, 1.f};
  float texSz[2]{};
  float field[4]{};
};

struct Result
{
  bool ok{};
  std::vector<std::uint8_t> pixels;  // RGBA8, w*h*4
};

/// Render `src` (already a stacked texture when fielded) through the seam.
Result render(
    QRhi& rhi, GraphicsApi api, QShaderVersion version, const QImage& src, int outW,
    int outH, float parity, float mode)
{
  const int w = src.width(), h = src.height();
  std::unique_ptr<QRhiTexture> tex{rhi.newTexture(QRhiTexture::RGBA8, QSize{w, h}, 1)};
  if(!tex->create())
    return {};

  // Linear, because bob relies on the sampler to interpolate between field
  // lines -- with Nearest the half-line offset would be unobservable.
  std::unique_ptr<QRhiSampler> sampler{rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge)};
  sampler->create();

  std::unique_ptr<QRhiTexture> target{
      rhi.newTexture(QRhiTexture::RGBA8, QSize{outW, outH}, 1, QRhiTexture::RenderTarget)};
  target->create();
  std::unique_ptr<QRhiTextureRenderTarget> rt{
      rhi.newTextureRenderTarget({{target.get()}})};
  std::unique_ptr<QRhiRenderPassDescriptor> rp{rt->newCompatibleRenderPassDescriptor()};
  rt->setRenderPassDescriptor(rp.get());
  rt->create();

  MaterialUBO ubo{};
  ubo.texSz[0] = float(outW);
  ubo.texSz[1] = float(outH);
  ubo.field[0] = parity;
  ubo.field[1] = mode;

  std::unique_ptr<QRhiBuffer> matBuf{rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(MaterialUBO))};
  matBuf->create();

  std::unique_ptr<QRhiShaderResourceBindings> srb{rhi.newShaderResourceBindings()};
  srb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           2, QRhiShaderResourceBinding::FragmentStage, matBuf.get()),
       QRhiShaderResourceBinding::sampledTexture(
           3, QRhiShaderResourceBinding::FragmentStage, tex.get(), sampler.get())});
  srb->create();

  auto [vs, vsErr]
      = ShaderCache::get(api, version, QByteArray{kVert}, QShader::VertexStage);
  auto [fs, fsErr]
      = ShaderCache::get(api, version, fragSource().toUtf8(), QShader::FragmentStage);
  if(!vsErr.isEmpty() || !fsErr.isEmpty())
  {
    std::printf("    shader did not bake: %s\n", qPrintable(fsErr));
    return {};
  }

  std::unique_ptr<QRhiGraphicsPipeline> ps{rhi.newGraphicsPipeline()};
  ps->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
  ps->setVertexInputLayout({});
  ps->setShaderResourceBindings(srb.get());
  ps->setRenderPassDescriptor(rp.get());
  ps->setTopology(QRhiGraphicsPipeline::Triangles);
  if(!ps->create())
    return {};

  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return {};

  auto* batch = rhi.nextResourceUpdateBatch();
  batch->uploadTexture(tex.get(), src);
  batch->updateDynamicBuffer(matBuf.get(), 0, sizeof(MaterialUBO), &ubo);

  cb->beginPass(rt.get(), QColor::fromRgbF(0, 0, 0, 1), {1.f, 0}, batch);
  cb->setGraphicsPipeline(ps.get());
  cb->setViewport({0, 0, float(outW), float(outH)});
  cb->setShaderResources(srb.get());
  cb->draw(3);
  cb->endPass();

  QRhiReadbackResult rb;
  bool done = false;
  rb.completed = [&done] { done = true; };
  auto* readBatch = rhi.nextResourceUpdateBatch();
  readBatch->readBackTexture({target.get()}, &rb);
  cb->resourceUpdate(readBatch);
  rhi.endOffscreenFrame();

  if(!done || rb.data.isEmpty())
    return {};

  Result out;
  out.ok = true;
  out.pixels.assign(
      reinterpret_cast<const std::uint8_t*>(rb.data.constData()),
      reinterpret_cast<const std::uint8_t*>(rb.data.constData()) + rb.data.size());
  return out;
}

int red(const Result& r, int w, int x, int y)
{
  return r.pixels[(size_t(y) * w + x) * 4];
}
int green(const Result& r, int w, int x, int y)
{
  return r.pixels[(size_t(y) * w + x) * 4 + 1];
}

/// Progressive: the seam must be the exact identity. Every video in score goes
/// through this path, so a half-texel drift here is a half-line shift
/// everywhere.
void testIdentity(QRhi& rhi, GraphicsApi api, QShaderVersion v)
{
  std::printf("\n  == progressive: score_tc is the identity ==\n");
  const int w = 8, h = 16;

  // A different grey per line, so any vertical shift is visible as a wrong row.
  QImage src(w, h, QImage::Format_RGBA8888);
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
      src.setPixelColor(x, y, QColor(y * 16, 255 - y * 16, 0));

  const auto r = render(rhi, api, v, src, w, h, 0.f, 0.f);
  if(!r.ok)
  {
    std::printf("    skip: no render\n");
    return;
  }

  int worst = 0, worstRow = -1;
  for(int y = 0; y < h; ++y)
  {
    const int d = std::abs(red(r, w, w / 2, y) - y * 16);
    if(d > worst)
    {
      worst = d;
      worstRow = y;
    }
  }
  std::printf("    worst row error: %d (row %d)\n", worst, worstRow);
  check(worst <= 1, "every line renders from its own source line");
}

/// Weave: even output lines come from the top half, odd from the bottom.
void testWeave(QRhi& rhi, GraphicsApi api, QShaderVersion v)
{
  std::printf("\n  == weave: each line reads the field that owns its parity ==\n");
  const int w = 8, outH = 16, halfH = outH / 2;

  // Stacked: top half solid red (field 0, the even lines), bottom half solid
  // green (field 1, the odd lines). Red and green are as far apart as two
  // channels get, so a transposed mapping cannot hide.
  QImage src(w, outH, QImage::Format_RGBA8888);
  for(int y = 0; y < outH; ++y)
    for(int x = 0; x < w; ++x)
      src.setPixelColor(x, y, y < halfH ? QColor(255, 0, 0) : QColor(0, 255, 0));

  const auto r = render(rhi, api, v, src, w, outH, 0.f, 1.f);
  if(!r.ok)
  {
    std::printf("    skip: no render\n");
    return;
  }

  bool ok = true;
  for(int y = 0; y < outH; ++y)
  {
    const bool evenLine = (y % 2) == 0;
    const int rr = red(r, w, w / 2, y), gg = green(r, w, w / 2, y);
    const bool fromField0 = rr > 200 && gg < 55;
    const bool fromField1 = gg > 200 && rr < 55;
    if(evenLine ? !fromField0 : !fromField1)
    {
      ok = false;
      std::printf("    line %d: (%d,%d) -- wrong field\n", y, rr, gg);
    }
  }
  check(ok, "even lines from field 0, odd lines from field 1");
}

/// Bob, and the half-line offset, measured.
///
/// One bright source line in one field; the rendered luminance centroid must
/// sit half an output line apart between the two parities. Zero means the
/// offset was forgotten; a whole line means it was applied twice.
void testBobHalfLineOffset(QRhi& rhi, GraphicsApi api, QShaderVersion v)
{
  std::printf("\n  == bob: the half-line offset, measured ==\n");
  const int w = 8, outH = 32, halfH = outH / 2;
  const int fieldLine = 6;  // same line index within each field

  double centroid[2]{};
  bool rendered = true;
  for(int parity = 0; parity < 2; ++parity)
  {
    QImage src(w, outH, QImage::Format_RGBA8888);
    src.fill(QColor(0, 0, 0));
    for(int x = 0; x < w; ++x)
      src.setPixelColor(x, parity * halfH + fieldLine, QColor(255, 255, 255));

    const auto r = render(rhi, api, v, src, w, outH, float(parity), 2.f);
    if(!r.ok)
    {
      rendered = false;
      break;
    }

    double num = 0, den = 0;
    for(int y = 0; y < outH; ++y)
    {
      const double lum = red(r, w, w / 2, y);
      num += lum * (y + 0.5);
      den += lum;
    }
    centroid[parity] = den > 0 ? num / den : -1;
  }

  if(!rendered)
  {
    std::printf("    skip: no render\n");
    return;
  }

  const double delta = centroid[1] - centroid[0];
  std::printf(
      "    centroid: field 0 at %.3f, field 1 at %.3f, delta %.3f output lines\n",
      centroid[0], centroid[1], delta);
  check(
      std::abs(delta - 1.0) < 0.35,
      "field 1 sits one output line (half a source line) below field 0");
  check(delta > 0.25, "the half-line offset is applied at all");
  check(delta < 2.0, "the half-line offset is not applied twice");
}

int runTests()
{
  const GraphicsApi api = GraphicsApi::OpenGL;
  bool isEs = false;
  {
    QOpenGLContext probe;
    if(probe.create())
      isEs = probe.isOpenGLES();
  }
  const QShaderVersion version
      = isEs ? QShaderVersion{300, QShaderVersion::GlslEs} : QShaderVersion{330};

  std::unique_ptr<QOffscreenSurface> surface{QRhiGles2InitParams::newFallbackSurface()};
  QRhiGles2InitParams params;
  params.fallbackSurface = surface.get();
  std::unique_ptr<QRhi> rhiOwner{QRhi::create(QRhi::OpenGLES2, &params)};
  if(!rhiOwner)
  {
    std::printf("DeinterlaceShaderTest: no RHI available, skipping\n");
    return 0;
  }
  auto& rhi = *rhiOwner;
  std::printf(
      "DeinterlaceShaderTest: %s, %s\n", rhi.backendName(),
      isEs ? "GLSL ES 300" : "GLSL 330");

  testIdentity(rhi, api, version);
  testWeave(rhi, api, version);
  testBobHalfLineOffset(rhi, api, version);

  std::printf(
      "\ndeinterlace shader: %s (%d failure%s)\n", g_fail ? "FAILED" : "passed", g_fail,
      g_fail == 1 ? "" : "s");
  return g_fail ? 1 : 0;
}
}

int main(int argc, char** argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setlocale(LC_ALL, "C");
  QGuiApplication app{argc, argv};
  // After QGuiApplication: it installs the system locale, which would print
  // "13,000" for thirteen point zero and make the centroid unreadable.
  std::setlocale(LC_ALL, "C");
  return runTests();
}
