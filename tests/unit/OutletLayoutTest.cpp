// Where a node draws its outlets.
//
// score::GraphicsIORootLayout pushes the outlet column against the right-hand
// edge of a node that has inlets too. A node with outlets only went through
// GraphicsDefaultOutletLayout on its own, which laid the column out from x = 0
// -- so the texture outlet of a generator such as the Random Characters
// shader, folded, was drawn on the LEFT of the object.

#include <score/graphics/layouts/GraphicsGridLayout.hpp>

#include <score_test/App.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <catch2/catch_all.hpp>

namespace
{
//! No pen: QGraphicsRectItem's bounding rect otherwise carries half a pen
//! width on each side, which has nothing to do with what is being measured.
QGraphicsRectItem* box(QGraphicsItem& parent, double w, double h)
{
  auto* it = new QGraphicsRectItem{QRectF{0., 0., w, h}, &parent};
  it->setPen(Qt::NoPen);
  return it;
}
}

TEST_CASE("an outlet-only column sits against the right edge of the node")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    score::GraphicsDefaultOutletLayout lay{nullptr};
    scene.addItem(&lay);

    auto* wide = box(lay, 30., 10.);
    auto* narrow = box(lay, 10., 10.);

    lay.setMinimumWidth(200.);
    lay.layout();

    // Both right edges land on the node's right edge.
    CHECK(wide->pos().x() + 30. == Catch::Approx(200.));
    CHECK(narrow->pos().x() + 10. == Catch::Approx(200.));
    // ... stacked, not overlapping.
    CHECK(narrow->pos().y() == Catch::Approx(10.));
  });
}

TEST_CASE("an outlet column with no width to fill still lines up on its own right")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    score::GraphicsDefaultOutletLayout lay{nullptr};
    scene.addItem(&lay);

    auto* wide = box(lay, 30., 10.);
    auto* narrow = box(lay, 10., 10.);

    // No minimum: the behaviour a node with inlets relies on, where the root
    // layout does the pushing.
    lay.layout();

    CHECK(wide->pos().x() == Catch::Approx(0.));
    CHECK(narrow->pos().x() == Catch::Approx(20.));
  });
}

TEST_CASE("a column wider than the node is not pushed off it")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    score::GraphicsDefaultOutletLayout lay{nullptr};
    scene.addItem(&lay);

    auto* wide = box(lay, 120., 10.);
    lay.setMinimumWidth(40.);
    lay.layout();

    CHECK(wide->pos().x() == Catch::Approx(0.));
  });
}

// The inlet column is the mirror image and must stay where it is.
TEST_CASE("an inlet column stays on the left")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    score::GraphicsDefaultInletLayout lay{nullptr};
    scene.addItem(&lay);

    auto* wide = box(lay, 30., 10.);
    auto* narrow = box(lay, 10., 10.);
    lay.layout();

    CHECK(wide->pos().x() == Catch::Approx(0.));
    CHECK(narrow->pos().x() == Catch::Approx(0.));
  });
}
