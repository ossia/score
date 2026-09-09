// Opening a texture port's preview must not restart what is already rendering.
//
// Selecting a process with a texture outlet builds a GraphPreviewWidget in the
// inspector, which is a Gfx::RhiPreviewWidget on its "context backend": it puts
// a score::gfx::BackgroundNode into the DOCUMENT's GfxContext and wires the
// outlet to it. That node is an OutputNode, and GfxContext::updateGraph asked
// for a full rebuild the moment an output appeared -- Graph::createAllRenderLists
// stops every output, releases every renderer and builds the lot again.
//
// So the window that was already rendering went black for the length of the
// rebuild, every time a preview opened, and again when it closed. That is the
// blink reported on the rect-mapper patch, whose inspector carries such a
// preview; nothing about the patch is special, any texture outlet does it.
//
// Observable: OutputNode::stopRendering() on the output that was ALREADY live.
// A full rebuild calls it; bringing the new output up on its own does not. The
// render-list pointer is checked too, but only as a corroborating signal -- a
// freed RenderList can be reallocated at the same address, so the counter is
// what the case turns on.
//
// This drives the real widget rather than the registration call underneath it,
// so it stays honest whichever side of that boundary the fix lands on.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Widgets/RhiPreviewWidget.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
//! A document output that counts how many times it was stopped.
struct CountingSink final : score::gfx::BackgroundNode
{
  int stops{};
  void stopRendering() override
  {
    stops++;
    score::gfx::BackgroundNode::stopRendering();
  }
};
}

TEST_CASE(
    "opening a texture-port preview does not restart the live outputs",
    "[gfx][preview][incremental][gui]")
{
  bool live = false;
  int baseline{}, afterAttach{}, afterDetach{};
  const void* rlBaseline{};
  const void* rlAfterAttach{};
  const void* rlAfterDetach{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto producer = std::make_unique<score::gfx::ImagesNode>(doc->context());
    auto owned = std::make_unique<CountingSink>();
    owned->shared_readback = std::make_shared<QRhiReadbackResult>();
    auto* sink = owned.get();

    const int32_t p = g.register_node(std::move(producer));
    const int32_t s = g.register_node(std::move(owned));

    const auto publish = [&](std::vector<Gfx::EdgeSpec> es) {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      for(const auto& e : es)
        exec.setEdge(e.first, e.second, e.type);
      exec.endTick(st);
    };

    using pi = Gfx::port_index;
    const auto glutton = Process::CableType::ImmediateGlutton;
    const Gfx::EdgeSpec ePS{pi{p, 0}, pi{s, 0}, glutton};

    // The document's own output, rendering. This frame is a full rebuild --
    // the output node landed in it -- which is exactly what must not happen
    // again below.
    publish({ePS});
    g.updateGraph();

    live = sink->canRender() && sink->renderer() != nullptr;
    baseline = sink->stops;
    rlBaseline = sink->renderer();

    // The inspector opens on a texture outlet.
    auto* preview = new Gfx::RhiPreviewWidget;
    preview->resize(64, 64);
    preview->useContext(&g, pi{p, 0});
    g.updateGraph();

    afterAttach = sink->stops;
    rlAfterAttach = sink->renderer();

    // ... and closes again.
    delete preview;
    g.updateGraph();

    afterDetach = sink->stops;
    rlAfterDetach = sink->renderer();

    g.unregister_node(s);
    g.unregister_node(p);
    g.updateGraph();
  });

  if(!live)
    SKIP("no working RHI here: the document output never came up");

  INFO(
      "stops: baseline " << baseline << ", after attach " << afterAttach
                         << ", after detach " << afterDetach);

  CHECK(afterAttach == baseline);
  CHECK(afterDetach == baseline);

  CHECK(rlAfterAttach == rlBaseline);
  CHECK(rlAfterDetach == rlBaseline);
}
