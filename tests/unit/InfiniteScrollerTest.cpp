#include <score/graphics/InfiniteScroller.hpp>
#include <score/tools/PointerLock.hpp>

#include <QApplication>
#include <QCursor>
#include <QEventLoop>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QTimer>

#include <catch2/catch_all.hpp>

namespace
{
struct FakeLock
{
  bool grant{true};
  bool granted{};
  QPointF delta{};
  score::PointerLock::MotionCallback motion{};
  score::PointerLock::ReleaseCallback release{};
};

FakeLock g_lock;

QApplication& app()
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  static int argc = 1;
  static char arg0[] = "test_unit_infinite_scroller";
  static char* argv[] = {arg0, nullptr};
  static QApplication* a = new QApplication{argc, argv};
  return *a;
}

void spin(int ms)
{
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, [&] { loop.quit(); });
  loop.exec();
}

void reset(bool grant)
{
  score::InfiniteScroller::cancel();
  g_lock = FakeLock{};
  g_lock.grant = grant;
}

void setPositions(
    QGraphicsSceneMouseEvent& ev, QPoint screen, QPoint lastScreen, QPoint down)
{
  ev.setScreenPos(screen);
  ev.setLastScreenPos(lastScreen);
  ev.setButtonDownScreenPos(Qt::LeftButton, down);
  ev.setButtons(Qt::LeftButton);
}
}

namespace score
{
bool PointerLock::beginRelative(
    QWindow*, MotionCallback onMotion, ReleaseCallback onRelease) noexcept
{
  if(!g_lock.grant)
    return false;

  g_lock.motion = onMotion;
  g_lock.release = onRelease;
  g_lock.delta = {};
  g_lock.granted = true;
  return true;
}

bool PointerLock::active() noexcept
{
  return g_lock.granted;
}

QPointF PointerLock::takeDelta() noexcept
{
  const QPointF d = g_lock.delta;
  g_lock.delta = {};
  return d;
}

void PointerLock::endRelative() noexcept
{
  g_lock.granted = false;
  g_lock.motion = nullptr;
  g_lock.release = nullptr;
  g_lock.delta = {};
}
}

// The pointer stays where it was pressed for the whole of a locked drag, so any
// delta measured from pointer positions afterwards is the whole drag, backwards.
// Applying it would send the control back to the value it had on press.
TEST_CASE("infinite scroller keeps its value when the lock ends before the release")
{
  app();
  reset(true);

  QGraphicsRectItem item;
  score::InfiniteScroller::start(item, 0.5);

  const double h = score::InfiniteScroller::currentGeometry.height();
  REQUIRE(h > 400.);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();

  double v = 0.5;
  for(int i = 0; i < 3; i++)
  {
    g_lock.delta += QPointF(0., -20.);

    QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
    setPositions(move, press, press, press);
    v = score::InfiniteScroller::move(&move);
  }

  const double dragged = 0.5 + 60. / h;
  REQUIRE(v == Catch::Approx(dragged));

  // The platform reports the button-up on its own channel and the session gets
  // torn down before the widget sees its release.
  REQUIRE(g_lock.release != nullptr);
  g_lock.release();
  spin(200);
  REQUIRE(!score::PointerLock::active());

  QGraphicsSceneMouseEvent release{QEvent::GraphicsSceneMouseRelease};
  setPositions(release, press, press - QPoint{0, 60}, press);
  const double released = score::InfiniteScroller::move(&release);
  score::InfiniteScroller::stop(item, &release);

  REQUIRE(released == Catch::Approx(dragged));
}

TEST_CASE("infinite scroller follows the pointer when the lock is refused")
{
  app();
  reset(false);

  QGraphicsRectItem item;
  score::InfiniteScroller::start(item, 0.5);

  const double h = score::InfiniteScroller::currentGeometry.height();
  REQUIRE(h > 400.);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();

  QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
  setPositions(move, press - QPoint{0, 20}, press, press);
  const double v = score::InfiniteScroller::move(&move);

  QGraphicsSceneMouseEvent release{QEvent::GraphicsSceneMouseRelease};
  setPositions(release, press - QPoint{0, 20}, press - QPoint{0, 20}, press);
  const double released = score::InfiniteScroller::move(&release);
  score::InfiniteScroller::stop(item, &release);

  REQUIRE(v == Catch::Approx(0.5 + 20. / h));
  REQUIRE(released == Catch::Approx(v));
}

