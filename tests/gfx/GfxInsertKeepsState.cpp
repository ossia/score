// Inserting a node into a running graph must leave the nodes already rendering
// alone: a feedback shader keeps its accumulated image, a simulation keeps its
// buffers. Only the new node and the passes the new cables need are built.
// Resizing the output (a window, or the score view a background output follows,
// which moves whenever a side panel opens) rebuilds what follows the output size
// and nothing else.
//
// Driven through the real document GfxContext, the way a process inserted
// while playing reaches it: GfxContext::register_node for the new node, the
// execution's edge set republished through GfxExecutionAction, then
// GfxContext::renderFrames, which runs updateGraph (incrementalEdgeUpdate)
// before drawing.
//
// The state probe is a PERSISTENT pass adding 16/255 to its own red every frame
// (isf-persistent-last-pass-accumulate.fs, the multi-pass ISF renderer), or a
// persistent storage buffer counting frames (isf-persistent-counter.fs, the
// single-pass renderer). Both read 16 per frame rendered since the renderer was
// built: 48 after three frames, 64 after four. A renderer torn down and
// rebuilt by the change reads 16 on the frame after it. The pass textures of the
// first follow the size of what the probe draws into, so a resize of that
// target starts them over by design; the storage buffer of the second does not.
//
// The inspector's texture-port preview is an output of its own, rendering the
// producer's chain again in its own render list. It is built at whatever size
// the widget has when the process is selected, and resized when the inspector
// lays it out: right away, or only once the inspector is shown again when the
// selection happened behind a side-panel script editor. That resize must not
// start the chain it shows over either.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Widgets/RhiPreviewWidget.hpp>

#include <core/document/Document.hpp>

#include <QCoreApplication>
#include <QResizeEvent>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>
#include <vector>

namespace
{
QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}

QString apiSettingName(score::gfx::GraphicsApi api)
{
  const Gfx::Settings::GraphicsApis apis{};
  switch(api)
  {
    case score::gfx::Vulkan:
      return apis.Vulkan;
    case score::gfx::Metal:
      return apis.Metal;
    case score::gfx::D3D11:
      return apis.D3D11;
    case score::gfx::D3D12:
      return apis.D3D12;
    default:
      return apis.OpenGL;
  }
}

enum class Change
{
  //! A node registered with no cable.
  Unconnected,
  //! probe -> mix.A -> sink; the new node feeds mix.B.
  SecondInput,
  //! Same, and the new node asks its render list for depth.
  SecondInputWithDepth,
  //! probe -> sink; the new node, asking for depth, feeds the same sink.
  SameSinkWithDepth,
  //! probe -> sink becomes probe -> new -> sink.
  Between,
  //! probe -> sink, and the sink is resized.
  ResizeSink,
  //! probe -> mix.A -> sink, and the sink is resized.
  ResizeSinkThroughMix,
};

const char* name(Change i)
{
  switch(i)
  {
    case Change::Unconnected:
      return "unconnected";
    case Change::SecondInput:
      return "second input";
    case Change::SecondInputWithDepth:
      return "second input, needs depth";
    case Change::SameSinkWithDepth:
      return "same sink, needs depth";
    case Change::Between:
      return "between the probe and the sink";
    case Change::ResizeSink:
      return "sink resized";
    case Change::ResizeSinkThroughMix:
      return "sink resized, probe through a mix";
  }
  return "";
}

struct Outcome
{
  std::string skip;
  std::string error;
  int before{-1};
  int after{-1};
};

