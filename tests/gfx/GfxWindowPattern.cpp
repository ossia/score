// =============================================================================
// L3 ORIENTATION AND MRT CORRECTNESS *THROUGH THE WINDOW OUTPUT PATH*.
//
// Every other analytic orientation test in tests/gfx/ (GfxOrientation,
// GfxOrientationMatrix, GfxMrtPattern) reads the render list back through a
// BackgroundNode offscreen target. The windowed tests (ScreenOutput,
// WindowOutputTorture, WindowDeviceLifecycle) drive a real ScreenNode but assert
// lifecycle and presentation only -- none of them looks at a single pixel.
//
// So the path the golden-render gate actually exercises -- ISF -> ScreenNode ->
// swapchain -> readback -- has no pixel-level coverage at all, and that is where
// the golden refs for build-mrt-gbuffer and build-isf-mrt-four-outputs disagree
// with the engine by an exact vertical flip on the two OpenGL backends.
//
// The relevant asymmetry is in WindowDevice::grabTo, which Y-corrects a swapchain
// readback with one blanket rule:
//
//     if(st->rhi->isYUpInFramebuffer()) img = img.mirrored(false, true);
//
// true on OpenGL, false on Vulkan/D3D/Metal. These tests apply exactly that rule
// and then assert the closed-form pattern, so a correction that is right for the
// single-output path and wrong for MRT (or the reverse) shows up as a flip on the
// backend where the branch is taken, instead of as a stale reference image.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_window_pattern
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_window_pattern
// Needs a real windowing system: the rig SKIPs under offscreen/minimal QPA, where
// there is no swapchain to present to and the Null backend would answer instead.
// =============================================================================
#include "WindowedOutputCommon.hpp"

#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QImage>

#include <cmath>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int kTol = 8; // 8-bit quantisation plus swapchain format conversion

/// Drive frames until the armed readback lands, then present it the way
/// WindowDevice::grabTo does: swapchain format fix-up, then the isYUpInFramebuffer
/// Y-correction. Returns a null image if nothing came back.
QImage grab_like_grabTo(ScreenRig& rig)
{
  // Present a few frames first: a readback armed before anything has been drawn
  // has nothing to copy.
  rig.render(4);
  rig.screen->requestReadback();
  const auto& rbp = rig.screen->readback();
  if(!rbp)
    return {};

  // A buffered swapchain does not hand back the frame that armed the request, so
  // keep driving until the copy lands.
  for(int i = 0; i < 60 && rbp->data.isEmpty(); ++i)
    rig.render(1);
  const auto& rb = *rbp;
  if(rb.data.isEmpty() || rb.pixelSize.width() <= 0 || rb.pixelSize.height() <= 0)
    return {};

  const int w = rb.pixelSize.width(), h = rb.pixelSize.height();
  if(rb.data.size() < w * h * 4)
    return {};

  const auto fmt
      = rb.format == QRhiTexture::BGRA8 ? QImage::Format_ARGB32 : QImage::Format_RGBA8888;
  QImage img{reinterpret_cast<const unsigned char*>(rb.data.constData()), w, h, w * 4, fmt};
  if(auto st = rig.screen->renderState(); st && st->rhi && st->rhi->isYUpInFramebuffer())
    img = img.mirrored(false, true);
  return img.copy().convertToFormat(QImage::Format_RGBA8888);
}

int expected_x(int col, int width)
{
  return int(255.0 * (double(col) + 0.5) / double(width) + 0.5);
}
/// Row 0 is the top of the delivered image; ISF's y == 1 is the top.
int expected_y(int row, int height)
{
  return int(255.0 * (1.0 - (double(row) + 0.5) / double(height)) + 0.5);
}

struct Fit
{
  int worst = 0, col = 0, row = 0, got = 0, expected = 0;
  char channel = '?';
  std::string describe() const
  {
    return "channel " + std::string(1, channel) + " at (" + std::to_string(col) + ","
           + std::to_string(row) + "): got " + std::to_string(got) + ", expected "
           + std::to_string(expected);
  }
};

/// Fit R against the X ramp and G against the Y ramp. B is left to the caller:
/// the single-output shader and the MRT shader put different things there.
Fit fit_xy(const QImage& img)
{
  Fit f;
  for(int y = 2; y < img.height() - 2; y += 7)
    for(int x = 2; x < img.width() - 2; x += 7)
    {
      const QRgb px = img.pixel(x, y);
      const int got[2] = {qRed(px), qGreen(px)};
      const int want[2] = {expected_x(x, img.width()), expected_y(y, img.height())};
      const char names[2] = {'R', 'G'};
      for(int c = 0; c < 2; ++c)
      {
        const int d = std::abs(got[c] - want[c]);
        if(d > f.worst)
          f = Fit{d, x, y, got[c], want[c], names[c]};
      }
    }
  return f;
}
} // namespace

// The single-output path through a real window. This is the control: if it fails,
// the window path is wrong for everything and the MRT case below says nothing
// specific about MRT.
namespace
{
/// Collected inside the app lambda; asserted after teardown, as the fixture
/// header requires (Catch2 macros must not run inside run_in_gui_app).
struct Shot
{
  bool skipped = false;
  std::string skipReason, error, backend;
  bool grabbed = false;
  Fit fit;
};

Shot shoot(score::gfx::GraphicsApi api, const QString& shader)
{
  Shot o;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, QSize{256, 192}, shader))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    const QImage img = grab_like_grabTo(rig);
    if(img.isNull())
    {
      o.error = "the window produced no readback";
      return;
    }
    o.grabbed = true;
    o.fit = fit_xy(img);
  });
  return o;
}

void check(const Shot& o)
{
  if(o.skipped)
    SKIP(o.backend + ": " + o.skipReason);
  INFO("backend=" << o.backend);
  REQUIRE(o.error.empty());
  REQUIRE(o.grabbed);
  INFO(o.fit.describe());
  CHECK(o.fit.worst <= kTol);
}
} // namespace

TEST_CASE(
    "window output: a single-output ISF arrives right way up",
    "[gfx][window][screen][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  check(shoot(api, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-xy-pattern.fs")));
}

// attachment 0 is what a ScreenNode presents, so through the window an MRT shader
// must be indistinguishable from the single-output render above.
TEST_CASE(
    "window output: an MRT ISF arrives right way up",
    "[gfx][window][screen][orientation][mrt]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  check(shoot(api, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-mrt-pattern.fs")));
}