TEST_CASE("infinite scroller resumes on pointer deltas when the lock is lost mid-drag")
{
  app();
  reset(true);

  QGraphicsRectItem item;
  score::InfiniteScroller::start(item, 0.5);

  const double h = score::InfiniteScroller::currentGeometry.height();
  REQUIRE(h > 400.);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();

  g_lock.delta += QPointF(0., -20.);
  QGraphicsSceneMouseEvent locked{QEvent::GraphicsSceneMouseMove};
  setPositions(locked, press, press, press);
  score::InfiniteScroller::move(&locked);

  score::PointerLock::endRelative();

  // First move after the lock: the pointer is back where it was pressed.
  QGraphicsSceneMouseEvent stale{QEvent::GraphicsSceneMouseMove};
  setPositions(stale, press, press - QPoint{0, 20}, press);
  const double afterStale = score::InfiniteScroller::move(&stale);
  REQUIRE(afterStale == Catch::Approx(0.5 + 20. / h));

  QGraphicsSceneMouseEvent moved{QEvent::GraphicsSceneMouseMove};
  setPositions(moved, press - QPoint{0, 30}, press, press);
  const double afterMove = score::InfiniteScroller::move(&moved);
  score::InfiniteScroller::stop(item, &moved);

  REQUIRE(afterMove == Catch::Approx(0.5 + 50. / h));
}

// A backend can grant the lock and then never deliver a single delta: a
// compositor that accepts the constraint without sending relative motion, a
// platform whose event selection does not reach us. The pointer keeps moving
// because nothing is holding it, so the drag must go back to reading it.
TEST_CASE("infinite scroller drops a lock that is granted but delivers nothing")
{
  app();
  reset(true);

  QGraphicsRectItem item;
  score::InfiniteScroller::start(item, 0.5);

  const double h = score::InfiniteScroller::currentGeometry.height();
  REQUIRE(h > 400.);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();

  // The lock says it is held, but g_lock.delta is never fed.
  REQUIRE(score::PointerLock::active());

  double v = 0.5;
  for(int i = 1; i <= 4; i++)
  {
    QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
    setPositions(move, press - QPoint{0, 30 * i}, press - QPoint{0, 30 * (i - 1)}, press);
    v = score::InfiniteScroller::move(&move);
  }

  // The first two moves are swallowed by the lock; the third gives up on it and
  // applies its own delta, and the pointer drives the value from then on.
  REQUIRE(!score::PointerLock::active());
  REQUIRE(v == Catch::Approx(0.5 + 60. / h));

  QGraphicsSceneMouseEvent release{QEvent::GraphicsSceneMouseRelease};
  setPositions(release, press - QPoint{0, 120}, press - QPoint{0, 120}, press);
  score::InfiniteScroller::move(&release);
  score::InfiniteScroller::stop(item, &release);
}

// The deferred fallback teardown must only ever end the session that armed it:
// a drag that ends normally leaves the timer running, and the next drag can
// easily start inside the grace window.
TEST_CASE("infinite scroller keeps a second drag started inside the grace window")
{
  app();
  reset(true);

  QGraphicsRectItem item;
  score::InfiniteScroller::start(item, 0.5);

  const double h = score::InfiniteScroller::currentGeometry.height();
  REQUIRE(h > 400.);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();

  g_lock.delta += QPointF(0., -20.);
  QGraphicsSceneMouseEvent m1{QEvent::GraphicsSceneMouseMove};
  setPositions(m1, press, press, press);
  score::InfiniteScroller::move(&m1);

  // The platform reports the button-up on its own channel, then the widget gets
  // its own release right away, as it normally does.
  REQUIRE(g_lock.release != nullptr);
  g_lock.release();

  QGraphicsSceneMouseEvent r1{QEvent::GraphicsSceneMouseRelease};
  setPositions(r1, press, press, press);
  score::InfiniteScroller::move(&r1);
  score::InfiniteScroller::stop(item, &r1);
  REQUIRE(!score::PointerLock::active());

  reset(true);
  score::InfiniteScroller::start(item, 0.5);
  REQUIRE(score::PointerLock::active());

  g_lock.delta += QPointF(0., -40.);
  QGraphicsSceneMouseEvent m2{QEvent::GraphicsSceneMouseMove};
  setPositions(m2, press, press, press);
  REQUIRE(score::InfiniteScroller::move(&m2) == Catch::Approx(0.5 + 40. / h));

  // The first drag's deferred teardown fires here.
  spin(200);
  REQUIRE(score::PointerLock::active());

  g_lock.delta += QPointF(0., -40.);
  QGraphicsSceneMouseEvent m3{QEvent::GraphicsSceneMouseMove};
  setPositions(m3, press, press, press);
  REQUIRE(score::InfiniteScroller::move(&m3) == Catch::Approx(0.5 + 80. / h));

  QGraphicsSceneMouseEvent r2{QEvent::GraphicsSceneMouseRelease};
  setPositions(r2, press, press, press);
  score::InfiniteScroller::stop(item, &r2);
}

