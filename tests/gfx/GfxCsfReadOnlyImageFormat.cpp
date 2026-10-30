// The inlet of a CSF image declared ACCESS read_only is a render target the
// upstream node draws into. It was always RGBA8 unless the inlet set a format
// by hand, whatever FORMAT the shader declared: float input (an R32F depth in
// metres, an RGBA32F position map) arriving there was clamped to 0-1. The
// declared format now applies, and a format set on the inlet still wins.
//
// An ISF writes values outside 0-1 into the CSF's read-only image; the CSF
// copies what it reads into an RGBA32F storage image that is read back here.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <QtGui/private/qrhi_p.h>

#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr const char* upstreamIsf = R"_(/*{
  "ISFVSN": "2"
}*/
void main() { gl_FragColor = vec4(2.5, -1.0, 10.0, 1.0); }
)_";

QString csf(const std::string& format)
{
  return QString::fromStdString(R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "inputImage", "TYPE": "image", "ACCESS": "read_only", "FORMAT": ")_"
                                + format + R"_(" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba32f", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  imageStore(outputImage, p, imageLoad(inputImage, p));
}
)_");
}

bool writeFile(const QString& path, const QByteArray& data)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return false;
  return f.write(data) == data.size();
}

struct Run
{
  bool skipped{};
  std::string error;
  int inletFormat{-1};
  std::array<float, 4> texel{-99.f, -99.f, -99.f, -99.f};
};

std::optional<std::array<float, 4>> readCentreRgba32f(QRhi& rhi, QRhiTexture* tex)
{
  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return std::nullopt;
  QRhiReadbackResult rr;
  auto* batch = rhi.nextResourceUpdateBatch();
  batch->readBackTexture(QRhiReadbackDescription{tex}, &rr);
  cb->resourceUpdate(batch);
  rhi.endOffscreenFrame();
  const int w = rr.pixelSize.width();
  const int h = rr.pixelSize.height();
  if(w <= 0 || h <= 0 || rr.data.size() < qsizetype(w) * h * 16)
    return std::nullopt;
  std::array<float, 4> px{};
  std::memcpy(
      px.data(), rr.data.constData() + (std::size_t(h / 2) * w + w / 2) * 16, 16);
  return px;
}

Run run(
    score::gfx::GraphicsApi api, const std::string& format,
    std::optional<ossia::texture_format> inletFormat)
{
  Run r;
  QTemporaryDir dir;
  if(!dir.isValid())
  {
    r.error = "no temporary dir";
    return r;
  }
  const QString fs = dir.path() + "/up.fs";
  const QString cs = dir.path() + "/ro.cs";
  if(!writeFile(fs, upstreamIsf) || !writeFile(cs, csf(format).toUtf8()))
  {
    r.error = "cannot write shaders";
    return r;
  }

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int up = p.addIsf(fs);
    const int c = p.addCsf(cs);
    if(up < 0 || c < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    if(inletFormat)
    {
      ossia::render_target_spec s;
      s.format = *inletFormat;
      s.format_set = true;
      p.isf(c)->process(0, s);
    }
    p.wire(p.imageOut(up, 0), p.imageIn(c, 0));
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(c, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);

    auto& node = *p.isf(c);
    if(node.renderedNodes.empty())
    {
      r.error = "the CSF has no renderer";
      return;
    }
    auto* rn = node.renderedNodes.begin()->second;
    auto* rl = p.sink(sink)->renderer();
    if(!rl || !rl->state.rhi)
    {
      r.error = "no render list";
      return;
    }
    QRhiTexture* inlet = rn->renderTargetForInput(*node.input[0]).texture;
    if(!inlet)
      inlet = rl->renderTargetForInputPort(*node.input[0]).texture;
    if(inlet)
      r.inletFormat = int(inlet->format());
    auto* out = rn->textureForOutput(*node.output[0]);
    if(!out)
    {
      r.error = "the CSF has no output texture";
      return;
    }
    if(auto px = readCentreRgba32f(*rl->state.rhi, out))
      r.texel = *px;
    else
      r.error = "readback failed";
  });
  return r;
}

struct Case
{
  std::string format;
  QRhiTexture::Format expected;
  int channels;
};
}

TEST_CASE(
    "a CSF read-only image's inlet gets the format it declares",
    "[gfx][csf][format][read_only]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto c = GENERATE(from_range(std::vector<Case>{
      Case{"r32f", QRhiTexture::R32F, 1},
      Case{"rgba32f", QRhiTexture::RGBA32F, 4},
  }));
  CAPTURE(backend_name(be), c.format);

  const Run r = run(be, c.format, std::nullopt);
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(r.error);
  REQUIRE(r.error.empty());
  CHECK(r.inletFormat == int(c.expected));

  INFO(
      "read " << r.texel[0] << ", " << r.texel[1] << ", " << r.texel[2] << ", "
              << r.texel[3]);
  // Exact: 2.5, -1 and 10 are representable in 32-bit float. Alpha is 1: the
  // upstream ISF premultiplies its colour.
  CHECK(r.texel[0] == 2.5f);
  if(c.channels == 4)
  {
    CHECK(r.texel[1] == -1.0f);
    CHECK(r.texel[2] == 10.0f);
    CHECK(r.texel[3] == 1.0f);
  }
}

TEST_CASE(
    "a format set on a CSF read-only image's inlet wins over the declared one",
    "[gfx][csf][format][read_only]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  // Declared r32f, the inlet asks for RGBA32F: the render target is the
  // inlet's. Only its format is checked: reading an RGBA32F texture through an
  // r32f image is not something every backend defines.
  const Run r = run(be, "r32f", ossia::texture_format::RGBA32F);
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(r.error);
  REQUIRE(r.error.empty());
  CHECK(r.inletFormat == int(QRhiTexture::RGBA32F));
}
