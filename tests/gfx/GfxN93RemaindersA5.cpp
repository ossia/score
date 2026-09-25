// N93 remainders (agent A5).
//
// A read_only storage image INPUT in a raw raster has an image port, so
// initInputSamplers() gives it a sampler, but libisf declares no sampler for
// it. The raw raster bound that stray sampler at the first sampler binding
// and every sampler after it one binding late: the fixture's t0 read the
// storage image's placeholder instead of the cabled texture.
//
// On OpenGL a storage image is bound to the image unit of its binding, and
// NVIDIA exposes 8 units. A shader whose storage images reach the context's
// GL_MAX_IMAGE_UNITS logs one warning per shader (ISF, CSF, raw raster) on
// OpenGL, and none on the other backends or on a context with more units
// (Mesa's llvmpipe exposes 192).
//
// Registration:
//   score_add_gfx_test(n93_remainders_a5 GfxN93RemaindersA5.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QMutex>

#include <array>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

QMutex g_logMutex;
std::vector<QString> g_log;
QtMessageHandler g_prevHandler{};

void captureHandler(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
{
  {
    QMutexLocker lock{&g_logMutex};
    g_log.push_back(msg);
  }
  if(g_prevHandler)
    g_prevHandler(t, ctx, msg);
}

struct LogCapture
{
  LogCapture() { g_prevHandler = qInstallMessageHandler(captureHandler); }
  ~LogCapture()
  {
    qInstallMessageHandler(g_prevHandler);
    g_prevHandler = {};
  }

  static void clear()
  {
    QMutexLocker lock{&g_logMutex};
    g_log.clear();
  }

  static int count(const QString& needle)
  {
    QMutexLocker lock{&g_logMutex};
    int n = 0;
    for(const auto& m : g_log)
      if(m.contains(needle))
        n++;
    return n;
  }
};
}

TEST_CASE("a raw raster read_only storage image takes no sampler binding", "[gfx][raster][binding]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int green = p.addIsf(corpus("a5-solid-green.fs"));
    const int raster = p.addRaster(
        corpus("rr-a5-ro-image-then-sampler.vs"), corpus("rr-a5-ro-image-then-sampler.fs"));
    if(green < 0 || raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(green, 0), p.imageIn(raster, 1));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);

  INFO("error=" << err);
  REQUIRE(err.empty());
  for(auto [x, y] : std::array<std::array<int, 2>, 3>{{{4, 4}, {16, 16}, {27, 27}}})
  {
    const auto px = img.at(x, y);
    INFO("pixel " << x << "," << y << " = " << int(px[0]) << " " << int(px[1]) << " "
                  << int(px[2]));
    CHECK(px[0] < 40);
    CHECK(px[1] > 200);
    CHECK(px[2] < 40);
  }
}

TEST_CASE("storage image bindings past the GL image units warn once", "[gfx][binding]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto file = GENERATE("isf-a5-six-images.fs", "csf-a5-six-images.cs", "rr-a5-six-images");
  CAPTURE(backend_name(api), file);

  const QString f = QString::fromUtf8(file);
  constexpr int highestImageBinding = 8;

  LogCapture::clear();
  bool skipped = false;
  std::string backend;
  std::string err;
  int units = 0;
  for(int run = 0; run < 2; run++)
  {
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      LogCapture capture;
      GfxPipeline p;
      const int node = f.startsWith("rr-")
                           ? p.addRaster(corpus("rr-a5-six-images.vs"), corpus("rr-a5-six-images.fs"))
                       : f.endsWith(".cs") ? p.addCsf(corpus(file))
                                           : p.addIsf(corpus(file));
      if(node < 0)
      {
        err = p.error();
        return;
      }
      const int sink = p.addSink({16, 16});
      p.wire(p.imageOut(node, 0), p.sinkInput(sink));
      if(!p.create(api))
      {
        skipped = p.skipped();
        err = skipped ? std::string{} : p.error();
        return;
      }
      backend = p.backend();
      p.render(2);
      if(const auto st = p.sink(sink)->renderState(); st && st->rhi)
        units = score::gfx::storageImageUnitLimit(*st->rhi);
    });
    if(skipped)
      SKIP("backend unavailable");
  }
  if(f.endsWith(".cs"))
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);

  INFO("error=" << err);
  REQUIRE(err.empty());
  REQUIRE(units > 0);

  const QString kind = f.startsWith("rr-")  ? QStringLiteral("raw raster shader")
                       : f.endsWith(".cs") ? QStringLiteral("CSF shader")
                                           : QStringLiteral("ISF shader");
  const int warnings = LogCapture::count(QStringLiteral("image units"));
  const int kindWarnings = LogCapture::count(kind);
  INFO("backend " << backend << ", image units " << units);
  if(api == score::gfx::OpenGL && highestImageBinding >= units)
  {
    CHECK(warnings == 1);
    CHECK(kindWarnings == 1);
  }
  else
  {
    CHECK(warnings == 0);
  }
}

TEST_CASE("storage image bindings are read from the GLSL", "[gfx][binding]")
{
  using score::gfx::maxStorageImageBinding;
  CHECK(maxStorageImageBinding(u"layout(binding = 3) uniform sampler2D t;") == -1);
  CHECK(
      maxStorageImageBinding(
          u"layout(binding = 3, rgba8) uniform readonly image2D a;\n"
          u"layout(binding = 9, r32ui) restrict uniform uimage3D b;\n"
          u"layout(std430, binding = 12) buffer B { vec4 image2D_like[]; };")
      == 9);
}
