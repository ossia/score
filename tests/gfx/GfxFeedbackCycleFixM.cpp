// Image feedback loops through the render graph (fixM: N63, N64).
//
// fixm-step-add.fs outputs its input plus 1/32 grey. Wired into a loop, the
// level at the sink climbs with the frame count; a loop that does not carry
// leaves it flat, and a dropped chain leaves the sink black.
//
// * two nodes, the loop closed by a Delayed cable: the reference, works.
// * two nodes, every cable Immediate (N63): the topological sort used to
//   throw not_a_dag and leave the render list with no nodes at all -- the
//   whole chain vanished with only a qDebug. One edge of the cycle is now
//   treated as delayed, and a warning names the nodes of the cycle.
// * one node, Immediate cable straight back into itself: renders and warns.
// * one node, Delayed or Immediate cable straight back into itself carries
//   its state (N64).
//
//   DISPLAY=:0 SCORE_GPU_VALIDATION=0 ctest -R gfx_feedback_cycle_fixm
#include "GfxIncrementalCommon.hpp"

#include <QtGlobal>

#include <mutex>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
enum class Topology
{
  DelayedPair,
  ImmediatePair,
  DelayedSelf,
  ImmediateSelf
};

const char* topology_name(Topology t)
{
  switch(t)
  {
    case Topology::DelayedPair:
      return "A->B immediate, B->A delayed";
    case Topology::ImmediatePair:
      return "A->B immediate, B->A immediate";
    case Topology::DelayedSelf:
      return "A->A delayed";
    case Topology::ImmediateSelf:
      return "A->A immediate";
  }
  return "?";
}

struct CycleShot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  int early = -1, late = -1;
};

std::mutex g_msgMutex;
std::vector<std::string> g_messages;
QtMessageHandler g_previous{};

void capture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  {
    std::lock_guard l{g_msgMutex};
    g_messages.push_back(msg.toStdString());
  }
  if(g_previous)
    g_previous(type, ctx, msg);
}

int center_level(const ReadbackImage& img)
{
  if(!img.valid())
    return -1;
  return int(img.center()[0]);
}

CycleShot render_cycle(score::gfx::GraphicsApi be, Topology t)
{
  CycleShot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    g_previous = qInstallMessageHandler(capture);
    struct Restore
    {
      ~Restore()
      {
        qInstallMessageHandler(g_previous);
        g_previous = {};
      }
    } restore;
    GfxPipeline p;
    const int a = p.addIsf(corpus("fixm-step-add.fs"));
    const bool pair = t == Topology::DelayedPair || t == Topology::ImmediatePair;
    const int b = pair ? p.addIsf(corpus("fixm-step-add.fs")) : a;
    const int sink = p.addSink({32, 32});
    if(a < 0 || b < 0)
    {
      r.error = p.error();
      return;
    }
    const auto back = (t == Topology::DelayedPair || t == Topology::DelayedSelf)
                          ? Process::CableType::DelayedGlutton
                          : Process::CableType::ImmediateGlutton;
    if(pair)
      p.wire(p.imageOut(a, 0), p.imageIn(b, 0));
    p.wire(p.imageOut(b, 0), p.imageIn(a, 0), back);
    p.wire(p.imageOut(b, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    p.render(2);
    r.early = center_level(p.readback(sink));
    p.render(4);
    r.late = center_level(p.readback(sink));
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}

bool warned_about_cycle()
{
  std::lock_guard l{g_msgMutex};
  for(const auto& m : g_messages)
    if(m.find("immediate cables form a cycle") != std::string::npos)
      return true;
  return false;
}

CycleShot run(score::gfx::GraphicsApi backend, Topology topo)
{
  {
    std::lock_guard l{g_msgMutex};
    g_messages.clear();
  }
  return render_cycle(backend, topo);
}
}

TEST_CASE("a two-node image feedback loop carries its state", "[gfx][feedback][fixm]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const auto topo = GENERATE(Topology::DelayedPair, Topology::ImmediatePair);
  CAPTURE(backend_name(backend), topology_name(topo));

  const CycleShot s = run(backend, topo);
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.early, s.late);

  // One step is 1/32 of full scale, 8 in 8-bit, and the loop takes two steps
  // per frame. After two frames the sink has seen at least two steps; four
  // more frames add at least eight more.
  CHECK(s.early >= 16);
  CHECK(s.late >= s.early + 48);
  CHECK(warned_about_cycle() == (topo == Topology::ImmediatePair));
}

TEST_CASE(
    "an immediate self-cable renders and is reported", "[gfx][feedback][fixm]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const CycleShot s = run(backend, Topology::ImmediateSelf);
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.early, s.late);

  CHECK(s.early >= 8);
  CHECK(s.late >= 8);
  CHECK(warned_about_cycle());
}

// N64: the pass into a self-fed port used to draw into the very texture the
// node samples (RenderList::renderTargetForOutput fell through to the port's
// own input target), and the level froze. The render list now draws that
// port into a second target and copies it back at the start of the next
// frame.
TEST_CASE(
    "a delayed self-cable carries its state", "[gfx][feedback][fixm][n64]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const auto topo = GENERATE(Topology::DelayedSelf, Topology::ImmediateSelf);
  CAPTURE(backend_name(backend), topology_name(topo));

  const CycleShot s = run(backend, topo);
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.early, s.late);

  CHECK(s.early >= 16);
  CHECK(s.late >= s.early + 24);
}

// The same self-cable added and removed while rendering, through the
// incremental path the app uses (reconcile + createAllMissingPasses).
TEST_CASE("a self-cable added live carries its state", "[gfx][feedback][fixm][n64]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  CycleShot s;
  int before = -1, after = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("fixm-step-add.fs"));
    const int sink = p.addSink({32, 32});
    if(a < 0)
    {
      s.error = p.error();
      return;
    }
    p.wire(p.imageOut(a, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      s.skipped = p.skipped();
      s.skip_reason = p.skipReason();
      s.error = p.error();
      return;
    }
    s.backend = p.backend();

    p.render(2);
    before = center_level(p.readback(sink));
    p.addEdgeIncremental(
        p.imageOut(a, 0), p.imageIn(a, 0), Process::CableType::DelayedGlutton);
    p.render(2);
    s.early = center_level(p.readback(sink));
    p.render(4);
    s.late = center_level(p.readback(sink));
    p.removeEdgeIncremental(p.imageOut(a, 0), p.imageIn(a, 0));
    p.render(2);
    after = center_level(p.readback(sink));
    if(s.error.empty())
      s.error = p.error();
  });

  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, before, s.early, s.late, after);

  CHECK(before >= 6);
  CHECK(before <= 10);
  CHECK(s.early >= before + 8);
  CHECK(s.late >= s.early + 24);
  CHECK(after >= 6);
  CHECK(after <= 10);
}
