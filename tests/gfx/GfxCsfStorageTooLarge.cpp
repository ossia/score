// A storage buffer whose LAYOUT and size control ask for more bytes than can be
// allocated: 536870911 vec4 (the size control's maximum) is 8 GiB. The buffer
// was silently created at INT_MAX bytes and the shader run against a fraction
// of what it declares; a failed regrow kept the old buffer and recorded the new
// size (shader board C13). The pass must not run on a buffer smaller than its
// declaration, and the user must be told.
//
// csf-storage-too-large.cs only reads the bound length, it never writes the
// buffer, so the old behaviour shows as a red image rather than as
// out-of-bounds GPU writes.
#include <score_test/Gfx.hpp>

#include "GfxLogCapture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#if QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>
#endif
#include <QtGui/private/qrhi_p.h>
#if QT_CONFIG(vulkan) && __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#endif

using namespace score::test::gfx;

namespace
{
// The device's largest bindable storage buffer, queried here independently of
// the engine: Vulkan maxStorageBufferRange, GL MAX_SHADER_STORAGE_BLOCK_SIZE.
// -1 where the backend does not expose it. The Vulkan native handles carry
// the instance from Qt 6.6 (and live in qrhi_platform.h from 6.6 too).
int64_t deviceStorageBufferLimit(QRhi& rhi)
{
#if QT_CONFIG(vulkan) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if(rhi.backend() == QRhi::Vulkan)
  {
    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(nh && nh->inst && nh->physDev)
    {
      VkPhysicalDeviceProperties props{};
      nh->inst->functions()->vkGetPhysicalDeviceProperties(nh->physDev, &props);
      return props.limits.maxStorageBufferRange;
    }
  }
#endif
  if(rhi.backend() == QRhi::OpenGLES2 && rhi.makeThreadLocalNativeContextCurrent())
    if(auto* ctx = QOpenGLContext::currentContext())
    {
      GLint64 v = 0;
      ctx->extraFunctions()->glGetInteger64v(0x90DE /* MAX_SHADER_STORAGE_BLOCK_SIZE */, &v);
      return v;
    }
  return -1;
}
}

TEST_CASE(
    "a CSF storage buffer too large to allocate stops its passes with a warning",
    "[gfx][csf][storage][C13]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  struct
  {
    bool skipped{};
    std::string error;
    ReadbackImage img;
    std::vector<LogCapture::Message> messages;
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int csf
        = p.addCsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-storage-too-large.cs"));
    if(csf < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(csf, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    setControl(*p.isf(csf), nth_control_input(*p.isf(csf), 0), ossia::value{536870911});
    p.render(3);
    r.img = p.readback(sink);
    r.messages = log.messages();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());

  int red = 0;
  for(int y = 0; y < r.img.height; y++)
    for(int x = 0; x < r.img.width; x++)
      red += near(r.img.at(x, y), {255, 0, 0, 255}, 2);
  CHECK(red == 0);

  int warned = 0;
  for(const auto& m : r.messages)
    warned += m.text.contains(QStringLiteral("cannot be allocated"));
  CHECK(warned >= 1);
}

// The cap was INT_MAX bytes on every backend (2 GiB), from `int` sizes in the
// engine, while QRhi buffers take a quint32 size and Vulkan devices commonly bind
// 4 GiB - 1. A 2.4 GB buffer must be allocated where the device can bind it, its
// last element written and read back, and refused with the warning where it
// cannot (GL on NVIDIA: 2 GiB - 1).
TEST_CASE(
    "a CSF storage buffer above 2 GiB is allocated when the device can bind it",
    "[gfx][csf][storage][C13]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  constexpr int64_t required = 150000000LL * 16;
  struct
  {
    bool skipped{};
    std::string error;
    int64_t limit{-1};
    ReadbackImage img;
    std::vector<LogCapture::Message> messages;
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int csf
        = p.addCsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-storage-above-2gib.cs"));
    if(csf < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(csf, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    if(auto rs = p.sink(sink)->renderState(); rs && rs->rhi)
      r.limit = deviceStorageBufferLimit(*rs->rhi);
    setControl(*p.isf(csf), nth_control_input(*p.isf(csf), 0), ossia::value{150000000});
    p.render(3);
    r.img = p.readback(sink);
    r.messages = log.messages();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());
  CAPTURE(r.limit);

  int red = 0, green = 0;
  for(int y = 0; y < r.img.height; y++)
    for(int x = 0; x < r.img.width; x++)
    {
      red += near(r.img.at(x, y), {255, 0, 0, 255}, 2);
      green += near(r.img.at(x, y), {0, 255, 0, 255}, 2);
    }
  int warned = 0;
  for(const auto& m : r.messages)
    warned += m.text.contains(QStringLiteral("cannot be allocated"));

  CHECK(red == 0);
  if(r.limit >= required)
  {
    CHECK(warned == 0);
    CHECK(green == r.img.width * r.img.height);
  }
  else if(r.limit >= 0)
  {
    CHECK(warned >= 1);
    CHECK(green == 0);
  }
}
