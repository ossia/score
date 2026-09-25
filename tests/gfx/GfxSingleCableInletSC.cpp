// Single-cable texture inlets (agent SC).
//
// An avnd input declaring halp_flag(single_cable) marks its Process::Port
// singleCable (on creation and on load: the flag is not serialized). A texture
// inlet with it reads the upstream's own texture when the upstream publishes
// one (no composite into the inlet's render target, whatever size or format is
// set on the inlet), and its render target otherwise; the choice follows the
// cable when it changes. A normal texture inlet (Texture to buffer) keeps
// mixing every cable into its render target.
//
// Registration: see the test_gfx_single_cable_inlet_sc target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <Threedim/BufferInfo.hpp>
#include <Threedim/GeometryInfo.hpp>
#include <Threedim/TextureInfo.hpp>
#include <Threedim/TextureToBuffer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;

  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

score::gfx::Port* firstInput(score::gfx::Node& n, score::gfx::Types t)
{
  for(auto* port : n.input)
    if(port->type == t)
      return port;
  return nullptr;
}

ossia::render_target_spec setSpec()
{
  ossia::render_target_spec s;
  s.size = ossia::texture_size{64, 48};
  s.format = ossia::texture_format::RGBA8;
  s.format_set = true;
  return s;
}

struct Producer
{
  const char* path;
  bool compute;
};

const Producer csf37{"sc-csf-image-37x23-rgba32f.cs", true};
const Producer isfSolid{"isf-solid-color.fs", false};

struct Info
{
  int width{}, height{};
  std::string format;
  bool readsUpstream{};
  bool readsRenderTarget{};
  bool composited{};
  bool hasSampler{};
};

struct InfoRun
{
  bool skipped{};
  std::string error;
  std::vector<Info> steps;
};

// producers[0] -> Texture Info; each further producer replaces the cable
// through the incremental edge path.
InfoRun runTextureInfo(
    score::gfx::GraphicsApi api, std::vector<Producer> producers,
    std::optional<ossia::render_target_spec> spec)
{
  InfoRun out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;

    std::vector<int> prods;
    for(auto& pr : producers)
    {
      const int idx = pr.compute ? p.addCsf(corpus(pr.path)) : p.addIsf(corpus(pr.path));
      if(idx < 0)
      {
        out.error = "producer build failed: " + p.error();
        return;
      }
      prods.push_back(idx);
    }
    auto infoOwned = procs.make<Threedim::TextureInfo>(ctx);
    auto* infoNode = infoOwned.get();
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int ti = p.addNode(std::move(infoOwned));
    if(spec)
      p.node(ti)->process(0, *spec);
    auto* in = firstInput(*p.node(ti), score::gfx::Types::Image);
    REQUIRE(in);
    p.wire(p.imageOut(prods[0], 0), in);

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }

    for(std::size_t k = 0; k < prods.size(); ++k)
    {
      const int prod = prods[k];
      if(k > 0)
      {
        p.removeEdgeIncremental(p.imageOut(prods[k - 1], 0), in);
        p.addEdgeIncremental(p.imageOut(prod, 0), in);
      }
      for(int f = 0; f < 4; ++f)
      {
        p.render(1);
        info->render();
      }

      REQUIRE(!infoNode->renderedNodes.empty());
      auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureInfo>*>(
          infoNode->renderedNodes.begin()->second);
      REQUIRE(rn);
      REQUIRE(in->edges.size() == 1);
      Info step;
      step.width = rn->state->outputs.width.value;
      step.height = rn->state->outputs.height.value;
      step.format = rn->state->outputs.format.value;
      const auto& t = rn->state->inputs.texture.texture;
      REQUIRE(!p.isf(prod)->renderedNodes.empty());
      auto* upstream = p.isf(prod)->renderedNodes.begin()->second;
      auto* upTex = upstream->textureForOutput(*p.imageOut(prod, 0));
      step.readsUpstream = upTex && t.handle == upTex;
      auto* rt = rn->renderTargetForInput(*in).texture;
      step.readsRenderTarget = rt && t.handle == rt;
      step.composited = upstream->hasOutputPassForEdge(*in->edges.front());
      step.hasSampler = t.sampler_handle != nullptr;
      out.steps.push_back(step);
    }
  });
  return out;
}

struct T2B
{
  bool skipped{};
  std::string error;
  int width{}, height{};
  bool hasRenderTarget{};
  std::vector<unsigned char> bytes;
};

T2B runMixedTextureToBuffer(score::gfx::GraphicsApi api)
{
  T2B out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;

    const int left = p.addIsf(corpus("i6-half-left.fs"));
    const int right = p.addIsf(corpus("i6-half-right.fs"));
    if(left < 0 || right < 0)
    {
      out.error = "producer build failed: " + p.error();
      return;
    }
    auto t2bOwned = procs.make<Threedim::TextureToBuffer>(ctx);
    auto* t2bNode = t2bOwned.get();
    const int t2b = p.addNode(std::move(t2bOwned));
    auto infoOwned = procs.make<Threedim::BufferInfo>(ctx);
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int bi = p.addNode(std::move(infoOwned));

    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{16, 8};
    spec.mag_filter = ossia::texture_filter::NEAREST;
    spec.min_filter = ossia::texture_filter::NEAREST;
    p.node(t2b)->process(0, spec);
    auto* tin = firstInput(*p.node(t2b), score::gfx::Types::Image);
    REQUIRE(tin);
    p.wire(p.imageOut(left, 0), tin);
    p.wire(p.imageOut(right, 0), tin);
    p.wire(p.nodeBufferOut(t2b, 0), firstInput(*p.node(bi), score::gfx::Types::Buffer));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < 6; ++f)
    {
      p.render(1);
      info->render();
    }

    REQUIRE(!t2bNode->renderedNodes.empty());
    auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureToBuffer>*>(
        t2bNode->renderedNodes.begin()->second);
    REQUIRE(rn);
    out.hasRenderTarget = rn->renderTargetForInput(*tin).texture != nullptr;
    const auto& t = rn->state->inputs.texture.texture;
    out.width = t.width;
    out.height = t.height;
    if(t.bytes && t.bytesize() > 0)
      out.bytes.assign(t.bytes, t.bytes + t.bytesize());
  });
  return out;
}

