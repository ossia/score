#include <Curve/CurveModel.hpp>
#include <Curve/Segment/EasingSegment.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <QDataStream>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;

QDataStream& operator<<(QDataStream& s, const Curve::EasingData&)
{
  return s;
}
QDataStream& operator>>(QDataStream& s, Curve::EasingData&)
{
  return s;
}

namespace
{
const Curve::SegmentModel& base(const Curve::SegmentModel& s)
{
  return s;
}
}

TEST_CASE("Linear segment interpolates exactly", "[curve][segment][linear]")
{
  QObject parent;
  Curve::LinearSegment seg{Id<Curve::SegmentModel>{1}, &parent};
  seg.setStart({0., 0.});
  seg.setEnd({1., 1.});

  CHECK(base(seg).valueAt(0.) == 0.);
  CHECK(base(seg).valueAt(0.25) == 0.25);
  CHECK(base(seg).valueAt(0.5) == 0.5);
  CHECK(base(seg).valueAt(1.) == 1.);

  // Non-trivial span and range: (0.2, 1) -> (0.8, 4)
  Curve::LinearSegment seg2{Id<Curve::SegmentModel>{2}, &parent};
  seg2.setStart({0.2, 1.});
  seg2.setEnd({0.8, 4.});
  CHECK(base(seg2).valueAt(0.2) == Approx(1.).margin(1e-12));
  CHECK(base(seg2).valueAt(0.5) == Approx(2.5).margin(1e-12));
  CHECK(base(seg2).valueAt(0.8) == Approx(4.).margin(1e-12));

  // Its ossia execution functor is plain linear interpolation of the ratio.
  auto f = base(seg2).makeDoubleFunction();
  CHECK(f(0., 1., 4.) == Approx(1.).margin(1e-12));
  CHECK(f(0.3, 0., 10.) == Approx(3.).margin(1e-12));
  CHECK(f(1., 1., 4.) == Approx(4.).margin(1e-12));
}

TEST_CASE("Power segment with linear gamma equals linear", "[curve][segment][power]")
{
  QObject parent;
  Curve::PowerSegment seg{Id<Curve::SegmentModel>{1}, &parent};
  seg.setStart({0., 2.});
  seg.setEnd({1., 6.});
  REQUIRE(seg.gamma == Curve::PowerSegmentData::linearGamma);

  CHECK(base(seg).valueAt(0.) == Approx(2.).margin(1e-12));
  CHECK(base(seg).valueAt(0.25) == Approx(3.).margin(1e-12));
  CHECK(base(seg).valueAt(0.75) == Approx(5.).margin(1e-12));
  CHECK(base(seg).valueAt(1.) == Approx(6.).margin(1e-12));
}

TEST_CASE("Power segment samples y = y0 + dy * x^gamma on [0,1]", "[curve][segment][power]")
{
  QObject parent;
  Curve::PowerSegment seg{Id<Curve::SegmentModel>{1}, &parent};
  seg.setStart({0., 0.});
  seg.setEnd({1., 1.});
  seg.gamma = 2.;

  CHECK(base(seg).valueAt(0.) == Approx(0.).margin(1e-12));
  CHECK(base(seg).valueAt(0.25) == Approx(0.0625).margin(1e-12));
  CHECK(base(seg).valueAt(0.5) == Approx(0.25).margin(1e-12));
  CHECK(base(seg).valueAt(0.75) == Approx(0.5625).margin(1e-12));
  CHECK(base(seg).valueAt(1.) == Approx(1.).margin(1e-12));

  seg.gamma = 0.5; // sqrt
  CHECK(base(seg).valueAt(0.25) == Approx(0.5).margin(1e-12));
  CHECK(base(seg).valueAt(0.81) == Approx(0.9).margin(1e-12));

  // Scaled y-range: (0,2) -> (1,6), gamma 2 => y = 2 + 4 x^2
  Curve::PowerSegment seg2{Id<Curve::SegmentModel>{2}, &parent};
  seg2.setStart({0., 2.});
  seg2.setEnd({1., 6.});
  seg2.gamma = 2.;
  CHECK(base(seg2).valueAt(0.5) == Approx(3.).margin(1e-12));
  CHECK(base(seg2).valueAt(0.75) == Approx(4.25).margin(1e-12));

  // Execution functor: ratio-based power easing.
  auto f = base(seg2).makeDoubleFunction();
  CHECK(f(0.5, 0., 10.) == Approx(2.5).margin(1e-12));  // 0.5^2 * 10
  CHECK(f(0.25, 0., 16.) == Approx(1.).margin(1e-12));  // 0.0625 * 16
  CHECK(f(1., 2., 6.) == Approx(6.).margin(1e-12));
}