Outcome run(score::gfx::GraphicsApi backend, const char* probeShader, Change kind)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto& settings = app.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setRate(60.);
    settings.setGraphicsApi(apiSettingName(backend));

    std::string probed;
    if(!score::test::gfx::probe_api(backend, probed))
    {
      out.skip = std::string("RHI backend '") + score::test::gfx::backend_name(backend)
                 + "' unavailable";
      return;
    }

    score::Document* doc = score::test::new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto build = [&](const char* file) -> std::unique_ptr<score::gfx::ISFNode> {
      auto b = score::test::gfx::make_isf_node(corpus(file));
      if(!b.node && out.error.empty())
        out.error = std::string(file) + ": " + b.error;
      return std::move(b.node);
    };

    auto probeNode = build(probeShader);
    const bool viaMix = kind == Change::SecondInput
                        || kind == Change::SecondInputWithDepth
                        || kind == Change::ResizeSinkThroughMix;
    const bool resize
        = kind == Change::ResizeSink || kind == Change::ResizeSinkThroughMix;
    auto mixNode = viaMix ? build("isf-two-images.fs") : nullptr;
    const char* insertedShader = [&] {
      switch(kind)
      {
        case Change::SecondInputWithDepth:
        case Change::SameSinkWithDepth:
          return "isf-solid-writes-depth.fs";
        case Change::Between:
          return "isf-passthrough-plain.fs";
        default:
          return "isf-solid-color.fs";
      }
    }();
    auto insertedNode = resize ? nullptr : build(insertedShader);
    if(!probeNode || (viaMix && !mixNode) || (!resize && !insertedNode))
      return;

    auto sinkOwned = std::make_unique<score::gfx::BackgroundNode>();
    sinkOwned->shared_readback = std::make_shared<QRhiReadbackResult>();
    sinkOwned->setSize(QSize{64, 64});
    auto* sink = sinkOwned.get();

    using pi = Gfx::port_index;
    const auto cable = Process::CableType::ImmediateGlutton;
    const int32_t p = g.register_node(std::move(probeNode));
    const int32_t m = viaMix ? g.register_node(std::move(mixNode)) : -1;
    const int32_t s = g.register_node(std::move(sinkOwned));

    const auto publish = [&](const std::vector<Gfx::EdgeSpec>& es) {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      for(const auto& e : es)
        exec.setEdge(e.first, e.second, e.type);
      exec.endTick(st);
    };

    std::vector<Gfx::EdgeSpec> edges;
    if(viaMix)
      edges = {{pi{p, 0}, pi{m, 0}, cable}, {pi{m, 0}, pi{s, 0}, cable}};
    else
      edges = {{pi{p, 0}, pi{s, 0}, cable}};
    publish(edges);

    // The probe's red (or grey) channel in the left quarter: the mix shows the
    // probe on its left half, and the depth-writing shader leaves the left half
    // of a shared sink alone.
    const auto probeLevel = [&]() -> int {
      const auto& rb = *sink->shared_readback;
      score::test::gfx::ReadbackImage img;
      img.width = rb.pixelSize.width();
      img.height = rb.pixelSize.height();
      img.bytes = rb.data;
      if(!img.valid())
        return -1;
      return img.at(img.width / 4, img.height / 2)[0];
    };

    g.renderFrames(3);
    out.before = probeLevel();

    // The insertion, as the execution delivers it: the node, then the edge set
    // the next tick publishes. The resize, as a window or a background output
    // following the score view gets it.
    const int32_t x = resize ? -1 : g.register_node(std::move(insertedNode));
    switch(kind)
    {
      case Change::Unconnected:
        break;
      case Change::SecondInput:
      case Change::SecondInputWithDepth:
        edges.push_back({pi{x, 0}, pi{m, 1}, cable});
        break;
      case Change::SameSinkWithDepth:
        edges.push_back({pi{x, 0}, pi{s, 0}, cable});
        break;
      case Change::Between:
        edges = {{pi{p, 0}, pi{x, 0}, cable}, {pi{x, 0}, pi{s, 0}, cable}};
        break;
      case Change::ResizeSink:
      case Change::ResizeSinkThroughMix:
        sink->setSize(QSize{96, 80});
        break;
    }
    publish(edges);

    g.renderFrames(1);
    out.after = probeLevel();

    publish({});
    if(!resize)
      g.unregister_node(x);
    g.unregister_node(s);
    if(viaMix)
      g.unregister_node(m);
    g.unregister_node(p);
    g.renderFrames(1);
  });
  return out;
}

