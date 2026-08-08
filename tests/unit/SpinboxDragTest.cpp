#include <score/graphics/DefaultGraphicsSpinboxImpl.hpp>
#include <score/graphics/InfiniteScroller.hpp>

#include <QApplication>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_all.hpp>

namespace
{
QApplication& app()
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  static int argc = 1;
  static char arg0[] = "test_unit_spinbox_drag";
  static char* argv[] = {arg0, nullptr};
  static QApplication* a = new QApplication{argc, argv};
  return *a;
}

struct FakeSpinbox
{
  double min{};
  double max{};
  double m_value{};
};

void setPositions(
    QGraphicsSceneMouseEvent& ev, QPoint screen, QPoint lastScreen, QPoint down)
{
  ev.setScreenPos(screen);
  ev.setLastScreenPos(lastScreen);
  ev.setButtonDownScreenPos(Qt::LeftButton, down);
  ev.setButtons(Qt::LeftButton);
}

// Drags by `step` pixels, `count` times, and answers the last mapped value.
double drag(FakeSpinbox& box, QPoint& at, QPoint press, int step, int count)
{
  double v = 0.;
  for(int i = 0; i < count; i++)
  {
    const QPoint next = at + QPoint{0, step};
    QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
    setPositions(move, next, at, press);
    v = score::DefaultGraphicsSpinboxImpl::mapValue(box, &move);
    at = next;
  }
  return v;
}
}

// The spinbox maps the drag itself rather than going through
// InfiniteScroller::move(), which is what holds the accumulator at the ends for
// every other control. Overshooting used to keep piling delta up, so the drag
// had to be walked all the way back before the value moved again -- reachable
// without limit as soon as the pointer can be locked in place.
TEST_CASE("spinbox drag does not wind up past its bounds")
{
  app();

  QGraphicsRectItem item;
  FakeSpinbox box{0., 1., 0.5};

  score::InfiniteScroller::start(item, 0.5);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();
  QPoint at = press;

  REQUIRE(drag(box, at, press, 40, 20) == Catch::Approx(0.));

  const QPoint back = at - QPoint{0, 40};
  QGraphicsSceneMouseEvent reverse{QEvent::GraphicsSceneMouseMove};
  setPositions(reverse, back, at, press);
  REQUIRE(score::DefaultGraphicsSpinboxImpl::mapValue(box, &reverse) > 0.);

  score::InfiniteScroller::cancel();
}

TEST_CASE("spinbox drag does not wind up past its upper bound")
{
  app();

  QGraphicsRectItem item;
  FakeSpinbox box{0., 1., 0.5};

  score::InfiniteScroller::start(item, 0.5);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();
  QPoint at = press;

  REQUIRE(drag(box, at, press, -40, 20) == Catch::Approx(1.));

  const QPoint back = at + QPoint{0, 40};
  QGraphicsSceneMouseEvent reverse{QEvent::GraphicsSceneMouseMove};
  setPositions(reverse, back, at, press);
  REQUIRE(score::DefaultGraphicsSpinboxImpl::mapValue(box, &reverse) < 1.);

  score::InfiniteScroller::cancel();
}