TEST_CASE("Power segment vertical parameter maps to gamma = 16^-p", "[curve][segment][power]")
{
  QObject parent;
  Curve::PowerSegment seg{Id<Curve::SegmentModel>{1}, &parent};
  seg.setStart({0., 0.});
  seg.setEnd({1., 1.}); // ascending: gamma = 16^-p
  Curve::SegmentModel& s = seg; // the parameter API is on the base interface

  s.setVerticalParameter(1.);
  CHECK(seg.gamma == Approx(1. / 16.).margin(1e-12));
  REQUIRE(s.verticalParameter());
  CHECK(*s.verticalParameter() == Approx(1.).margin(1e-12));

  s.setVerticalParameter(-0.5);
  CHECK(seg.gamma == Approx(4.).margin(1e-12));
  CHECK(*s.verticalParameter() == Approx(-0.5).margin(1e-12));

  s.setVerticalParameter(0.);
  CHECK(seg.gamma == Approx(1.).margin(1e-12)); // back to linear
}

TEST_CASE("Power segment updateData interpolation lies on the analytic curve", "[curve][segment][power]")
{
  QObject parent;
  Curve::PowerSegment seg{Id<Curve::SegmentModel>{1}, &parent};
  seg.setStart({0., 0.});
  seg.setEnd({1., 1.});
  seg.gamma = 2.;

  const int numInterp = 10;
  base(seg).updateData(numInterp);
  const auto& data = seg.data();
  REQUIRE(data.size() == std::size_t(numInterp + 1));

  // Endpoints exact.
  CHECK(data.front().x() == Approx(0.).margin(1e-12));
  CHECK(data.front().y() == Approx(0.).margin(1e-12));
  CHECK(data.back().x() == Approx(1.).margin(1e-12));
  CHECK(data.back().y() == Approx(1.).margin(1e-12));

  for(std::size_t i = 0; i < data.size(); i++)
  {
    // On [0,1] with gamma 2 every cached point must satisfy y = x^2.
    CHECK(data[i].y() == Approx(std::pow(data[i].x(), 2.)).margin(1e-9));
    // Monotonic non-decreasing x.
    if(i > 0)
      CHECK(data[i].x() >= data[i - 1].x());
  }

  // Linear-gamma segments collapse the cache to the two endpoints.
  Curve::PowerSegment lin{Id<Curve::SegmentModel>{2}, &parent};
  lin.setStart({0., 3.});
  lin.setEnd({1., 5.});
  base(lin).updateData(10);
  REQUIRE(lin.data().size() == 2);
  CHECK(lin.data()[0] == QPointF(0., 3.));
  CHECK(lin.data()[1] == QPointF(1., 5.));
}

TEST_CASE("Easing segments sample their easing function", "[curve][segment][easing]")
{
  QObject parent;

  Curve::Segment_quadraticIn quadIn{Id<Curve::SegmentModel>{1}, &parent};
  quadIn.setStart({0., 0.});
  quadIn.setEnd({1., 1.});
  CHECK(quadIn.valueAt(0.5) == Approx(0.25).margin(1e-12));
  CHECK(quadIn.valueAt(0.25) == Approx(0.0625).margin(1e-12));
  CHECK(quadIn.valueAt(1.) == Approx(1.).margin(1e-12));

  Curve::Segment_quadraticOut quadOut{Id<Curve::SegmentModel>{2}, &parent};
  quadOut.setStart({0., 0.});
  quadOut.setEnd({1., 1.});
  // quadraticOut(t) = -(t * (t - 2))
  CHECK(quadOut.valueAt(0.5) == Approx(0.75).margin(1e-12));
  CHECK(quadOut.valueAt(0.25) == Approx(0.4375).margin(1e-12));

  Curve::Segment_cubicInOut cubicIO{Id<Curve::SegmentModel>{3}, &parent};
  cubicIO.setStart({0., 0.});
  cubicIO.setEnd({1., 1.});
  // t < 0.5 : 4 t^3
  CHECK(cubicIO.valueAt(0.25) == Approx(0.0625).margin(1e-12));
  CHECK(cubicIO.valueAt(0.5) == Approx(0.5).margin(1e-12));
  // t >= 0.5 : 0.5 (2t-2)^3 + 1
  CHECK(cubicIO.valueAt(0.75) == Approx(0.9375).margin(1e-12));

  // Scaled into y-range [10, 20]: y = 10 + 10 * quadIn(x)
  Curve::Segment_quadraticIn scaled{Id<Curve::SegmentModel>{4}, &parent};
  scaled.setStart({0., 10.});
  scaled.setEnd({1., 20.});
  CHECK(scaled.valueAt(0.5) == Approx(12.5).margin(1e-12));

  // updateData samples the same shape.
  quadIn.updateData(4);
  REQUIRE(quadIn.data().size() == 5);
  for(const auto& pt : quadIn.data())
    CHECK(pt.y() == Approx(pt.x() * pt.x()).margin(1e-12));

  // Execution functor = ratio-based easing (matches valueAt on [0,1]).
  auto f = quadIn.makeDoubleFunction();
  CHECK(f(0.5, 0., 1.) == Approx(0.25).margin(1e-12));
  CHECK(f(0.5, 0., 8.) == Approx(2.).margin(1e-12));
}