void checkKept(score::gfx::GraphicsApi backend, const char* probe, Change kind)
{
  CAPTURE(score::test::gfx::backend_name(backend), probe, name(kind));

  const Outcome o = run(backend, probe, kind);
  if(!o.skip.empty())
    SKIP(o.skip);

  // Asked after the run: the GL capability probe needs the application the run
  // brought up once.
  if(std::string_view{probe} == "isf-persistent-counter.fs")
    if(const char* why = score::test::gfx::storage_buffer_skip_reason(backend))
      SKIP(why);
  REQUIRE(o.error.empty());

  // Three frames drawn: the probe is running.
  INFO("level after 3 frames " << o.before << ", after the change and one more "
                               << o.after);
  REQUIRE(o.before >= 40);
  REQUIRE(o.before <= 56);

  // One more frame of the same accumulation, not the first frame of a new one.
  CHECK(o.after >= o.before + 10);
  CHECK(o.after <= o.before + 22);
}
}

TEST_CASE(
    "inserting a node keeps the state of the nodes already rendering",
    "[gfx][incremental][insert]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  const char* probe = GENERATE(
      "isf-persistent-last-pass-accumulate.fs", "isf-persistent-counter.fs");
  const Change kind = GENERATE(
      Change::Unconnected, Change::SecondInput, Change::SecondInputWithDepth,
      Change::SameSinkWithDepth, Change::Between);
  checkKept(backend, probe, kind);
}

TEST_CASE(
    "resizing the output keeps the state that does not follow its size",
    "[gfx][incremental][resize]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  const Change kind = GENERATE(Change::ResizeSink, Change::ResizeSinkThroughMix);
  checkKept(backend, "isf-persistent-counter.fs", kind);
}

namespace
{
//! The counter probe seen through an inspector preview, before and after the
//! preview widget is resized.
Outcome runPreviewResize(score::gfx::GraphicsApi backend)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto& settings = app.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setRate(60.);
    settings.setGraphicsApi(apiSettingName(backend));

    std::string probed;
    if(!score::test::gfx::probe_api(backend, probed))
    {
      out.skip = std::string("RHI backend '") + score::test::gfx::backend_name(backend)
                 + "' unavailable";
      return;
    }

    score::Document* doc = score::test::new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;

    auto probe = score::test::gfx::make_isf_node(corpus("isf-persistent-counter.fs"));
    if(!probe.node)
    {
      out.error = probe.error;
      return;
    }
    const int32_t p = g.register_node(std::move(probe.node));
    g.renderFrames(1);

    // Selected while the inspector is not laid out: the widget still has its
    // default size.
    auto* preview = new Gfx::RhiPreviewWidget;
    preview->useContext(&g, Gfx::port_index{p, 0});

    const auto level = [&]() -> int {
      auto* node = preview->liveNode();
      if(!node || !node->shared_readback)
        return -1;
      const auto& rb = *node->shared_readback;
      score::test::gfx::ReadbackImage img;
      img.width = rb.pixelSize.width();
      img.height = rb.pixelSize.height();
      img.bytes = rb.data;
      if(!img.valid())
        return -1;
      return img.center()[0];
    };

    g.renderFrames(3);
    out.before = level();

    // The inspector lays it out.
    const QSize from = preview->size();
    const QSize to{232, 200};
    preview->resize(to);
    QResizeEvent ev{to, from};
    QCoreApplication::sendEvent(preview, &ev);

    g.renderFrames(1);
    out.after = level();

    delete preview;
    g.unregister_node(p);
    g.renderFrames(1);
  });
  return out;
}
}

TEST_CASE(
    "resizing a texture-port preview keeps the state of the chain it shows",
    "[gfx][preview][resize]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const Outcome o = runPreviewResize(backend);
  if(!o.skip.empty())
    SKIP(o.skip);
  if(const char* why = score::test::gfx::storage_buffer_skip_reason(backend))
    SKIP(why);
  REQUIRE(o.error.empty());

  INFO("level after 3 frames " << o.before << ", after the resize and one more "
                               << o.after);
  REQUIRE(o.before >= 40);
  REQUIRE(o.before <= 56);
  CHECK(o.after >= o.before + 10);
  CHECK(o.after <= o.before + 22);
}
