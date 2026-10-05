// The tab and strip / detail layouts own items of their own (the tab bar, the
// strip of cells) next to the pages a UI builder gives them. An avnd UI is
// rebuilt by deleting every child of its root layout and building again into
// the same root, so a root layout must cope with its own items going away.

#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsStripDetailLayout.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/widgets/QGraphicsEnum.hpp>

#include <score_test/App.hpp>

#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>

namespace
{
QGraphicsItem* page(QGraphicsItem* parent)
{
  auto* p = new score::GraphicsVBoxLayout{parent};
  p->setRect({0., 0., 50., 20.});
  return p;
}

std::vector<score::QGraphicsEnum*> tabBars(QGraphicsItem& root)
{
  std::vector<score::QGraphicsEnum*> res;
  for(auto* item : root.childItems())
    if(auto* bar = dynamic_cast<score::QGraphicsEnum*>(item))
      res.push_back(bar);
  return res;
}

void deleteChildren(QGraphicsItem& root)
{
  for(auto* item : root.childItems())
    delete item;
}
}

TEST_CASE("A tab layout survives the deletion of all its children", "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* tabs = new score::GraphicsTabLayout{nullptr};
    scene.addItem(tabs);
    tabs->addTab("A");
    tabs->addTab("B");

    page(tabs);
    page(tabs);
    tabs->layout();
    REQUIRE(tabBars(*tabs).size() == 1);

    tabs->setCurrentIndex(1);
    deleteChildren(*tabs);
    REQUIRE(tabs->childItems().empty());

    // Between the deletion and the next build
    tabs->setCurrentIndex(0);

    auto* a = page(tabs);
    auto* b = page(tabs);
    tabs->layout();

    const auto bars = tabBars(*tabs);
    REQUIRE(bars.size() == 1);
    CHECK(bars.front()->isVisible());
    CHECK(bars.front()->value() == 0);
    CHECK(a->isVisible());
    CHECK(!b->isVisible());

    tabs->setCurrentIndex(1);
    CHECK(!a->isVisible());
    CHECK(b->isVisible());
    CHECK(bars.front()->value() == 1);

    delete tabs;
  });
}

TEST_CASE(
    "A strip / detail layout survives the deletion of all its children",
    "[graphics][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* strip = new score::GraphicsStripDetailLayout{nullptr};
    scene.addItem(strip);

    auto build = [strip] {
      std::vector<QGraphicsItem*> pages;
      for(int i = 0; i < 2; i++)
      {
        strip->addCell(new score::GraphicsStripCell{nullptr});
        pages.push_back(page(strip));
      }
      strip->strip().layout();
      strip->layout();
      return pages;
    };

    build();
    deleteChildren(*strip);
    REQUIRE(strip->childItems().empty());
    strip->setCurrentIndex(0);

    const auto pages = build();
    auto& cells = strip->strip();
    CHECK(cells.parentItem() == strip);
    CHECK(cells.childItems().size() == 2);
    CHECK(pages[0]->isVisible());
    CHECK(!pages[1]->isVisible());

    strip->setCurrentIndex(1);
    CHECK(!pages[0]->isVisible());
    CHECK(pages[1]->isVisible());

    delete strip;
  });
}
