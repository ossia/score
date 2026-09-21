// Integration test: the zone that grabs a cable's end to re-plug it.
//
// The innermost pixels around an endpoint belong to the port, which starts a
// *new* cable there. With a grab zone of max(8, 10% of the cable), a short
// cable left about two usable pixels between PortItem::hitRadius and the edge
// of the zone, which is not a target anyone can hit.

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

  SECTION("on a very short cable")
  {
    CHECK(usableBand({0., 0.}, {20., 0.}) >= minBand);
  }
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

TEST_CASE("Both ends in range moves the sink", "[integration][dataflow]")
{
  // "If the two ends are too close, we'll move the destination."
  const QPointF src{0., 0.};
  const QPointF snk{4., 0.};
  REQUIRE(QLineF{src, snk}.length() < CableItem::grabZoneRadius(src, snk));

  CHECK(CableItem::endNear(src, src, snk) == End::Sink);
  CHECK(CableItem::endNear(snk, src, snk) == End::Sink);
  CHECK(CableItem::endNear(QPointF{2., 0.}, src, snk) == End::Sink);
}