// A drag torn down without the widget ever seeing a release must not leave the
// stale-delta guard armed for the next one.
TEST_CASE("infinite scroller applies the first delta of the drag after a teardown")
{
  app();
  reset(true);

  QGraphicsRectItem item;
  score::InfiniteScroller::start(item, 0.5);

  const double h = score::InfiniteScroller::currentGeometry.height();
  REQUIRE(h > 400.);
  const QPoint press = score::InfiniteScroller::currentGeometry.center().toPoint();

  g_lock.delta += QPointF(0., -20.);
  QGraphicsSceneMouseEvent m1{QEvent::GraphicsSceneMouseMove};
  setPositions(m1, press, press, press);
  score::InfiniteScroller::move(&m1);

  REQUIRE(g_lock.release != nullptr);
  g_lock.release();
  spin(200);
  REQUIRE(!score::PointerLock::active());

  reset(false);
  score::InfiniteScroller::start(item, 0.5);

  QGraphicsSceneMouseEvent m2{QEvent::GraphicsSceneMouseMove};
  setPositions(m2, press - QPoint{0, 10}, press, press);
  const double v = score::InfiniteScroller::move(&m2);
  score::InfiniteScroller::stop(item, &m2);

  REQUIRE(v == Catch::Approx(0.5 + 10. / h));
}

// A locked drag as the platform delivers it: each relative motion is reported
// to the scroller, which synthesizes a move at the virtual pointer, while the
// real pointer stays on the press point.
struct LockedDrag
{
  QGraphicsScene scene;
  QGraphicsView view{&scene};
  QGraphicsRectItem item{0., 0., 10., 10.};
  QPoint press{400, 300};
  QPointF virtualPos{press};
  double height{};

  LockedDrag()
  {
    app();
    reset(true);
    scene.addItem(&item);
    view.setGeometry(0, 0, 1000, 800);
    view.show();
    QCursor::setPos(press);
    score::InfiniteScroller::start(item, 0.5);
    height = score::InfiniteScroller::currentGeometry.height();
  }

  ~LockedDrag()
  {
    QGraphicsSceneMouseEvent release{QEvent::GraphicsSceneMouseRelease};
    setPositions(release, press, virtualPos.toPoint(), press);
    score::InfiniteScroller::stop(item, &release);
    scene.removeItem(&item);
  }

  double synthesized(QPointF delta)
  {
    if(score::PointerLock::active())
    {
      g_lock.delta += delta;
      g_lock.motion(delta);
    }
    const QPoint last = virtualPos.toPoint();
    virtualPos += delta;

    QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
    setPositions(move, virtualPos.toPoint(), last, press);
    return score::InfiniteScroller::move(&move);
  }

  double replayed()
  {
    QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
    setPositions(move, virtualPos.toPoint(), virtualPos.toPoint(), press);
    return score::InfiniteScroller::move(&move);
  }

  double held()
  {
    QGraphicsSceneMouseEvent move{QEvent::GraphicsSceneMouseMove};
    setPositions(move, press, virtualPos.toPoint(), press);
    return score::InfiniteScroller::move(&move);
  }
};

// A drag that has gone down and then wanders sideways must stay on the lock:
// dropping it there would read the real pointer back on the press point, and
// apply that as the whole drag, backwards.
TEST_CASE("infinite scroller keeps the lock through sideways motion")
{
  LockedDrag drag;
  REQUIRE(QCursor::pos() == drag.press);
  REQUIRE(drag.view.window()->windowHandle() != nullptr);
  const double h = drag.height;
  REQUIRE(h > 400.);

  double v = 0.5;
  for(int i = 0; i < 10; i++)
    v = drag.synthesized({0., 10.});
  REQUIRE(v == Catch::Approx(0.5 - 100. / h));

  for(int i = 0; i < 4; i++)
    v = drag.synthesized({1., 0.});
  REQUIRE(v == Catch::Approx(0.5 - 100. / h));

  REQUIRE(drag.held() == Catch::Approx(0.5 - 100. / h));
  REQUIRE(score::PointerLock::active());

  REQUIRE(drag.synthesized({0., 1.}) == Catch::Approx(0.5 - 101. / h));
}

// The view replays its last mouse event when it scrolls: during a locked drag
// that is a synthesized move, sent again with no motion behind it. However far
// it is from the press point, it says nothing about the real pointer.
TEST_CASE("infinite scroller keeps the lock through replayed moves")
{
  LockedDrag drag;
  REQUIRE(QCursor::pos() == drag.press);
  REQUIRE(drag.view.window()->windowHandle() != nullptr);
  const double h = drag.height;
  REQUIRE(h > 400.);

  REQUIRE(drag.synthesized({0., 60.}) == Catch::Approx(0.5 - 60. / h));

  for(int i = 0; i < 4; i++)
    REQUIRE(drag.replayed() == Catch::Approx(0.5 - 60. / h));

  REQUIRE(drag.held() == Catch::Approx(0.5 - 60. / h));
  REQUIRE(score::PointerLock::active());

  REQUIRE(drag.synthesized({0., 1.}) == Catch::Approx(0.5 - 61. / h));
}
