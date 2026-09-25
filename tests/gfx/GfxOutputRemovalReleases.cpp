// A removed output releases what it holds outside the graph (a TCP port, an NDI
// sender name, a pipeline) in the same graph update that removes it, not when
// the nursery later deletes the node: an output created in the same update,
// with the same settings (a document loaded over an open one), would otherwise
// find its port taken and never recover.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Settings/Model.hpp>

#include <ossia/audio/audio_tick.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}

struct Released
{
  int count = 0;
};

// destroyOutput() from ~BackgroundNode does not reach this override: only a
// release made by the removal path itself is counted.
struct ProbeOutput final : score::gfx::BackgroundNode
{
  std::shared_ptr<Released> released;
  explicit ProbeOutput(std::shared_ptr<Released> r)
      : released{std::move(r)}
  {
  }
  void destroyOutput() override
  {
    released->count++;
    BackgroundNode::destroyOutput();
  }
};
}

TEST_CASE(
    "A removed output is released in the update that removes it",
    "[gfx][output][lifecycle]")
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    settings.setGraphicsApi(QStringLiteral("OpenGL"));
    std::string backendName;
    if(!score::test::gfx::probe_api(score::gfx::GraphicsApi::OpenGL, backendName))
      SKIP("OpenGL unavailable on this machine");

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto built = score::test::gfx::make_isf_node(corpus("isf-solid-color.fs"));
    REQUIRE(built.node);
    auto released = std::make_shared<Released>();
    auto probe = std::make_unique<ProbeOutput>(released);
    probe->shared_readback = std::make_shared<QRhiReadbackResult>();

    const int32_t src = g.register_node(std::move(built.node));
    const int32_t out = g.register_node(std::move(probe));
    {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      exec.setEdge(
          Gfx::port_index{src, 0}, Gfx::port_index{out, 0},
          Process::CableType::ImmediateGlutton);
      exec.endTick(st);
    }
    g.updateGraph();
    REQUIRE(released->count == 0);

    // What removing an output device does: REMOVE_NODE, handled at the next
    // graph update. No event loop runs in between, so the nursery's deferred
    // deletion cannot be what releases it.
    g.unregister_node(out);
    g.updateGraph();
    CHECK(released->count == 1);
  });
}