TEST_CASE("flatCurveSegment normalizes the value into [min,max]", "[curve][segment]")
{
  const auto seg = Curve::flatCurveSegment(5., 0., 10.);
  CHECK(seg.start.x() == 0.);
  CHECK(seg.end.x() == 1.);
  CHECK(seg.start.y() == Approx(0.5).margin(1e-12));
  CHECK(seg.end.y() == Approx(0.5).margin(1e-12));
  CHECK(seg.type == Metadata<ConcreteKey_k, Curve::PowerSegment>::get());

  const auto seg2 = Curve::flatCurveSegment(-2., -4., 4.);
  CHECK(seg2.start.y() == Approx(0.25).margin(1e-12));
}

TEST_CASE("Curve::Model samples across multiple segments", "[curve][model]")
{
  QObject parent;
  Curve::Model model{Id<Curve::Model>{1}, &parent};

  SECTION("empty curve has no value anywhere")
  {
    CHECK(!model.valueAt(0.));
    CHECK(!model.valueAt(0.5));
    CHECK(!model.valueAt(1.));
  }

  SECTION("single sub-range segment: out-of-domain x yields nullopt")
  {
    auto seg = new Curve::LinearSegment{Id<Curve::SegmentModel>{1}, &model};
    seg->setStart({0.25, 0.});
    seg->setEnd({0.75, 1.});
    model.addSegment(seg);

    CHECK(!model.valueAt(0.1));
    CHECK(!model.valueAt(0.8));
    REQUIRE(model.valueAt(0.25));
    CHECK(*model.valueAt(0.25) == Approx(0.).margin(1e-12));
    CHECK(*model.valueAt(0.5) == Approx(0.5).margin(1e-12));
    CHECK(*model.valueAt(0.75) == Approx(1.).margin(1e-12));
  }

  SECTION("triangle curve: piecewise values and continuity at the junction")
  {
    auto up = new Curve::LinearSegment{Id<Curve::SegmentModel>{1}, &model};
    up->setStart({0., 0.});
    up->setEnd({0.5, 1.});
    auto down = new Curve::LinearSegment{Id<Curve::SegmentModel>{2}, &model};
    down->setStart({0.5, 1.});
    down->setEnd({1., 0.});
    model.addSegment(up);
    model.addSegment(down);

    CHECK(*model.valueAt(0.) == Approx(0.).margin(1e-12));
    CHECK(*model.valueAt(0.25) == Approx(0.5).margin(1e-12));
    CHECK(*model.valueAt(0.5) == Approx(1.).margin(1e-12));
    CHECK(*model.valueAt(0.75) == Approx(0.5).margin(1e-12));
    CHECK(*model.valueAt(1.) == Approx(0.).margin(1e-12));

    // Both segments agree at the shared point.
    const Curve::SegmentModel& u = *up;
    const Curve::SegmentModel& d = *down;
    CHECK(u.valueAt(0.5) == Approx(d.valueAt(0.5)).margin(1e-12));

    // sortedSegments orders by start abscissa.
    auto sorted = model.sortedSegments();
    REQUIRE(sorted.size() == 2);
    CHECK(sorted[0] == up);
    CHECK(sorted[1] == down);
  }

  SECTION("power segment on a sub-range: model sampling matches the engine")
  {
    auto seg = new Curve::PowerSegment{Id<Curve::SegmentModel>{1}, &model};
    seg->setStart({0.5, 1.});
    seg->setEnd({1., 3.});
    seg->gamma = 2.;
    model.addSegment(seg);

    REQUIRE(model.valueAt(0.75));
    CHECK(*model.valueAt(0.75) == Approx(1.5).margin(1e-12)); // model == engine
    const Curve::SegmentModel& s = *seg;
    CHECK(s.makeDoubleFunction()(0.5, 1., 3.) == Approx(1.5).margin(1e-12)); // ratio
  }
}
