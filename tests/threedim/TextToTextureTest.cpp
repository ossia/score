// Text to Texture — CPU text rasterization guard (Threedim/TextToTexture.hpp).
//
// The node paints a string with QPainter into a Format_RGBA8888 QImage and
// memcpy's it into a halp::texture_output<rgba_texture> (create() sizes the
// storage and clears `changed`; upload() re-raises it). Everything is CPU-side
// — no QRhi, no GPU — but QPainter text needs a QGuiApplication, so one is
// made on first use (mirroring score-plugin-gfx/tests/InteropRingPolicyTest).
// ctest runs with QT_QPA_PLATFORM=offscreen (ScoreTests.cmake); for manual
// runs the helper below sets it as a fallback before the app is created.
//
// Fonts differ across machines, so no assertion depends on glyph geometry:
// we assert painted-vs-background pixel counts, exact background fills,
// dominant painted colour under an antialiasing tolerance, painted-pixel
// centroids for alignment, and monotonic growth with font size.
#include <Threedim/TextToTexture.hpp>

#include <QFontDatabase>
#include <QGuiApplication>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

namespace
{

// QPainter's text path (QFontDatabase, glyph caches) needs a
// QGuiApplication. Catch2 owns main(), so make one on first use.
void ensureApp()
{
  if(!qApp)
  {
    if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "offscreen");
    static int argc = 1;
    static char arg0[] = "TextToTextureTest";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication app(argc, argv);
  }
}

// A platform with zero fonts cannot rasterize any glyph; the node is not at
// fault there, so the text-painting tests skip instead of failing.
bool hasFonts()
{
  ensureApp();
  return !QFontDatabase::families().isEmpty();
}

using Px = std::array<unsigned char, 4>; // R, G, B, A — RGBA8888 byte order

Px pixel(const Threedim::TextToTexture& n, int x, int y)
{
  const auto& t = n.outputs.main.texture;
  const unsigned char* p = t.bytes + (std::size_t(y) * t.width + x) * 4;
  return {p[0], p[1], p[2], p[3]};
}

// Number of pixels differing from the background colour.
int paintedCount(const Threedim::TextToTexture& n, Px bg)
{
  const auto& t = n.outputs.main.texture;
  int count = 0;
  for(int y = 0; y < t.height; y++)
    for(int x = 0; x < t.width; x++)
      if(pixel(n, x, y) != bg)
        count++;
  return count;
}

// Centre of mass of the non-background pixels; {-1, -1} if none.
std::array<double, 2> paintedCentroid(const Threedim::TextToTexture& n, Px bg)
{
  const auto& t = n.outputs.main.texture;
  double sx = 0., sy = 0.;
  int count = 0;
  for(int y = 0; y < t.height; y++)
    for(int x = 0; x < t.width; x++)
      if(pixel(n, x, y) != bg)
      {
        sx += x;
        sy += y;
        count++;
      }
  if(count == 0)
    return {-1., -1.};
  return {sx / count, sy / count};
}

constexpr Px transparent{0, 0, 0, 0};

} // namespace

TEST_CASE(
    "TextToTexture declared size matches the byte buffer",
    "[threedim][text_to_texture]")
{
  ensureApp();
  Threedim::TextToTexture node;

  // Defaults: 1024x256 canvas.
  node.recreate();
  auto& out = node.outputs.main;
  CHECK(out.texture.width == node.inputs.canvas_w.value);
  CHECK(out.texture.height == node.inputs.canvas_h.value);
  REQUIRE(out.texture.bytes != nullptr);
  CHECK(out.texture.bytes == out.storage.data());
  CHECK(
      out.storage.size()
      == std::size_t(out.texture.width) * out.texture.height * 4);
  CHECK(out.texture.bytesize() == out.storage.size());

  // A custom canvas resizes the buffer accordingly.
  node.inputs.canvas_w.value = 128;
  node.inputs.canvas_h.value = 64;
  node.recreate();
  CHECK(out.texture.width == 128);
  CHECK(out.texture.height == 64);
  CHECK(out.storage.size() == std::size_t(128) * 64 * 4);
  CHECK(out.texture.bytes == out.storage.data());
}

TEST_CASE(
    "TextToTexture empty text yields a fully-background texture",
    "[threedim][text_to_texture]")
{
  ensureApp();
  Threedim::TextToTexture node;
  node.inputs.text.value = "";
  node.inputs.canvas_w.value = 64;
  node.inputs.canvas_h.value = 32;

  SECTION("default transparent background: every byte is zero")
  {
    node.recreate();
    REQUIRE(node.outputs.main.texture.bytes != nullptr);
    CHECK(paintedCount(node, transparent) == 0);
  }

  SECTION("opaque blue background fills every pixel exactly")
  {
    node.inputs.bg_r.value = 0.f;
    node.inputs.bg_g.value = 0.f;
    node.inputs.bg_b.value = 1.f;
    node.inputs.bg_a.value = 1.f;
    node.recreate();
    const Px blue{0, 0, 255, 255};
    CHECK(paintedCount(node, blue) == 0);
    CHECK(pixel(node, 0, 0) == blue);
    CHECK(pixel(node, 63, 31) == blue);
  }
}

