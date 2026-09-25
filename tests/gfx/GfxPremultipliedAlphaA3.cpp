// =============================================================================
// N57: render targets between nodes hold premultiplied colour.
//
// Each shader output is composited "over" its target with the factors that
// match its ALPHA key (ISF default straight; CSF, raw raster, VSA default
// premultiplied); several cables into one input are drawn in source-node order,
// each over the previous. What this pins:
//   * a straight ISF stores (c * a, a), not the a^2 alpha of the old seed;
//   * CSF image copies and raw raster MRT blits composite premultiplied data
//     once (no second multiply, no overwrite of other cables);
//   * CSF / raw raster default to premultiplied, and ALPHA overrides it;
//   * an explicit PIPELINE_STATE.BLEND still wins over the ALPHA default;
//   * ISF MRT, multi-pass and persistent paths carry alpha to the output;
//   * the cable order into one input follows the source node, not insertion.
//
// Every image is read through fixc-opaque-view.fs, which shows the stored rgb
// on the left half and the stored alpha as grey on the right half.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage image;
};

template <typename Build>
Shot render_pipeline(score::gfx::GraphicsApi be, Build&& build)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int sink = build(p);
    if(sink < 0 || !p.error().empty())
    {
      r.error = p.error().empty() ? "pipeline build failed" : p.error();
      return;
    }
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(3);
    r.image = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
    if(r.error.empty() && !r.image.valid())
      r.error = "empty readback";
  });
  return r;
}

