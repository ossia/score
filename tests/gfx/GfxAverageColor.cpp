// The avendish "Average color" example returns the average of its input.
// A solid opaque colour must average to itself, alpha included: the block sums
// are strided by the buffer's width and divided by the pixel count, not the
// block count.
#include "IsfTestCommon.hpp"

#include <score_test/Document.hpp>

#include <Crousti/GpuComputeNode.hpp>

#include <Gpu/Compute.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{

using AverageColor = examples::GpuComputeExample;

struct Result
{
  bool skipped{};
  std::string error;
  float color[4]{-1.f, -1.f, -1.f, -1.f};
};

Result average(score::gfx::GraphicsApi api, QSize size)
{
  Result r;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      r.error = "no document";
      return;
    }

    GfxPipeline p;
    const int image = p.addIsf(corpus("isf-solid-color-opaque.fs"));
    auto node = std::make_unique<oscr::GpuComputeNode<AverageColor>>(
        std::weak_ptr<Execution::ExecutionCommandQueue>{}, Gfx::exec_controls{}, 1,
        doc->context());
    auto* avg = node.get();

    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{size.width(), size.height()};
    spec.format = ossia::texture_format::RGBA32F;
    avg->process(0, spec);

    const int idx = p.addNode(std::move(node));
    if(image < 0 || idx < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(image, 0), avg->input[0]);
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }

    auto st = avg->renderState();
    if(!st || !st->rhi)
    {
      r.skipped = true;
      return;
    }
    if(!st->rhi->isFeatureSupported(QRhi::Compute))
    {
      r.skipped = true;
      return;
    }

    for(int i = 0; i < 4; i++)
    {
      p.render(1);
      avg->render();
    }

    if(avg->renderedNodes.empty())
    {
      r.error = "no renderer";
      return;
    }
    auto* renderer = dynamic_cast<oscr::GpuComputeRenderer<AverageColor>*>(
        avg->renderedNodes.begin()->second);
    if(!renderer || !renderer->state)
    {
      r.error = "not a compute renderer";
      return;
    }
    std::copy_n(renderer->state->outputs.color_out.value, 4, r.color);
  });
  return r;
}
}

TEST_CASE(
    "Average color of a solid opaque image is that colour",
    "[gfx][avnd][compute][average_color]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // The node's Width and Height default to 100: less than the image, and not a
  // multiple of its 16 pixel blocks.
  const auto r = average(api, QSize{128, 128});
  if(r.skipped)
    SKIP("backend unavailable or without compute");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());

  INFO(
      "average " << r.color[0] << " " << r.color[1] << " " << r.color[2] << " "
                 << r.color[3]);
  // The source is RGBA8: 0.25 reads back as 64 / 255.
  CHECK(r.color[0] == Catch::Approx(0.25).margin(0.01));
  CHECK(r.color[1] == Catch::Approx(0.5).margin(0.01));
  CHECK(r.color[2] == Catch::Approx(0.75).margin(0.01));
  CHECK(r.color[3] == Catch::Approx(1.0).margin(0.01));
}