TEST_CASE(
    "TextToTexture non-empty text paints pixels over the background",
    "[threedim][text_to_texture]")
{
  if(!hasFonts())
    SKIP("no fonts available on this platform");

  Threedim::TextToTexture node; // default text "Hello, world", white on transparent
  node.recreate();
  CHECK(paintedCount(node, transparent) > 0);
}

TEST_CASE(
    "TextToTexture text colour control is honoured",
    "[threedim][text_to_texture]")
{
  if(!hasFonts())
    SKIP("no fonts available on this platform");

  Threedim::TextToTexture node;
  node.inputs.fg_r.value = 1.f;
  node.inputs.fg_g.value = 0.f;
  node.inputs.fg_b.value = 0.f;
  node.inputs.fg_a.value = 1.f;
  node.recreate();

  // At 64px on a 1024x256 canvas the glyph interiors are fully opaque, so
  // strongly-painted pixels exist and their unpremultiplied colour is the
  // requested red. Antialiased edges are excluded by the alpha threshold;
  // the channel means tolerate rounding through Qt's premultiply round-trip.
  const auto& t = node.outputs.main.texture;
  double sr = 0., sg = 0., sb = 0.;
  int strong = 0;
  for(int y = 0; y < t.height; y++)
    for(int x = 0; x < t.width; x++)
    {
      const Px p = pixel(node, x, y);
      if(p[3] >= 250)
      {
        sr += p[0];
        sg += p[1];
        sb += p[2];
        strong++;
      }
    }
  REQUIRE(strong > 0);
  CHECK(sr / strong > 200.);
  CHECK(sg / strong < 60.);
  CHECK(sb / strong < 60.);
}

TEST_CASE(
    "TextToTexture control update hook re-renders and re-raises changed",
    "[threedim][text_to_texture]")
{
  if(!hasFonts())
    SKIP("no fonts available on this platform");

  Threedim::TextToTexture node;
  node.inputs.canvas_w.value = 128;
  node.inputs.canvas_h.value = 64;
  node.recreate();

  // recreate() ends in upload(): the changed flag (the output's version /
  // re-render signal) must be up even though create() cleared it.
  CHECK(node.outputs.main.texture.changed);
  REQUIRE(paintedCount(node, transparent) > 0);

  // A consumer takes the frame and clears the flag; changing the text
  // through its port hook must repaint and raise it again.
  node.outputs.main.texture.changed = false;
  node.inputs.text.value = "";
  node.inputs.text.update(node);
  CHECK(node.outputs.main.texture.changed);
  CHECK(paintedCount(node, transparent) == 0); // content really re-rendered
}

TEST_CASE(
    "TextToTexture larger font size paints more pixels",
    "[threedim][text_to_texture]")
{
  if(!hasFonts())
    SKIP("no fonts available on this platform");

  Threedim::TextToTexture node;
  node.inputs.text.value = "Hello"; // single word: no wrap effects

  node.inputs.font_size.value = 8;
  node.recreate();
  const int small = paintedCount(node, transparent);
  REQUIRE(small > 0);

  node.inputs.font_size.value = 128;
  node.recreate();
  const int big = paintedCount(node, transparent);

  // Glyph area grows ~quadratically with pixel size (8 -> 128 is 16x
  // linear); 2x is a font-independent lower bound.
  CHECK(big > 2 * small);
}

TEST_CASE(
    "TextToTexture alignment controls move the glyphs",
    "[threedim][text_to_texture]")
{
  if(!hasFonts())
    SKIP("no fonts available on this platform");

  Threedim::TextToTexture node;
  node.inputs.text.value = "Hi"; // much smaller than the canvas in every font
  node.inputs.font_size.value = 32;
  node.inputs.canvas_w.value = 256;
  node.inputs.canvas_h.value = 256;

  node.inputs.h_align.value = 0; // left
  node.inputs.v_align.value = 0; // top
  node.recreate();
  const auto topLeft = paintedCentroid(node, transparent);
  REQUIRE(topLeft[0] >= 0.);

  node.inputs.h_align.value = 2; // right
  node.inputs.v_align.value = 2; // bottom
  node.recreate();
  const auto bottomRight = paintedCentroid(node, transparent);
  REQUIRE(bottomRight[0] >= 0.);

  CHECK(topLeft[0] < bottomRight[0]);
  CHECK(topLeft[1] < bottomRight[1]);
}