// Sources are added in the given order (so their node ids follow it) and wired
// into the view in the reverse order.
Shot stored(score::gfx::GraphicsApi be, std::vector<std::pair<const char*, int>> sources)
{
  return render_pipeline(be, [&](GfxPipeline& p) {
    std::vector<int> ids;
    for(auto [file, outIndex] : sources)
    {
      const QString path = corpus(file);
      const int id = path.endsWith(".cs") ? p.addCsf(path) : p.addIsf(path);
      if(id < 0)
        return -1;
      ids.push_back(id);
    }
    const int view = p.addIsf(corpus("fixc-opaque-view.fs"));
    const int sink = p.addSink({64, 64});
    if(view < 0)
      return -1;
    for(std::size_t i = ids.size(); i-- > 0;)
      p.wire(p.imageOut(ids[i], sources[i].second), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    return sink;
  });
}

Shot stored_raster(
    score::gfx::GraphicsApi be, const char* below, const char* rasterFs, int outIndex = 0)
{
  return render_pipeline(be, [&](GfxPipeline& p) {
    const int under = below ? p.addCsf(corpus(below)) : -2;
    const QString fs = corpus(rasterFs);
    const int rr = p.addRaster(QString{fs}.replace(".fs", ".vs"), fs);
    const int view = p.addIsf(corpus("fixc-opaque-view.fs"));
    const int sink = p.addSink({64, 64});
    if(under == -1 || rr < 0 || view < 0)
      return -1;
    p.wire(p.imageOut(rr, outIndex), p.imageIn(view, 0));
    if(under >= 0)
      p.wire(p.imageOut(under, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    return sink;
  });
}

#define A3_REQUIRE_LIVE(s, backend, compute)                                 \
  if((s).skipped)                                                            \
    SKIP((s).backend + ": " + (s).skip_reason);                              \
  if(compute)                                                                \
    if(const char* why = compute_shader_skip_reason(backend))                \
      SKIP(std::string{backend_name(backend)} + ": " + why);                 \
  CAPTURE((s).backend);                                                      \
  REQUIRE((s).error.empty());                                                \
  REQUIRE((s).image.valid())

void check_stored(const Shot& s, std::array<uint8_t, 4> rgb, uint8_t alpha)
{
  const auto c = s.image.at(16, 32);
  const auto a = s.image.at(48, 32);
  INFO("stored rgb = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  INFO("stored alpha = " << int(a[0]));
  CHECK(near(c, rgb, 3));
  CHECK(near(a, {alpha, alpha, alpha, 255}, 3));
}
}

TEST_CASE(
    "N57: a straight ISF stores premultiplied colour, alpha not squared",
    "[gfx][isf][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = stored(backend, {{"a3alpha-straight-red.fs", 0}});
  A3_REQUIRE_LIVE(s, backend, false);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "N57: a premultiplied passthrough composites the intermediate over black as "
    "half red",
    "[gfx][isf][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = render_pipeline(backend, [](GfxPipeline& p) {
    const int red = p.addIsf(corpus("a3alpha-straight-red.fs"));
    const int pass = p.addIsf(corpus("a3alpha-pass-premul.fs"));
    const int sink = p.addSink({64, 64});
    if(red < 0 || pass < 0)
      return -1;
    p.wire(p.imageOut(red, 0), p.imageIn(pass, 0));
    p.wire(p.imageOut(pass, 0), p.sinkInput(sink));
    return sink;
  });
  A3_REQUIRE_LIVE(s, backend, false);
  const auto c = s.image.center();
  INFO("output = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {128, 0, 0, 255}, 3));
}

TEST_CASE(
    "N57: a CSF image and a raw raster store their premultiplied output as written",
    "[gfx][csf][raster][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot csf = stored(backend, {{"a3alpha-csf-red.cs", 0}});
  A3_REQUIRE_LIVE(csf, backend, true);
  check_stored(csf, {128, 0, 0, 255}, 128);

  const Shot rr = stored_raster(backend, nullptr, "a3alpha-rr-green.fs");
  A3_REQUIRE_LIVE(rr, backend, false);
  check_stored(rr, {0, 128, 0, 255}, 128);
}

TEST_CASE(
    "N57: a CSF declaring ALPHA straight is premultiplied by its copy",
    "[gfx][csf][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = stored(backend, {{"a3alpha-csf-straight-red.cs", 0}});
  A3_REQUIRE_LIVE(s, backend, true);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "N57: an ISF and a CSF cabled into one input mix as premultiplied over, in "
    "source-node order",
    "[gfx][csf][isf][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Green (0, 0.5, 0, 0.5) first, red (0.5, 0, 0, 0.5) over it.
  const Shot s
      = stored(backend, {{"a3alpha-straight-green.fs", 0}, {"a3alpha-csf-red.cs", 0}});
  A3_REQUIRE_LIVE(s, backend, true);
  check_stored(s, {128, 64, 0, 255}, 191);
}

TEST_CASE(
    "N57: two CSF images cabled into one input mix as premultiplied over",
    "[gfx][csf][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Red first, green over it.
  const Shot s
      = stored(backend, {{"a3alpha-csf-red.cs", 0}, {"a3alpha-csf-green.cs", 0}});
  A3_REQUIRE_LIVE(s, backend, true);
  check_stored(s, {64, 128, 0, 255}, 191);
}

TEST_CASE(
    "N57: a raw raster composites premultiplied over another cable by default",
    "[gfx][csf][raster][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = stored_raster(backend, "a3alpha-csf-red.cs", "a3alpha-rr-green.fs");
  A3_REQUIRE_LIVE(s, backend, true);
  check_stored(s, {64, 128, 0, 255}, 191);
}

TEST_CASE(
    "N57: an explicit PIPELINE_STATE.BLEND wins over the ALPHA-derived blend",
    "[gfx][csf][raster][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = stored_raster(
      backend, "a3alpha-csf-green.cs", "a3alpha-rr-red-explicit-blend.fs");
  A3_REQUIRE_LIVE(s, backend, true);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "N57: the raw raster MRT blit does not multiply premultiplied data again",
    "[gfx][raster][mrt][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  for(int out : {0, 1})
  {
    CAPTURE(out);
    const Shot s = stored_raster(backend, nullptr, "a3alpha-rr-mrt.fs", out);
    A3_REQUIRE_LIVE(s, backend, false);
    check_stored(s, {128, 0, 0, 255}, 128);
  }
}

TEST_CASE(
    "N57: a straight ISF with several OUTPUTS stores premultiplied colour once",
    "[gfx][isf][mrt][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  for(int out : {0, 1})
  {
    CAPTURE(out);
    const Shot s = stored(backend, {{"a3alpha-isf-mrt.fs", out}});
    A3_REQUIRE_LIVE(s, backend, false);
    check_stored(s, {128, 0, 0, 255}, 128);
  }
}

TEST_CASE(
    "N57: a multi-pass ISF carries alpha from its intermediate target",
    "[gfx][isf][passes][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = stored(backend, {{"a3alpha-multipass.fs", 0}});
  A3_REQUIRE_LIVE(s, backend, false);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "N57: a persistent last pass is copied to the output once",
    "[gfx][isf][passes][alpha][a3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = stored(backend, {{"a3alpha-persistent.fs", 0}});
  A3_REQUIRE_LIVE(s, backend, false);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE("N57: the ALPHA key and its per-mode defaults", "[gfx][isf][alpha][a3]")
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int straight = p.addIsf(corpus("a3alpha-straight-red.fs"));
    const int premul = p.addIsf(corpus("a3alpha-pass-premul.fs"));
    const int csf = p.addCsf(corpus("a3alpha-csf-red.cs"));
    const int csfStraight = p.addCsf(corpus("a3alpha-csf-straight-red.cs"));
    const int rr = p.addRaster(corpus("a3alpha-rr-green.vs"), corpus("a3alpha-rr-green.fs"));
    REQUIRE(p.error().empty());
    using ::isf::alpha_mode;
    CHECK(::isf::resolve_alpha(p.isf(straight)->descriptor()) == alpha_mode::straight);
    CHECK(::isf::resolve_alpha(p.isf(premul)->descriptor()) == alpha_mode::premultiplied);
    CHECK(::isf::resolve_alpha(p.isf(csf)->descriptor()) == alpha_mode::premultiplied);
    CHECK(::isf::resolve_alpha(p.isf(csfStraight)->descriptor()) == alpha_mode::straight);
    CHECK(::isf::resolve_alpha(p.isf(rr)->descriptor()) == alpha_mode::premultiplied);

    const std::string badAlpha = R"_(/*{
  "ISFVSN": "2",
  "ALPHA": "premultipled"
}*/
void main() { gl_FragColor = vec4(1.0); }
)_";
    CHECK_THROWS(::isf::parser{
        std::string{}, badAlpha, 450, ::isf::parser::ShaderType::ISF});
  });
}
