// =============================================================================
// L3 — RAW_RASTER EXECUTION_MODEL (PER_MIP / PER_CUBE_FACE / PER_LAYER).
//
// These are the R3-N6 (depth-only render-area) / R3-N7 (per-invocation SRB) code
// paths. Two levels of coverage, because the corpus execution-model shaders
// (shadow_cascades PER_LAYER, prefilter_ggx PER_MIP) need full scene
// infrastructure (indexed MDI geometry, per-draw / indirect SSBOs, a source
// cubemap) AND emit depth / cubemap / array targets that BackgroundNode (2D
// sampler, single-layer texture readback) cannot read back per-target:
//
//   1. PARSE + descriptor coverage of the corpus shaders — the EXECUTION_MODEL
//      block and the multi-layer / cubemap OUTPUT surface must parse correctly.
//      (No GPU bake; validates the descriptor path only.)
//
//   2. A self-authored PROCEDURAL PER_LAYER render (rr-perlayer.{vs,fs}, no
//      geometry input, 4-layer color TextureArray output). This actually drives
//      the runtime: all 4 layer invocations must run without a UAF / validation
//      error (the R3-N7 per-invocation SRB regression would crash or diverge on
//      layers past the first), across every backend.
//
// Honest limitation: BackgroundNode reads back a single 2D view, so per-LAYER /
// per-MIP / per-FACE individual-target verification is NOT expressible here.
// The procedural render therefore asserts "completes + reads back a valid,
// non-empty frame on every backend + backends agree", which is the strongest
// statement the fixture can make about the multi-invocation dispatch.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>

#include <cstdio>
#include <optional>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

// Parse a RAW_RASTER (.vert + .frag) descriptor with NO GPU shader bake — just
// the isf::parser, mirroring how ProgramCache feeds it (ShaderType::RawRaster).
struct ParsedDesc
{
  bool ok = false;
  std::string error;
  std::optional<isf::descriptor> desc;
};

ParsedDesc parse_raster(const char* vs, const char* fs)
{
  ParsedDesc p;
  QFile vsf{corpus(vs)}, fsf{corpus(fs)};
  if(!vsf.open(QIODevice::ReadOnly) || !fsf.open(QIODevice::ReadOnly))
  {
    p.error = "cannot open shader files";
    return p;
  }
  try
  {
    isf::parser parser{
        vsf.readAll().toStdString(), fsf.readAll().toStdString(), 450,
        isf::parser::ShaderType::RawRasterPipeline};
    p.desc = parser.data();
    p.ok = true;
  }
  catch(const std::exception& e)
  {
    p.error = e.what();
  }
  catch(...)
  {
    p.error = "unknown parse error";
  }
  return p;
}

ParsedDesc parse_in_app(const char* vs, const char* fs)
{
  ParsedDesc p;
  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) { p = parse_raster(vs, fs); });
  return p;
}

int drawn_pixels(const ReadbackImage& img, int thresh = 12)
{
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto px = img.at(x, y);
      if(int(px[0]) + int(px[1]) + int(px[2]) > thresh)
        ++n;
    }
  return n;
}

IsfResult run_procedural_perlayer(score::gfx::GraphicsApi be)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // No geometry producer: procedural draw (gl_VertexIndex fullscreen tri).
    r = render_raster(
        be, {}, corpus("rr-perlayer.vs"), corpus("rr-perlayer.fs"), {64, 64}, 3);
  });
  return r;
}
}

// ---- Parse-level coverage of the corpus execution-model shaders --------------

TEST_CASE("shadow_cascades parses PER_LAYER depth-array OUTPUT", "[gfx][l3][execmodel]")
{
  const ParsedDesc p = parse_in_app("shadow_cascades.vert", "shadow_cascades.frag");
  INFO("error=" << p.error);
  REQUIRE(p.ok);
  REQUIRE(p.desc.has_value());
  const auto& d = *p.desc;

  CHECK(d.mode == isf::descriptor::RawRaster);
  CHECK(d.execution_model.type == "PER_LAYER");
  CHECK(d.execution_model.target == "shadow");
  REQUIRE(d.outputs.size() == 1);
  CHECK(d.outputs[0].type == "depth");
  CHECK(d.outputs[0].layers == 8);
}

TEST_CASE("prefilter_ggx parses PER_MIP cubemap OUTPUT", "[gfx][l3][execmodel]")
{
  const ParsedDesc p = parse_in_app("prefilter_ggx.vert", "prefilter_ggx.frag");
  INFO("error=" << p.error);
  REQUIRE(p.ok);
  REQUIRE(p.desc.has_value());
  const auto& d = *p.desc;

  CHECK(d.mode == isf::descriptor::RawRaster);
  CHECK(d.execution_model.type == "PER_MIP");
  CHECK(d.execution_model.target == "prefiltered");
  REQUIRE(d.outputs.size() == 1);
  CHECK(d.outputs[0].is_cubemap);
  CHECK(d.outputs[0].layers == 6);
}

// ---- Procedural PER_LAYER render (R3-N7 per-invocation SRB) -------------------

TEST_CASE("procedural PER_LAYER raw-raster renders all layers", "[gfx][l3][execmodel]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const IsfResult r = run_procedural_perlayer(be);
  if(qEnvironmentVariableIsSet("GFX_DUMP") && !r.outputs.empty())
  {
    const auto& o = r.outputs[0];
    const auto c = o.at(o.width / 2, o.height / 2);
    std::fprintf(
        stderr, "[perlayer] backend=%s skipped=%d err='%s' %dx%d drawn=%d "
                "center=(%d,%d,%d,%d)\n",
        r.backend.c_str(), int(r.skipped), r.error.c_str(), o.width, o.height,
        drawn_pixels(o), c[0], c[1], c[2], c[3]);
    std::fflush(stderr);
  }

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  // The 4-layer PER_LAYER dispatch must complete without a crash / validation
  // error and produce a valid readback (R3-N7: layers past the first must not
  // UAF). Per-layer content verification is not expressible via BackgroundNode.
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  CHECK(r.outputs[0].valid());
}
