// A gpp compute node that is the output node of its render list, with two
// wired image inputs (agent L4).
//
// The compute node reads one texel of each input image into a storage buffer
// and reads it back; each image must hold its producer's colour, whether the
// two inputs are fed by two producers or by one, and an unwired first input
// is cleared to opaque black (the output node's clear colour) while the
// second one is fed.
//
// Registration: see the test_gfx_compute_output_two_images_l4 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/GpuComputeNode.hpp>

#include <gpp/commands.hpp>
#include <gpp/layout.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <memory>
#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct TwoImageProbe
{
  halp_meta(name, "L4 two image probe")
  halp_meta(uuid, "9a4e6c21-3d7b-4f85-b0c2-5e8d1a7f4b39")

  struct layout
  {
    halp_meta(local_size_x, 1)
    halp_meta(local_size_y, 1)
    halp_meta(local_size_z, 1)
    halp_flags(compute);

    struct bindings
    {
      struct
      {
        halp_meta(name, "probe_buf");
        halp_meta(binding, 0);
        halp_flags(std140, buffer, load, store);

        using color = float[4];
        gpp::uniform<"result", color*> values;
      } buf;

      struct
      {
        halp_meta(name, "img_a")
        halp_meta(format, "rgba8")
        halp_meta(binding, 1);
        halp_flags(image2D, readonly);
      } a;

      struct
      {
        halp_meta(name, "img_b")
        halp_meta(format, "rgba8")
        halp_meta(binding, 2);
        halp_flags(image2D, readonly);
      } b;
    } bindings;
  };

  using bindings = decltype(layout::bindings);

  struct
  {
    gpp::image_input_port<"A", &bindings::a> a;
    gpp::image_input_port<"B", &bindings::b> b;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "a")
      float value[4];
    } a;
    struct
    {
      halp_meta(name, "b")
      float value[4];
    } b;
  } outputs;

  std::string_view compute()
  {
    return R"_(
void main()
{
  result[0] = imageLoad(img_a, ivec2(1, 1));
  result[1] = imageLoad(img_b, ivec2(1, 1));
}
)_";
  }

  gpp::co_update update()
  {
    if(!buf)
      buf = co_yield gpp::static_allocation{
          .binding = lay.bindings.buf.binding(), .size = bytes};
  }

  gpp::co_release release()
  {
    if(buf)
    {
      co_yield gpp::buffer_release{.handle = buf};
      buf = nullptr;
    }
  }

  gpp::co_dispatch dispatch()
  {
    if(!buf)
      co_return;
    co_yield gpp::begin_compute_pass{};
    co_yield gpp::compute_dispatch{.x = 1, .y = 1, .z = 1};
    gpp::buffer_awaiter readback
        = co_yield gpp::readback_buffer{.handle = buf, .offset = 0, .size = bytes};
    co_yield gpp::end_compute_pass{};
    auto [data, size] = co_yield readback;
    if(!data || size < std::size_t(bytes))
      co_return;
    auto flt = reinterpret_cast<const float*>(data);
    std::copy_n(flt, 4, outputs.a.value);
    std::copy_n(flt + 4, 4, outputs.b.value);
  }

  static constexpr auto lay = layout{};
  static constexpr int bytes = 2 * 4 * sizeof(float);
  gpp::buffer_handle buf{};
};

struct Result
{
  bool skipped{};
  std::string error;
  float a[4]{-1.f, -1.f, -1.f, -1.f};
  float b[4]{-1.f, -1.f, -1.f, -1.f};
};

enum class Wiring
{
  TwoProducers,
  OneProducer,
  SecondOnly
};

Result run(score::gfx::GraphicsApi api, Wiring wiring)
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
    const int magenta = p.addIsf(corpus("isf-solid-color.fs"));
    const int blue = wiring == Wiring::OneProducer
                         ? magenta
                         : p.addIsf(corpus("fixg-solid-color.fs"));
    auto owned = std::make_unique<oscr::GpuComputeNode<TwoImageProbe>>(
        std::weak_ptr<Execution::ExecutionCommandQueue>{}, Gfx::exec_controls{}, 1,
        doc->context());
    auto* node = owned.get();
    const int idx = p.addNode(std::move(owned));
    if(magenta < 0 || blue < 0 || idx < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    if(wiring != Wiring::SecondOnly)
      p.wire(p.imageOut(magenta, 0), node->input[0]);
    p.wire(p.imageOut(blue, 0), node->input[1]);
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    auto st = node->renderState();
    if(!st || !st->rhi || !st->rhi->isFeatureSupported(QRhi::Compute))
    {
      r.skipped = true;
      return;
    }
    for(int i = 0; i < 4; i++)
    {
      p.render(1);
      node->render();
    }
    if(node->renderedNodes.empty())
    {
      r.error = "no renderer";
      return;
    }
    auto* rn = dynamic_cast<oscr::GpuComputeRenderer<TwoImageProbe>*>(
        node->renderedNodes.begin()->second);
    if(!rn || !rn->state)
    {
      r.error = "not a compute renderer";
      return;
    }
    std::copy_n(rn->state->outputs.a.value, 4, r.a);
    std::copy_n(rn->state->outputs.b.value, 4, r.b);
  });
  return r;
}

void checkColor(const float (&got)[4], std::array<float, 4> want)
{
  INFO("got " << got[0] << " " << got[1] << " " << got[2] << " " << got[3]);
  for(int i = 0; i < 4; i++)
    CHECK(got[i] == Catch::Approx(want[i]).margin(0.01));
}
}

TEST_CASE(
    "A compute output node with two image inputs fed by two producers",
    "[gfx][avnd][gpp][compute][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Result r = run(api, Wiring::TwoProducers);
  if(r.skipped)
    SKIP("backend unavailable or without compute");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  checkColor(r.a, {1.f, 0.f, 1.f, 1.f});
  checkColor(r.b, {0.25f, 0.5f, 0.75f, 1.f});
}

TEST_CASE(
    "A compute output node with two image inputs fed by one producer",
    "[gfx][avnd][gpp][compute][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Result r = run(api, Wiring::OneProducer);
  if(r.skipped)
    SKIP("backend unavailable or without compute");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  checkColor(r.a, {1.f, 0.f, 1.f, 1.f});
  checkColor(r.b, {1.f, 0.f, 1.f, 1.f});
}

TEST_CASE(
    "A compute output node with only its second image input wired",
    "[gfx][avnd][gpp][compute][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Result r = run(api, Wiring::SecondOnly);
  if(r.skipped)
    SKIP("backend unavailable or without compute");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  checkColor(r.a, {0.f, 0.f, 0.f, 1.f});
  checkColor(r.b, {0.25f, 0.5f, 0.75f, 1.f});
}
