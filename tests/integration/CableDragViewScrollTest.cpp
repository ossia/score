// Integration test: dragging a cable against the edge of the central view
// must reveal more of the score, in both display modes.
//
// The nodal canvas has a 10x10 scene rect and pans its node container; the
// timeline's scene only grows when one scrolls past its end. Neither moves when
// the view's scroll bars are simply pushed, which is what made the auto-scroll
// a no-op in the real application: ProcessGraphicsView implements
// Dataflow::AutoScrollableView with a handler the central display installs.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/CableDragAutoScroller.hpp>

#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QScrollBar>

#include <catch2/catch_test_macros.hpp>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

Scenario::NodalIntervalView* nodalView(QGraphicsScene& scene)
{
  for(auto item : scene.items())
    if(auto n = dynamic_cast<Scenario::NodalIntervalView*>(item))
      return n;
  return nullptr;
}

template <typename F>
void withPresenter(F&& fn)
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);
    spin(50);
    fn(*presenter);
  });
}
}

TEST_CASE("The timeline grows when a cable is dragged past its end", "[integration][dataflow][autoscroll][gui]")
{
  withPresenter([](Scenario::ScenarioDocumentPresenter& p) {
    auto& gv = p.view().view();
    p.setNodalMode(false);
    spin(50);

    // Whatever the zoom, pushing to the right always reveals more
    const QRectF before = gv.sceneRect();
    const int hbefore = gv.horizontalScrollBar()->value();
    for(int i = 0; i < 10; i++)
      CHECK(Dataflow::CableDragAutoScroller::scrollViewBy(gv, {24, 0}));
    spin(20);
    CHECK(gv.horizontalScrollBar()->value() > hbefore);
    CHECK(gv.sceneRect().right() >= before.right());
    // The same thing a drag-move against the edge would trigger
    CHECK(gv.autoScrollBy({24, 0}));
    CHECK(gv.horizontalScrollBar()->value() > hbefore + 10 * 24 - 1);

    // And back to the start, which is a hard limit
    for(int i = 0; i < 100; i++)
      gv.autoScrollBy({-24, 0});
    CHECK(gv.horizontalScrollBar()->value() == gv.horizontalScrollBar()->minimum());
    CHECK(!gv.autoScrollBy({-24, 0}));
  });
}

TEST_CASE("The nodal canvas pans when a cable is dragged against an edge", "[integration][dataflow][autoscroll][gui]")
{
  withPresenter([](Scenario::ScenarioDocumentPresenter& p) {
    auto& gv = p.view().view();
    p.setNodalMode(true);
    spin(50);

    auto nodal = nodalView(p.view().scene());
    REQUIRE(nodal != nullptr);
    auto& itv = p.displayedInterval();

    const QPointF pos0 = nodal->nodeContainer().pos();
    REQUIRE(itv.nodalCenter().has_value());
    const QPointF center0 = *itv.nodalCenter();
    const double scale = nodal->nodeContainer().scale();

    // Revealing what is to the right / below moves the canvas left / up
    CHECK(Dataflow::CableDragAutoScroller::scrollViewBy(gv, {24, 10}));
    CHECK(nodal->nodeContainer().pos() == pos0 - QPointF{24, 10});
    // ... and the center saved with the interval follows, as when dragging
    CHECK(*itv.nodalCenter() == center0 + QPointF{24, 10} / scale);

    // Infinite: it never refuses
    for(int i = 0; i < 1000; i++)
      CHECK(gv.autoScrollBy({-24, 0}));
    CHECK(nodal->nodeContainer().pos().x() == pos0.x() - 24 + 1000 * 24);

    // Leaving the nodal display removes its handler: back to the timeline's
    p.setNodalMode(false);
    spin(50);
    CHECK(gv.autoScrollBy({24, 0}));
  });
}
