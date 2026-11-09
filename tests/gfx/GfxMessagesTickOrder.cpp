// A node gets the messages of the execution in the order of the ticks that
// sent them.
//
// With score's Parallel execution setting, a tick runs each gfx exec node on
// whichever thread of the pool is free, and the node sends its message from
// there. The GfxContext's message queue keeps the order of each sending
// thread, not across threads: a node's message of one tick could be applied
// after the one of the next tick and undo it, so that a shader drawn from a
// stepped execution showed the value one tick late.
//
// This drives the real document GfxContext through GfxExecutionAction, as a
// parallel execution does: each tick sends from one of two threads that both
// stay alive, then updateGraph() is called once, as a frame does.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/Node.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace
{
struct RecordingNode final : score::gfx::ProcessNode
{
  std::vector<int>* received{};

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return nullptr;
  }

  using score::gfx::ProcessNode::process;
  void process(int32_t port, const ossia::value& v) override
  {
    if(port == 0)
      received->push_back(ossia::convert<int>(v));
  }
};
}

TEST_CASE(
    "messages sent from several threads reach the node in tick order",
    "[gfx][exec][parallel][gui]")
{
  constexpr int ticks = 16;
  std::vector<int> received;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto node = std::make_unique<RecordingNode>();
    node->received = &received;
    const int32_t id = g.register_node(std::move(node));
    g.updateGraph();

    // The tick being run, or -1 between ticks, and how many were sent.
    std::atomic_int current{-1};
    std::atomic_int sent{0};
    const auto worker = [&](int parity) {
      for(int t = parity; t < ticks; t += 2)
      {
        while(current.load(std::memory_order_acquire) != t)
          std::this_thread::yield();
        auto msg = exec.allocateMessage(1);
        msg.node_id = id;
        msg.input.resize(1);
        msg.input[0] = ossia::value{t};
        g.send_message(std::move(msg));
        sent.fetch_add(1, std::memory_order_release);
      }
    };

    std::thread even{worker, 0};
    std::thread odd{worker, 1};
    ossia::audio_tick_state st{};
    for(int t = 0; t < ticks; t++)
    {
      exec.startTick(st);
      current.store(t, std::memory_order_release);
      while(sent.load(std::memory_order_acquire) != t + 1)
        std::this_thread::yield();
      exec.endTick(st);
    }
    even.join();
    odd.join();

    g.updateGraph();

    g.unregister_node(id);
    g.updateGraph();
  });

  std::vector<int> expected;
  for(int t = 0; t < ticks; t++)
    expected.push_back(t);
  CHECK(received == expected);
}
