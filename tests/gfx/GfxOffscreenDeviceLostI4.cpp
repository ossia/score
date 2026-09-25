// An offscreen output reports a lost GPU device (N73, option A).
//
// OffscreenFrame records the begin and end FrameOp results; an offscreen output
// whose frame reports FrameOpDeviceLost logs it once, stops rendering and
// answers OutputNode::deviceLost(), so a caller grabbing its frames can fail
// instead of reading a stale or black image. The loss is injected through
// OffscreenFrame::simulateDeviceLost.
//
// Registration:
//   score_add_gfx_test(offscreen_device_lost_i4 GfxOffscreenDeviceLostI4.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QtGui/rhi/qrhi.h>

#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

std::vector<std::string> g_messages;
QtMessageHandler g_previous{};

void captureMessages(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(type == QtCriticalMsg && msg.contains(QStringLiteral("device was lost")))
    g_messages.push_back(msg.toStdString());
  if(g_previous)
    g_previous(type, ctx, msg);
}

struct SimulateDeviceLost
{
  SimulateDeviceLost() { score::gfx::OffscreenFrame::simulateDeviceLost = true; }
  ~SimulateDeviceLost() { score::gfx::OffscreenFrame::simulateDeviceLost = false; }
};
}

TEST_CASE("OffscreenFrame records the FrameOp results", "[gfx][output][device-lost]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  bool okOpened{}, okLost{}, lostOpened{}, lostLost{};
  int okEnd{-1}, lostBegin{-1};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addIsf(corpus("isf-solid-color.fs"));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(src, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = true;
      err = p.skipReason();
      return;
    }
    err = p.error();
    auto& rhi = *p.sink(sink)->renderState()->rhi;
    {
      score::gfx::OffscreenFrame f{rhi};
      okOpened = bool(f);
      okEnd = f.end();
      okLost = f.deviceLost();
    }
    {
      SimulateDeviceLost lost;
      score::gfx::OffscreenFrame f{rhi};
      lostOpened = bool(f);
      lostBegin = f.beginResult();
      lostLost = f.deviceLost();
    }
  });
  if(skipped)
    SKIP(err);
  REQUIRE(err.empty());

  CHECK(okOpened);
  CHECK(okEnd == QRhi::FrameOpSuccess);
  CHECK_FALSE(okLost);
  CHECK_FALSE(lostOpened);
  CHECK(lostBegin == QRhi::FrameOpDeviceLost);
  CHECK(lostLost);
}

TEST_CASE(
    "BackgroundNode logs a lost device once and reports it", "[gfx][output][device-lost]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  bool lostBefore{true}, lostAfter{}, lostLater{};
  bool renderedBefore{};
  std::size_t messagesAfterLoss{};
  std::vector<std::string> messages;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addIsf(corpus("isf-solid-color.fs"));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(src, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = true;
      err = p.skipReason();
      return;
    }
    err = p.error();
    p.render(2);
    lostBefore = p.sink(sink)->deviceLost();
    renderedBefore = !p.readback(sink).bytes.isEmpty();

    g_messages.clear();
    g_previous = qInstallMessageHandler(captureMessages);
    {
      SimulateDeviceLost lost;
      p.render(3);
    }
    lostAfter = p.sink(sink)->deviceLost();
    messagesAfterLoss = g_messages.size();
    p.render(2);
    p.sink(sink)->handleDeviceLost("BackgroundNode");
    lostLater = p.sink(sink)->deviceLost();
    qInstallMessageHandler(g_previous);
    messages = g_messages;
  });
  if(skipped)
    SKIP(err);
  REQUIRE(err.empty());

  CHECK(renderedBefore);
  CHECK_FALSE(lostBefore);
  CHECK(lostAfter);
  CHECK(messagesAfterLoss == 1);
  CHECK(lostLater);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].find("score::gfx::BackgroundNode") != std::string::npos);
}
