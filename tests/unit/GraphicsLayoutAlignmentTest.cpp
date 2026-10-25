// halp_meta(alignment, ...) on a layout: where the children sit across the
// free space of their cell, row or column. Without it, every layout keeps its
// own placement.

#include <Control/Layout.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>

#include <score_test/App.hpp>

#include <QGraphicsScene>

#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
std::vector<QGraphicsItem*>
alignmentTestBoxes(QGraphicsItem* parent, std::initializer_list<double> widths)
{
  std::vector<QGraphicsItem*> res;
  for(double w : widths)
  {
    auto* box = new score::EmptyRectItem{parent};
    box->setRect({0., 0., w, 10.});
    res.push_back(box);
  }
  return res;
}

double sceneX(const QGraphicsItem* item)
{
  return item->scenePos().x();
}
double sceneY(const QGraphicsItem* item)
{
  return item->scenePos().y();
}

struct left_aligned_grid
{
  halp_meta(layout, halp::layouts::grid)
  halp_meta(columns, 3)
  halp_meta(alignment, halp::alignments::left)
};
}

TEST_CASE("Alignment metas map to Qt alignments", "[graphics][layout]")
{
  using B = Process::LayoutBuilderBase;
  CHECK(B::get_alignment(halp::alignments::left) == Qt::AlignLeft);
  CHECK(B::get_alignment(halp::alignments::center) == Qt::AlignHCenter);
  CHECK(B::get_alignment(halp::alignments::right) == Qt::AlignRight);
  CHECK(B::get_alignment(left_aligned_grid::alignment()) == Qt::AlignLeft);

  // Any enum with those names, as for the other layout metas
  enum class own
  {
    right
  };
  CHECK(B::get_alignment(own::right) == Qt::AlignRight);
}

TEST_CASE("A grid without alignment keeps its uniform centred cells", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* grid = new score::GraphicsGridColumnsLayout{nullptr};
    scene.addItem(grid);
    grid->setColumns(2);
    grid->setPadding(4.);
    const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
    grid->layout();

    // Cells: 40 + 4 wide, 10 + 4 high, from (4, 4)
    CHECK(sceneX(b[0]) == 6.);
    CHECK(sceneX(b[1]) == 65.);
    CHECK(sceneX(b[2]) == 16.);
    CHECK(sceneX(b[3]) == 55.);
    CHECK(sceneY(b[0]) == 6.);
    CHECK(sceneY(b[2]) == 20.);

    delete grid;
  });
}

TEST_CASE("An aligned grid sizes each column to its widest item", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* grid = new score::GraphicsGridColumnsLayout{nullptr};
    scene.addItem(grid);
    grid->setColumns(2);
    grid->setPadding(4.);

    SECTION("left")
    {
      grid->setAlignment(Qt::AlignLeft);
      const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
      grid->layout();
      // Columns 40 and 30 wide, the padding after each
      CHECK(sceneX(b[0]) == 4.);
      CHECK(sceneX(b[2]) == 4.);
      CHECK(sceneX(b[1]) == 48.);
      CHECK(sceneX(b[3]) == 48.);
      CHECK(sceneY(b[0]) == 6.);
      CHECK(sceneY(b[1]) == 6.);
      CHECK(sceneY(b[2]) == 20.);
    }
    SECTION("right")
    {
      grid->setAlignment(Qt::AlignRight);
      const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
      grid->layout();
      CHECK(sceneX(b[0]) + 40. == 44.);
      CHECK(sceneX(b[2]) + 20. == 44.);
      CHECK(sceneX(b[1]) + 10. == 78.);
      CHECK(sceneX(b[3]) + 30. == 78.);
    }
    SECTION("spacing")
    {
      // Between cells, in place of the padding
      grid->setAlignment(Qt::AlignLeft);
      grid->setSpacing(10.);
      const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
      grid->layout();
      CHECK(sceneX(b[0]) == 4.);
      CHECK(sceneX(b[1]) == 54.);
      CHECK(sceneY(b[0]) == 9.);
      CHECK(sceneY(b[2]) == 29.);
    }
    SECTION("center")
    {
      grid->setAlignment(Qt::AlignHCenter);
      const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
      grid->layout();
      CHECK(sceneX(b[2]) == 14.);
      CHECK(sceneX(b[1]) == 58.);
    }

    delete grid;
  });
}

TEST_CASE("An aligned row-major grid lines up its columns", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* grid = new score::GraphicsGridRowsLayout{nullptr};
    scene.addItem(grid);
    grid->setRows(2);
    grid->setAlignment(Qt::AlignLeft);
    // Filled column by column: 40 and 10, then 20 and 30
    const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
    grid->layout();

    CHECK(sceneX(b[0]) == 0.);
    CHECK(sceneX(b[1]) == 0.);
    CHECK(sceneX(b[2]) == 40.);
    CHECK(sceneX(b[3]) == 40.);
    CHECK(sceneY(b[1]) == 10.);

    delete grid;
  });
}

