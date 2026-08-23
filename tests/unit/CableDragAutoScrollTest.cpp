// Dragging a cable towards the edge of the view scrolls it, so that ports in
// remote parts of the score can be reached without dropping the cable.

#include <Process/Dataflow/CableDragAutoScroller.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

#include <vector>

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

using Dataflow::CableDragAutoScroller;
constexpr int margin = CableDragAutoScroller::margin;
constexpr int maxStep = CableDragAutoScroller::maxStep;
}

TEST_CASE("Auto-scroll step: nothing away from the edges", "[dataflow][autoscroll]")
{
  const QSize sz{400, 300};
  CHECK(CableDragAutoScroller::scrollStep({200, 150}, sz).isNull());
  CHECK(CableDragAutoScroller::scrollStep({margin, margin}, sz).isNull());
  CHECK(CableDragAutoScroller::scrollStep({400 - margin - 1, 300 - margin - 1}, sz)
            .isNull());
}

TEST_CASE("Auto-scroll step: direction follows the edge", "[dataflow][autoscroll]")
{
  const QSize sz{400, 300};
  CHECK(CableDragAutoScroller::scrollStep({0, 150}, sz).x() < 0);
  CHECK(CableDragAutoScroller::scrollStep({0, 150}, sz).y() == 0);
  CHECK(CableDragAutoScroller::scrollStep({399, 150}, sz).x() > 0);
  CHECK(CableDragAutoScroller::scrollStep({200, 0}, sz).y() < 0);
  CHECK(CableDragAutoScroller::scrollStep({200, 299}, sz).y() > 0);

  // Corner: both axes
  const auto corner = CableDragAutoScroller::scrollStep({399, 299}, sz);
  CHECK(corner.x() > 0);
  CHECK(corner.y() > 0);
}

TEST_CASE("Auto-scroll step: faster the closer to the edge", "[dataflow][autoscroll]")
{
  const QSize sz{400, 300};
  int prev = 0;
  for(int x = margin - 1; x >= 0; x--)
  {
    const int step = -CableDragAutoScroller::scrollStep({x, 150}, sz).x();
    CHECK(step >= 1);
    CHECK(step >= prev);
    CHECK(step <= maxStep);
    prev = step;
  }
  CHECK(-CableDragAutoScroller::scrollStep({0, 150}, sz).x() == maxStep);

  // Beyond the viewport (the cursor can be reported slightly outside during a
  // drag): clamped, not runaway.
  CHECK(-CableDragAutoScroller::scrollStep({-50, 150}, sz).x() == maxStep);
  CHECK(CableDragAutoScroller::scrollStep({450, 150}, sz).x() == maxStep);
}

TEST_CASE("Auto-scroll step: tiny viewports never scroll", "[dataflow][autoscroll]")
{
  CHECK(CableDragAutoScroller::scrollStep({1, 1}, QSize{2 * margin, 2 * margin})
            .isNull());
  CHECK(CableDragAutoScroller::scrollStep({1, 1}, QSize{10, 10}).isNull());
}

TEST_CASE("Dragging against the edge scrolls the view", "[dataflow][autoscroll][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene{QRectF{0, 0, 4000, 3000}};
    QGraphicsView view{&scene};
    view.resize(400, 300);
    view.show();
    view.horizontalScrollBar()->setValue(0);
    view.verticalScrollBar()->setValue(0);
    spin(20);

    std::vector<QPointF> seen;
    CableDragAutoScroller scroller{[&](QPointF p) { seen.push_back(p); }};

    // A drag-move in the middle of the view: nothing happens.
    scroller.track(view.viewport(), view.mapToScene(QPoint{200, 150}));
    CHECK(!scroller.active());
    spin(60);
    CHECK(view.horizontalScrollBar()->value() == 0);
    CHECK(seen.empty());

    // Park the cursor against the right edge: the view scrolls right on its
    // own, and the scene position under the cursor is reported each step.
    const QPoint edge{view.viewport()->width() - 1, 150};
    scroller.track(view.viewport(), view.mapToScene(edge));
    CHECK(scroller.active());
    spin(120);
    const int h = view.horizontalScrollBar()->value();
    CHECK(h > 0);
    CHECK(view.verticalScrollBar()->value() == 0);
    REQUIRE(!seen.empty());
    // Follows the cursor: the last reported scene point is what is now under it
    CHECK(seen.back() == view.mapToScene(edge));
    CHECK(seen.back().x() >= seen.front().x());

    // Back to the middle: it stops where it is.
    scroller.track(view.viewport(), view.mapToScene(QPoint{200, 150}));
    CHECK(!scroller.active());
    spin(60);
    CHECK(view.horizontalScrollBar()->value() == h);

    // Bottom edge: vertical.
    scroller.track(view.viewport(), view.mapToScene(QPoint{200, view.viewport()->height() - 1}));
    spin(120);
    CHECK(view.verticalScrollBar()->value() > 0);
    CHECK(view.horizontalScrollBar()->value() == h);

    // Stopping (drop / drag leave) halts it.
    scroller.stop();
    CHECK(!scroller.active());
    const int v = view.verticalScrollBar()->value();
    spin(60);
    CHECK(view.verticalScrollBar()->value() == v);

    // Against an edge that cannot scroll any further: idles instead of spinning.
    view.horizontalScrollBar()->setValue(0);
    scroller.track(view.viewport(), view.mapToScene(QPoint{0, 150}));
    spin(60);
    CHECK(view.horizontalScrollBar()->value() == 0);
    CHECK(!scroller.active());

    // A widget that is not a view's viewport is ignored.
    QWidget plain;
    scroller.track(&plain, QPointF{0, 0});
    CHECK(!scroller.active());
    CHECK(scroller.view() == nullptr);
  });
}
