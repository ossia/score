// A CPU texture input declaring source_size() reads its producer's texture at
// the producer's size, texel for texel, when the formats match.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
constexpr int PW = 7, PH = 3;

struct FloatProducer
{
  halp_meta(name, "Float producer")
  halp_meta(c_name, "test_source_size_float_producer")
  halp_meta(category, "Test")
  halp_meta(uuid, "5a7e2d14-c0b9-4f63-81a2-d94e6f3b7c58")
  struct
  {
  } inputs;
  struct
  {
    halp::texture_output<"Out", halp::rgba32f_texture> image;
  } outputs;
  void operator()()
  {
    outputs.image.create(PW, PH);
    float* px = outputs.image.texture.bytes;
    for(int y = 0; y < PH; y++)
      for(int x = 0; x < PW; x++)
      {
        float* p = px + (y * PW + x) * 4;
        p[0] = 1000.f + x;
        p[1] = -2000.f - y;
        p[2] = 0.125f * (x + y);
        p[3] = (x + y) % 2 ? 1.f : 0.f;
      }
    outputs.image.upload();
  }
};

struct ByteProducer
{
  halp_meta(name, "Byte producer")
  halp_meta(c_name, "test_source_size_byte_producer")
  halp_meta(category, "Test")
  halp_meta(uuid, "b81f4c37-2e6d-4a95-9c10-6e3a7d52f0b9")
  struct
  {
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  void operator()()
  {
    outputs.image.create(PW, PH);
    std::memset(outputs.image.texture.bytes, 200, PW * PH * 4);
    outputs.image.upload();
  }
};

struct SourceSizeInput : halp::texture_input<"In", halp::rgba32f_texture>
{
  static constexpr bool source_size() { return true; }
  bool resampled{};
};

struct SourceSizeConsumer
{
  halp_meta(name, "Source size consumer")
  halp_meta(c_name, "test_source_size_consumer")
  halp_meta(category, "Test")
  halp_meta(uuid, "0e9c6b52-74a1-4d38-b2f5-a81c3e6d9f07")
  struct
  {
    SourceSizeInput image;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  int width{}, height{};
  bool resampled{};
  std::vector<float> texels;
  void operator()()
  {
    const auto& t = inputs.image.texture;
    if(t.bytes)
    {
      width = t.width;
      height = t.height;
      resampled = inputs.image.resampled;
      texels.assign(t.bytes, t.bytes + std::size_t(t.width) * t.height * 4);
    }
    outputs.image.create(4, 4);
    std::memset(outputs.image.texture.bytes, 255, 4 * 4 * 4);
    outputs.image.upload();
  }
};

struct Result
{
  bool skipped{};
  std::string error;
  int width{}, height{};
  bool resampled{};
  std::vector<float> texels;
};

struct IsfProducer
{
};

template <typename Producer>
Result run(score::gfx::GraphicsApi api)
{
  Result res;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      res.error = "no document";
      return;
    }
    auto add = [&]<typename T>(GfxPipeline& p) {
      const int id = int(models.size()) + 1;
      auto model = std::make_unique<oscr::ProcessModel<T>>(
          TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{id}, doc->context(),
          nullptr);
      auto& m = *model;
      models.push_back(std::move(model));
      return p.addNode(std::unique_ptr<score::gfx::Node>{
          new oscr::GfxNode<T>{m, {}, Gfx::exec_controls{}, id, doc->context()}});
    };
    GfxPipeline p;
    const int cons = add.template operator()<SourceSizeConsumer>(p);
    if constexpr(std::is_same_v<Producer, IsfProducer>)
    {
      const int prod = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-green.fs"));
      if(prod < 0)
      {
        res.error = p.error();
        return;
      }
      p.wire(p.imageOut(prod, 0), p.node(cons)->input[0]);
    }
    else
    {
      const int prod = add.template operator()<Producer>(p);
      p.wire(p.nodeImageOut(prod, 0), p.node(cons)->input[0]);
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.nodeImageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      res.skipped = p.skipped();
      res.error = res.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    for(auto& [rl, r] : p.node(cons)->renderedNodes)
      if(r)
      {
        auto& s = *static_cast<oscr::GfxRenderer<SourceSizeConsumer>*>(r)->state;
        res.width = s.width;
        res.height = s.height;
        res.resampled = s.resampled;
        res.texels = s.texels;
      }
  });
  return res;
}

#define SRC_REQUIRE_LIVE(o)      \
  if((o).skipped)                \
    SKIP("backend unavailable"); \
  INFO("error=" << (o).error);   \
  REQUIRE((o).error.empty())
}

TEST_CASE(
    "a source_size input reads a float producer at its size, texel for texel",
    "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const auto r = run<FloatProducer>(api);
  SRC_REQUIRE_LIVE(r);
  CHECK_FALSE(r.resampled);
  REQUIRE(r.width == PW);
  REQUIRE(r.height == PH);
  REQUIRE(r.texels.size() == std::size_t(PW * PH * 4));
  for(int y = 0; y < PH; y++)
    for(int x = 0; x < PW; x++)
    {
      const float* p = r.texels.data() + (y * PW + x) * 4;
      INFO(x << "," << y);
      CHECK(p[0] == 1000.f + x);
      CHECK(p[1] == -2000.f - y);
      CHECK(p[2] == 0.125f * (x + y));
      CHECK(p[3] == ((x + y) % 2 ? 1.f : 0.f));
    }
}

TEST_CASE(
    "a source_size input of another format reads a resampled copy and says so",
    "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const auto r = run<ByteProducer>(api);
  SRC_REQUIRE_LIVE(r);
  CHECK(r.resampled);
  CHECK(r.width > 0);
}

TEST_CASE(
    "a source_size input a shader draws into is not resampled",
    "[gfx][avnd][readback]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const auto r = run<IsfProducer>(api);
  SRC_REQUIRE_LIVE(r);
  CHECK_FALSE(r.resampled);
  CHECK(r.width > 0);
}