TEST_CASE("Box layouts align their children in their own width", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;

    SECTION("vbox")
    {
      auto* box = new score::GraphicsVBoxLayout{nullptr};
      scene.addItem(box);
      const auto b = alignmentTestBoxes(box, {40., 10.});
      box->layout();
      // As wide as a parent made it
      box->setRect({0., 0., 100., 20.});
      box->centerContent();
      CHECK(sceneX(b[0]) == 0.);
      CHECK(sceneX(b[1]) == 0.);

      box->setAlignment(Qt::AlignRight);
      box->layout();
      box->centerContent();
      CHECK(sceneX(b[0]) == 60.);
      CHECK(sceneX(b[1]) == 90.);
      CHECK(sceneY(b[1]) == 10.);

      // Idempotent: relayoutAround calls it again
      box->centerContent();
      CHECK(sceneX(b[1]) == 90.);
      delete box;
    }
    SECTION("hbox")
    {
      auto* box = new score::GraphicsHBoxLayout{nullptr};
      scene.addItem(box);
      const auto b = alignmentTestBoxes(box, {20., 30.});
      box->layout();
      box->setRect({0., 0., 100., 10.});
      box->centerContent();
      CHECK(sceneX(b[0]) == 0.);
      CHECK(sceneX(b[1]) == 20.);

      box->setAlignment(Qt::AlignHCenter);
      box->centerContent();
      CHECK(sceneX(b[0]) == 25.);
      CHECK(sceneX(b[1]) == 45.);
      box->centerContent();
      CHECK(sceneX(b[0]) == 25.);
      delete box;
    }
  });
}

TEST_CASE("A table aligns its cells in their column", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* table = new score::GraphicsTableLayout{nullptr};
    scene.addItem(table);
    auto* r0 = new score::GraphicsHBoxLayout{table};
    auto* r1 = new score::GraphicsHBoxLayout{table};
    const auto c0 = alignmentTestBoxes(r0, {10., 30.});
    const auto c1 = alignmentTestBoxes(r1, {20., 5.});

    table->layout();
    // Centred by default
    CHECK(sceneX(c0[0]) == 5.);
    CHECK(sceneX(c1[0]) == 0.);

    table->setAlignment(Qt::AlignLeft);
    table->layout();
    CHECK(sceneX(c0[0]) == 0.);
    CHECK(sceneX(c1[0]) == 0.);
    CHECK(sceneX(c0[1]) == 20.);
    CHECK(sceneX(c1[1]) == 20.);

    delete table;
  });
}

namespace
{
struct alignmentTestPlacement
{
  std::vector<QPointF> positions;
  QRectF extent;
  bool operator==(const alignmentTestPlacement&) const = default;
};

alignmentTestPlacement
alignmentTestPlace(const QGraphicsItem& grid, const std::vector<QGraphicsItem*>& items)
{
  alignmentTestPlacement res;
  for(auto* item : items)
    res.positions.push_back(item->scenePos());
  res.extent = grid.childrenBoundingRect();
  return res;
}
}

TEST_CASE("Laying out a grid again leaves it as it was", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* grid = new score::GraphicsGridColumnsLayout{nullptr};
    scene.addItem(grid);
    grid->setColumns(2);
    grid->setPadding(4.);

    SECTION("aligned, with spacing")
    {
      grid->setAlignment(Qt::AlignLeft);
      grid->setSpacing(6.);
    }
    SECTION("uniform") { }

    const auto b = alignmentTestBoxes(grid, {40., 10., 20., 30.});
    grid->layout();
    const auto first = alignmentTestPlace(*grid, b);

    for(int i = 0; i < 3; i++)
      grid->layout();
    CHECK(alignmentTestPlace(*grid, b) == first);

    // As when an item changes size in the running app
    for(int i = 0; i < 3; i++)
      score::GraphicsLayout::relayoutAround(b[1]);
    CHECK(alignmentTestPlace(*grid, b) == first);

    // An item that grows widens its column, and shrinking undoes it
    static_cast<score::EmptyRectItem*>(b[2])->setRect({0., 0., 50., 10.});
    score::GraphicsLayout::relayoutAround(b[2]);
    CHECK(alignmentTestPlace(*grid, b) != first);
    static_cast<score::EmptyRectItem*>(b[2])->setRect({0., 0., 20., 10.});
    score::GraphicsLayout::relayoutAround(b[2]);
    CHECK(alignmentTestPlace(*grid, b) == first);

    delete grid;
  });
}
