// The second node of a source is a handle: how far it sits from the first is
// the size of the shape and the direction it points is where the shape starts.
// Every closed trajectory therefore passes through it, which is what makes it
// visibly connected to the shape instead of a point that drifts on its own.

#include <score_test/App.hpp>

#include <score/graphics/widgets/QGraphicsPathGeneratorXY.hpp>

#include <ossia/network/value/value.hpp>

#include <QImage>
#include <QPainter>
#include <QPointF>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;

namespace
{
enum Path
{
  Linear,
  Circle,
  Spiral,
  Lissajous,
  Rose,
  Polygon
};

std::vector<ossia::value> source(ossia::vec2f a, ossia::vec2f b)
{
  return {ossia::value{a}, ossia::value{b}};
}

//! Where the handle itself is drawn, in the same item coordinates pathPoint uses.
QPointF handlePos(const score::QGraphicsPathGeneratorXY& w, ossia::vec2f b)
{
  return QPointF{b[0] * w.width(), (1. - b[1]) * w.height()};
}

void checkSame(QPointF a, QPointF b)
{
  CHECK(a.x() == Approx(b.x()).margin(1e-6));
  CHECK(a.y() == Approx(b.y()).margin(1e-6));
}
}

TEST_CASE("path generator: the handle sets where a shape starts", "[gfx][path]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsPathGeneratorXY w{nullptr};

    const ossia::vec2f a{0.5f, 0.5f};
    const ossia::vec2f b{0.8f, 0.5f};
    const auto src = source(a, b);

    SECTION("a circle passes through it")
    {
      w.setPathMode(Circle);
      checkSame(w.pathPoint(src, 0.), handlePos(w, b));
    }

    SECTION("a polygon puts a vertex on it")
    {
      w.setPathMode(Polygon);
      w.setRatioX(3);
      checkSame(w.pathPoint(src, 0.), handlePos(w, b));
    }

    SECTION("a rose starts on it whatever the petal count")
    {
      w.setPathMode(Rose);
      w.setRatioX(5);
      w.setRatioY(1);
      checkSame(w.pathPoint(src, 0.), handlePos(w, b));
    }

    SECTION("a spiral ends on it")
    {
      w.setPathMode(Spiral);
      checkSame(w.pathPoint(src, 1.), handlePos(w, b));
    }

    SECTION("a line ends on it")
    {
      w.setPathMode(Linear);
      checkSame(w.pathPoint(src, 1.), handlePos(w, b));
    }
  });
}

TEST_CASE("path generator: the handle sets the shape's size", "[gfx][path]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsPathGeneratorXY w{nullptr};
    w.setPathMode(Circle);

    const ossia::vec2f a{0.5f, 0.5f};
    const auto near = source(a, ossia::vec2f{0.6f, 0.5f});
    const auto far = source(a, ossia::vec2f{0.9f, 0.5f});

    // Half a turn later the point is on the far side of the centre, so the
    // bigger handle distance must give the bigger excursion.
    const QPointF centre{a[0] * w.width(), (1. - a[1]) * w.height()};
    const double dNear = std::abs(w.pathPoint(near, 0.5).x() - centre.x());
    const double dFar = std::abs(w.pathPoint(far, 0.5).x() - centre.x());

    CHECK(dFar > dNear);
  });
}

TEST_CASE("path generator: rotating the handle rotates the shape", "[gfx][path]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsPathGeneratorXY w{nullptr};
    w.setPathMode(Circle);

    const ossia::vec2f a{0.5f, 0.5f};
    const auto right = source(a, ossia::vec2f{0.8f, 0.5f});
    const auto up = source(a, ossia::vec2f{0.5f, 0.8f});

    // Same radius, quarter turn apart: each starts on its own handle.
    checkSame(w.pathPoint(right, 0.), handlePos(w, ossia::vec2f{0.8f, 0.5f}));
    checkSame(w.pathPoint(up, 0.), handlePos(w, ossia::vec2f{0.5f, 0.8f}));
  });
}

TEST_CASE("path generator: a source with no handle does not move", "[gfx][path]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsPathGeneratorXY w{nullptr};
    const ossia::vec2f a{0.5f, 0.5f};
    const std::vector<ossia::value> lone{ossia::value{a}};

    for(int mode = Linear; mode <= Polygon; mode++)
    {
      w.setPathMode(mode);
      checkSame(w.pathPoint(lone, 0.), handlePos(w, a));
      checkSame(w.pathPoint(lone, 0.5), handlePos(w, a));
    }
  });
}

TEST_CASE("path generator: the editor is sized, not hard-coded", "[gfx][path]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsPathGeneratorXY w{nullptr};

    CHECK(w.width() == score::QGraphicsPathGeneratorXY::defaultSize.width());
    CHECK(w.height() == score::QGraphicsPathGeneratorXY::defaultSize.height());

    const ossia::vec2f a{0.5f, 0.5f};
    const auto src = source(a, ossia::vec2f{0.8f, 0.5f});
    w.setPathMode(Circle);

    w.setSize(QSizeF{120., 80.});
    CHECK(w.width() == 120.);
    CHECK(w.height() == 80.);
    // Positions are normalised, so they follow the size in both axes
    checkSame(w.pathPoint(src, 0.), QPointF{0.8 * 120., 0.5 * 80.});

    // ... and it does not collapse to nothing
    w.setSize(QSizeF{1., 1.});
    CHECK(w.width() == score::QGraphicsPathGeneratorXY::minimumSize.width());
    CHECK(w.height() == score::QGraphicsPathGeneratorXY::minimumSize.height());
  });
}

TEST_CASE("path generator: painting leaves the painter as it found it", "[gfx][path]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsPathGeneratorXY w{nullptr};
    w.setPathMode(Circle);
    w.setValue(ossia::value{std::vector<ossia::value>{
        ossia::value{source(ossia::vec2f{0.5f, 0.5f}, ossia::vec2f{0.8f, 0.5f})}}});

    QImage img{64, 64, QImage::Format_ARGB32};
    QPainter painter{&img};
    QGraphicsItem& item = w;

    // The whole node paints through this painter: a clip left behind cuts the
    // items drawn after the editor, the outlets among them.
    item.paint(&painter, nullptr, nullptr);
    CHECK(!painter.hasClipping());

    const QRectF clip{8., 8., 16., 16.};
    painter.setClipRect(clip);
    item.paint(&painter, nullptr, nullptr);
    CHECK(painter.hasClipping());
    CHECK(painter.clipBoundingRect() == clip);
  });
}
