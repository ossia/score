// Integration test: the zone that grabs a cable's end to re-plug it.
//
// The innermost pixels around an endpoint belong to the port, which starts a
// *new* cable there, so the zone has to clear it by enough to be aimed at. It
// also has to stop short of the middle: two zones that meet leave a cable that
// can only ever be re-plugged, never selected -- which is what a short cable,
// or any cable on a zoomed-out canvas, turns into.

#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <QLineF>
#include <QPointF>

#include <catch2/catch_test_macros.hpp>

using Dataflow::CableItem;
using End = Dataflow::CableItem::GrabbedEnd;

namespace
{
//! The band a user can actually aim at: outside the port, inside the zone.
double usableBand(QPointF p1, QPointF p2) noexcept
{
  return CableItem::grabZoneRadius(p1, p2) - Dataflow::PortItem::hitRadius;
}
}

TEST_CASE("A cable end is grabbed near it and nowhere else", "[integration][dataflow]")
{
  const QPointF src{0., 0.};
  const QPointF snk{400., 0.};
  const double r = CableItem::grabZoneRadius(src, snk);

  CHECK(CableItem::endNear(src, src, snk) == End::Source);
  CHECK(CableItem::endNear(snk, src, snk) == End::Sink);

  // Just inside each zone
  CHECK(CableItem::endNear(src + QPointF{r - 1., 0.}, src, snk) == End::Source);
  CHECK(CableItem::endNear(snk - QPointF{r - 1., 0.}, src, snk) == End::Sink);

  // Just outside: the middle of the cable selects, it does not re-plug
  CHECK(CableItem::endNear(src + QPointF{r + 1., 0.}, src, snk) == End::None);
  CHECK(CableItem::endNear(snk - QPointF{r + 1., 0.}, src, snk) == End::None);
  CHECK(CableItem::endNear(QPointF{200., 0.}, src, snk) == End::None);

  // The zone is a disc, not a band along the cable
  CHECK(CableItem::endNear(src + QPointF{0., r - 1.}, src, snk) == End::Source);
  CHECK(CableItem::endNear(src + QPointF{0., r + 1.}, src, snk) == End::None);
}

TEST_CASE("The grab zone clears the port by a hittable margin", "[integration][dataflow]")
{
  // A port is ~5.5px; anything less than a few times that is not aimable.
  const double minBand = 4 * Dataflow::PortItem::hitRadius;

  SECTION("on a typical cable")
  {
    CHECK(usableBand({0., 0.}, {200., 0.}) >= minBand);
  }
  SECTION("on a long cable the zone grows with it")
  {
    CHECK(
        CableItem::grabZoneRadius({0., 0.}, {2000., 0.})
        > CableItem::grabZoneRadius({0., 0.}, {200., 0.}));
  }
}

TEST_CASE("Every cable keeps a middle that selects", "[integration][dataflow]")
{
  // The zones must never meet, at any length: the midpoint has to fall outside
  // both of them, or the cable cannot be clicked without arming a re-plug.
  for(double len : {4., 10., 20., 40., 56., 60., 120., 400., 2000., 10000.})
  {
    const QPointF a{0., 0.};
    const QPointF b{len, 0.};
    INFO("cable length " << len);
    CHECK(CableItem::grabZoneRadius(a, b) < len / 2.);
    CHECK(CableItem::endNear(QPointF{len / 2., 0.}, a, b) == End::None);
  }
}

TEST_CASE("Ends that cannot be told apart move the sink", "[integration][dataflow]")
{
  // "If the two ends are too close, we'll move the destination." Once the two
  // ports coincide there is no geometry left to choose by, so the order the two
  // are tested in is the rule.
  const QPointF p{10., 10.};
  CHECK(CableItem::endNear(p, p, p) == End::Sink);
}