std::array<int, 4> texel8(const T2B& r, int x, int y)
{
  std::array<int, 4> px{-1, -1, -1, -1};
  const std::size_t off = (std::size_t(y) * r.width + x) * 4;
  if(off + 4 <= r.bytes.size())
    for(int i = 0; i < 4; ++i)
      px[i] = r.bytes[off + i];
  return px;
}

bool firstInletSingleCable(const Process::ProcessModel& m)
{
  return !m.inlets().empty() && m.inlets().front()->singleCable;
}
}

TEST_CASE(
    "Info node inlets are single-cable, on creation and after loading",
    "[gfx][avnd][texture][single-cable][sc]")
{
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    REQUIRE(doc);
    const auto& ctx = doc->context();

    auto check = [&]<typename T>(bool expected) {
      oscr::ProcessModel<T> model{
          TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr};
      CHECK(firstInletSingleCable(model) == expected);

      auto reader
          = score::marshall<JSONObject>(static_cast<const Process::ProcessModel&>(model));
      rapidjson::Document json = readJson(reader.toByteArray());
      REQUIRE(!json.HasParseError());
      JSONObject::Deserializer des{json};
      oscr::ProcessModel<T> loaded{des, nullptr};
      CHECK(firstInletSingleCable(loaded) == expected);
    };
    check.template operator()<Threedim::TextureInfo>(true);
    check.template operator()<Threedim::BufferInfo>(true);
    check.template operator()<Threedim::GeometryInfo>(true);
    check.template operator()<Threedim::TextureToBuffer>(false);
  });
}

TEST_CASE(
    "Texture Info reads the upstream texture through its single-cable inlet",
    "[gfx][avnd][texture][single-cable][sc]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  for(auto spec : {std::optional{setSpec()}, std::optional<ossia::render_target_spec>{}})
  {
    const InfoRun r = runTextureInfo(api, {csf37}, spec);
    if(r.skipped)
      SKIP("backend unavailable");
    INFO("error=" << r.error << " spec set=" << spec.has_value());
    REQUIRE(r.error.empty());
    REQUIRE(r.steps.size() == 1);
    const auto& s = r.steps[0];
    CHECK(s.width == 37);
    CHECK(s.height == 23);
    CHECK(s.format == "RGBA32F");
    CHECK(s.readsUpstream);
    CHECK(!s.composited);
    CHECK(s.hasSampler);
  }
}

TEST_CASE(
    "A single-cable inlet fed by a node publishing no texture reads its render target",
    "[gfx][avnd][texture][single-cable][sc]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const InfoRun unset = runTextureInfo(api, {isfSolid}, std::nullopt);
  if(unset.skipped)
    SKIP("backend unavailable");
  INFO("error=" << unset.error);
  REQUIRE(unset.error.empty());
  REQUIRE(unset.steps.size() == 1);
  const auto sz = oscr::CustomGpuOutputNodeBase::defaultRenderSize;
  CHECK(unset.steps[0].width == sz.width());
  CHECK(unset.steps[0].height == sz.height());
  CHECK(unset.steps[0].format == "RGBA8");
  CHECK(unset.steps[0].readsRenderTarget);
  CHECK(unset.steps[0].composited);

  const InfoRun set = runTextureInfo(api, {isfSolid}, setSpec());
  INFO("error=" << set.error);
  REQUIRE(set.error.empty());
  REQUIRE(set.steps.size() == 1);
  CHECK(set.steps[0].width == 64);
  CHECK(set.steps[0].height == 48);
  CHECK(set.steps[0].format == "RGBA8");
  CHECK(set.steps[0].readsRenderTarget);
  CHECK(set.steps[0].composited);
}

TEST_CASE(
    "A single-cable inlet follows its upstream when the cable changes",
    "[gfx][avnd][texture][single-cable][sc]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const InfoRun r = runTextureInfo(api, {csf37, isfSolid, csf37}, setSpec());
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.steps.size() == 3);
  for(int k : {0, 2})
  {
    INFO("step " << k);
    CHECK(r.steps[k].width == 37);
    CHECK(r.steps[k].height == 23);
    CHECK(r.steps[k].format == "RGBA32F");
    CHECK(r.steps[k].readsUpstream);
    CHECK(!r.steps[k].composited);
  }
  CHECK(r.steps[1].width == 64);
  CHECK(r.steps[1].height == 48);
  CHECK(r.steps[1].format == "RGBA8");
  CHECK(r.steps[1].readsRenderTarget);
  CHECK(r.steps[1].composited);
}

TEST_CASE(
    "A multi-cable texture inlet still mixes its cables in a render target",
    "[gfx][avnd][texture][single-cable][sc]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const T2B r = runMixedTextureToBuffer(api);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.hasRenderTarget);
  REQUIRE(r.width == 16);
  REQUIRE(r.height == 8);
  const auto left = texel8(r, 2, 4);
  const auto right = texel8(r, 13, 4);
  INFO("left " << left[0] << "," << left[1] << " right " << right[0] << ","
               << right[1]);
  CHECK(left[0] == 255);
  CHECK(left[1] == 0);
  CHECK(right[0] == 0);
  CHECK(right[1] == 255);
}
