// A texture inlet's filter change reaches CSF, raw raster and VSA nodes at
// runtime.
//
// RenderList applies a filter/address change on an input through
// NodeRenderer::updateInputSamplerFilter, and only rebuilds the input target
// when its size or format changes. The ISF renderers override it; the CSF, raw
// raster and VSA renderers did not, so their samplers kept the filter they were
// created with until the whole renderer was rebuilt.
//
// Pinned here: an ISF writes one black and one red texel per row into the
// consumer's 2x2 input target, the consumer samples it at u = 0.4 and outputs
// the red: 0.3 through a linear sampler, 0 through a nearest one. The inlet is
// switched Linear -> Nearest -> Linear at the same size, and the output follows,
// on every backend this machine brings up.
//
// Registration:
//   score_add_gfx_test(input_sampler_filter_i1 GfxInputSamplerFilterI1.cpp)
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

int rawImageInputIndex(const score::gfx::Node& n)
{
  for(std::size_t i = 0; i < n.input.size(); ++i)
    if(n.input[i]->type == score::gfx::Types::Image)
      return int(i);
  return -1;
}

ossia::render_target_spec twoByTwo(ossia::texture_filter f)
{
  ossia::render_target_spec spec;
  spec.size = ossia::texture_size{2, 2};
  spec.mag_filter = f;
  spec.min_filter = f;
  return spec;
}

struct Shots
{
  bool skipped{};
  std::string skip_reason;
  std::string backend;
  std::string error;
  ReadbackImage linear, nearest, linearAgain;
};
}

TEST_CASE(
    "an inlet's filter change reaches CSF, raw raster and VSA samplers",
    "[gfx][incremental][sampler-filter]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string kind = GENERATE(std::string("csf"), std::string("raster"), std::string("vsa"));
  CAPTURE(backend_name(api), kind);

  Shots out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int up = p.addIsf(corpus("isf-i1-stripes.fs"));
    int c = -1;
    if(kind == "csf")
      c = p.addCsf(corpus("csf-i1-filter-probe.cs"));
    else if(kind == "raster")
      c = p.addRaster(corpus("rr-i1-filter-probe.vs"), corpus("rr-i1-filter-probe.fs"));
    else
      c = p.addVsa(corpus("vsa-i1-filter-probe.vs"));
    if(up < 0 || c < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(up, 0), p.imageIn(c, 0));
    p.wire(p.imageOut(c, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();

    auto& node = *p.isf(c);
    const int port = rawImageInputIndex(node);
    if(port < 0)
    {
      out.error = "consumer has no image input";
      return;
    }

    setRenderTargetSpec(node, port, twoByTwo(ossia::texture_filter::LINEAR));
    p.render(3);
    out.linear = p.readback(sink);

    setRenderTargetSpec(node, port, twoByTwo(ossia::texture_filter::NEAREST));
    p.render(3);
    out.nearest = p.readback(sink);

    setRenderTargetSpec(node, port, twoByTwo(ossia::texture_filter::LINEAR));
    p.render(3);
    out.linearAgain = p.readback(sink);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  if(kind == "csf")
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);
  INFO("backend=" << out.backend << " error=" << out.error);
  REQUIRE(out.error.empty());
  REQUIRE(out.linear.valid());
  REQUIRE(out.nearest.valid());
  REQUIRE(out.linearAgain.valid());

  const int linear = out.linear.center()[0];
  const int nearest = out.nearest.center()[0];
  const int linearAgain = out.linearAgain.center()[0];
  INFO("red: linear=" << linear << " nearest=" << nearest << " linear again=" << linearAgain);
  CHECK(std::abs(linear - 77) <= 12);
  CHECK(nearest <= 8);
  CHECK(std::abs(linearAgain - 77) <= 12);
}
