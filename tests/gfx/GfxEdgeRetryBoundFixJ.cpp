// An edge whose endpoint node never registers is retried a bounded number of
// ticks, then dropped with one warning naming it.
//
// GfxContext::incrementalEdgeUpdate defers an added edge whose node is not
// present yet: it takes the edge back out of the baseline and re-raises
// edges_changed so the next tick re-emits it. With no bound, an edge to a node
// that is gone for good (removed at stop while the producer still publishes
// it) re-entered the edge diff on every tick forever, visible only under
// SCORE_GFX_TRACE.
//
// Drives the real document GfxContext through the producer channel
// (GfxExecutionAction::setEdge/endTick) and calls updateGraph synchronously,
// as GfxEdgeConsumeLatch.cpp does. Observed through the GFX-EDGES consume
// trace line and the Qt warning.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>

#include <core/document/Document.hpp>

#include <QString>
#include <QStringList>

#include <catch2/catch_test_macros.hpp>

#if defined(__unix__)
#include <unistd.h>
#endif

#include <string>

namespace
{
#if defined(__unix__)
struct StderrCapture
{
  int saved{-1};
  int fds[2]{-1, -1};

  StderrCapture()
  {
    ::fflush(stderr);
    saved = ::dup(2);
    ::pipe(fds);
    ::dup2(fds[1], 2);
    ::close(fds[1]);
  }

  std::string finish()
  {
    ::fflush(stderr);
    ::dup2(saved, 2);
    ::close(saved);
    std::string r;
    char buf[4096];
    ssize_t n;
    while((n = ::read(fds[0], buf, sizeof buf)) > 0)
      r.append(buf, std::size_t(n));
    ::close(fds[0]);
    return r;
  }
};

int count(const std::string& s, const std::string& needle)
{
  int n = 0;
  for(auto p = s.find(needle); p != std::string::npos; p = s.find(needle, p + 1))
    n++;
  return n;
}

QStringList g_warnings;
void collectWarnings(QtMsgType t, const QMessageLogContext&, const QString& msg)
{
  if(t == QtWarningMsg)
    g_warnings.push_back(msg);
}
#endif
}

#if defined(__unix__)
TEST_CASE(
    "an edge to a node that never registers is retried a bounded number of "
    "ticks, then dropped with one warning",
    "[gfx][l3][incremental][edge-defer][gui]")
{
  qputenv("SCORE_GFX_TRACE", "1");

  constexpr int ticks = 40;
  int consumes = -1;
  int ghostWarnings = -1;
  QString warning;
  int32_t ghost = -1;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto prod = std::make_unique<score::gfx::ImagesNode>(doc->context());
    auto sink = std::make_unique<score::gfx::BackgroundNode>();
    sink->shared_readback = std::make_shared<QRhiReadbackResult>();

    const int32_t a = g.register_node(std::move(prod));
    const int32_t s = g.register_node(std::move(sink));
    ghost = s + 1000;

    using pi = Gfx::port_index;
    const Gfx::EdgeSpec real{pi{a, 0}, pi{s, 0}, Process::CableType::ImmediateGlutton};
    const Gfx::EdgeSpec dangling{
        pi{a, 0}, pi{ghost, 0}, Process::CableType::ImmediateGlutton};

    const auto publish = [&](std::vector<Gfx::EdgeSpec> es) {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      for(const auto& e : es)
        exec.setEdge(e.first, e.second, e.type);
      exec.endTick(st);
    };

    // The first frame lands the output node, which is a full rebuild; the
    // dangling edge is published after it so it takes the incremental path.
    publish({real});
    g.updateGraph();
    publish({real, dangling});

    g_warnings.clear();
    auto prev = qInstallMessageHandler(collectWarnings);
    std::string log;
    {
      StderrCapture cap;
      for(int i = 0; i < ticks; i++)
        g.updateGraph();
      log = cap.finish();
    }
    qInstallMessageHandler(prev);

    consumes = count(log, "GFX-EDGES consume");
    ghostWarnings = 0;
    for(const auto& w : g_warnings)
      if(w.contains(QStringLiteral("-> %1:0").arg(ghost)))
      {
        ghostWarnings++;
        warning = w;
      }

    g.unregister_node(s);
    g.unregister_node(a);
    g.updateGraph();
  });

  INFO("edge-diff passes over " << ticks << " ticks: " << consumes);
  INFO("warning: " << warning.toStdString());
  // The first pass plus a bounded number of retries, not one per tick.
  CHECK(consumes >= 2);
  CHECK(consumes <= 10);
  CHECK(ghostWarnings == 1);
}
#else
TEST_CASE("an edge to a node that never registers is retried a bounded number of ticks")
{
  SKIP("stderr capture via dup2 is a unix-only harness");
}
#endif
