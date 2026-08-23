// Dragging a cable towards the edge of the view scrolls it, so that ports in
// remote parts of the score can be reached without dropping the cable.

#include <Process/Dataflow/AutoScrollableView.hpp>
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
constexpr int outsideLimit = CableDragAutoScroller::outsideLimit;

//! The cursor position the scroller's timer sees, under the test's control
struct FakeCursor
{
  QPoint pos{200, 150};
  CableDragAutoScroller::CursorProvider provider()
  {
    return [this](QWidget&) { return pos; };
  }
};
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

TEST_CASE("Auto-scroll step: faster the closer to the edge, full speed beyond it", "[dataflow][autoscroll]")
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

  // Outside the viewport (the cursor left the view while dragging): full
  // speed, as long as it stays close...
  CHECK(-CableDragAutoScroller::scrollStep({-50, 150}, sz).x() == maxStep);
  CHECK(CableDragAutoScroller::scrollStep({450, 150}, sz).x() == maxStep);
  CHECK(CableDragAutoScroller::scrollStep({200, 300 + outsideLimit - 1}, sz).y() == maxStep);
  // ... and nothing once it is far away (another window, another panel)
  CHECK(CableDragAutoScroller::scrollStep({-outsideLimit - 1, 150}, sz).isNull());
  CHECK(CableDragAutoScroller::scrollStep({400 + outsideLimit, 150}, sz).isNull());
  CHECK(CableDragAutoScroller::scrollStep({200, -5000}, sz).isNull());
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

    FakeCursor cursor;
    std::vector<QPointF> seen;
    CableDragAutoScroller scroller{[&](QPointF p) { seen.push_back(p); }, cursor.provider()};

    // A drag-move in the middle of the view: nothing happens.
    cursor.pos = {200, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    CHECK(!scroller.active());
    spin(60);
    CHECK(view.horizontalScrollBar()->value() == 0);
    CHECK(seen.empty());

    // Park the cursor against the right edge: the view scrolls right on its
    // own, and the scene position under the cursor is reported each step.
    cursor.pos = {view.viewport()->width() - 1, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    CHECK(scroller.active());
    spin(120);
    const int h = view.horizontalScrollBar()->value();
    CHECK(h > 0);
    CHECK(view.verticalScrollBar()->value() == 0);
    REQUIRE(!seen.empty());
    // Follows the cursor: the last reported scene point is what is now under it
    CHECK(seen.back() == view.mapToScene(cursor.pos));
    CHECK(seen.back().x() >= seen.front().x());

    // Back to the middle: it stops where it is.
    cursor.pos = {200, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    CHECK(!scroller.active());
    spin(60);
    CHECK(view.horizontalScrollBar()->value() == h);

    // Bottom edge: vertical.
    cursor.pos = {200, view.viewport()->height() - 1};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    spin(120);
    CHECK(view.verticalScrollBar()->value() > 0);
    CHECK(view.horizontalScrollBar()->value() == h);

    // Stopping (drop) halts it.
    scroller.stop();
    CHECK(!scroller.active());
    const int v = view.verticalScrollBar()->value();
    spin(60);
    CHECK(view.verticalScrollBar()->value() == v);

    // Against an edge that cannot scroll any further: idles instead of spinning.
    view.horizontalScrollBar()->setValue(0);
    cursor.pos = {0, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
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

TEST_CASE("Leaving the view keeps scrolling while the cursor stays close", "[dataflow][autoscroll][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene{QRectF{0, 0, 4000, 3000}};
    QGraphicsView view{&scene};
    view.resize(400, 300);
    view.show();
    view.horizontalScrollBar()->setValue(0);
    spin(20);

    FakeCursor cursor;
    CableDragAutoScroller scroller{[](QPointF) {}, cursor.provider()};

    // The last drag-move was near the edge, then the cursor left the viewport
    // (no more drag-move events): the scroller is told the drag left.
    cursor.pos = {view.viewport()->width() - 2, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    cursor.pos = {view.viewport()->width() + 60, 150};
    scroller.continueFromCursor();
    CHECK(scroller.active());
    spin(120);
    const int h = view.horizontalScrollBar()->value();
    CHECK(h > 0);
    CHECK(scroller.active());

    // Wandering far off (another window): the view is left alone
    cursor.pos = {view.viewport()->width() + outsideLimit + 100, 150};
    spin(60);
    CHECK(!scroller.active());
    CHECK(view.horizontalScrollBar()->value() == h);

    // Coming back close to the edge: told again, it resumes
    cursor.pos = {-30, 150};
    scroller.continueFromCursor();
    spin(120);
    CHECK(view.horizontalScrollBar()->value() < h);

    // Even when the last drag-move was in the middle: it is the cursor that counts
    cursor.pos = {200, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    CHECK(!scroller.active());
    cursor.pos = {200, view.viewport()->height() + 20};
    scroller.continueFromCursor();
    spin(120);
    CHECK(view.verticalScrollBar()->value() > 0);
  });
}

namespace
{
// A view that scrolls its own way (panning an item, growing its scene...)
struct PanningView final
    : QGraphicsView
    , Dataflow::AutoScrollableView
{
  using QGraphicsView::QGraphicsView;
  std::vector<QPoint> deltas;
  bool canMove{true};
  bool autoScrollBy(QPoint delta) override
  {
    deltas.push_back(delta);
    return canMove;
  }
};
}

TEST_CASE("Views that know how to move their content are asked to", "[dataflow][autoscroll][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // A scene no bigger than the view: the scroll bars have nothing to scroll,
    // the view itself must be asked (nodal canvas, infinite timeline).
    QGraphicsScene scene{QRectF{0, 0, 10, 10}};
    PanningView view{&scene};
    view.resize(400, 300);
    view.show();
    spin(20);

    FakeCursor cursor;
    CableDragAutoScroller scroller{[](QPointF) {}, cursor.provider()};
    cursor.pos = {view.viewport()->width() - 1, 150};
    scroller.track(view.viewport(), view.mapToScene(cursor.pos));
    CHECK(scroller.active());
    spin(120);
    REQUIRE(!view.deltas.empty());
    for(auto d : view.deltas)
    {
      CHECK(d.x() == maxStep);
      CHECK(d.y() == 0);
    }
    CHECK(scroller.active());

    // Once it reports it cannot move, the scroller idles
    view.canMove = false;
    const auto n = view.deltas.size();
    spin(60);
    CHECK(!scroller.active());
    CHECK(view.deltas.size() == n + 1);

    // The scroll bar fallback is still what plain views get
    QGraphicsScene big{QRectF{0, 0, 4000, 3000}};
    QGraphicsView plain{&big};
    plain.resize(400, 300);
    plain.show();
    spin(20);
    const int h0 = plain.horizontalScrollBar()->value();
    CHECK(CableDragAutoScroller::scrollViewBy(plain, {24, 0}));
    CHECK(plain.horizontalScrollBar()->value() == h0 + 24);
    CHECK(CableDragAutoScroller::scrollViewBy(view, {24, 0}) == false);
    CHECK(view.deltas.size() == n + 2);
  });
}
