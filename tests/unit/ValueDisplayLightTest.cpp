// Value display: the arrival light is lit while values arrive and fades once
// they stop. Its timer does not tick while values stream (each arrival repaints
// anyway), and stops once the light is out: an idle display never repaints.

#include <score/application/GUIApplicationContext.hpp>

#include <Process/Dataflow/PortFactory.hpp>

#include <Ui/ValueDisplay.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>

#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

namespace
{
void runEventsFor(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(1);
  }
}
}

TEST_CASE("value display: the arrival light does not repaint when idle", "[ui][value_display]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, QStringLiteral("3f4a41f2-fa39-420f-ab0f-0af6b8409edb"), {});
    REQUIRE(proc);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QGraphicsRectItem root{QRectF{0., 0., 200., 100.}};
    using Layer = Ui::ValueDisplay::Node::Layer;
    Layer layer{*proc, pctx, &root};
    int ticks = 0;
    QObject::connect(&layer.m_flashTimer, &QTimer::timeout, [&] { ticks++; });

    int seq = 0;
    auto arrive = [&] {
      layer.on_values(ossia::value{std::vector<ossia::value>{seq, float(seq)}});
      seq++;
    };

    CHECK(layer.flashLevel() == 0.);
    CHECK(!layer.m_flashTimer.isActive());

    // Streaming faster than the fade steps: lit, no fade tick
    for(int i = 0; i < 15; i++)
    {
      arrive();
      CHECK(layer.flashLevel() > 0.8);
      runEventsFor(Layer::flash_step_ms / 3);
    }
    CHECK(ticks == 0);

    // Stopped: a few fade steps, then nothing
    runEventsFor(Layer::flash_ms + 3 * Layer::flash_step_ms);
    CHECK(layer.flashLevel() == 0.);
    CHECK(!layer.m_flashTimer.isActive());
    const int fadeTicks = ticks;
    CHECK(fadeTicks >= 1);
    CHECK(fadeTicks <= Layer::flash_ms / Layer::flash_step_ms + 2);

    runEventsFor(4 * Layer::flash_step_ms);
    CHECK(ticks == fadeTicks);
  });
}
