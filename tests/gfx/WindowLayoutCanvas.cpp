// The two editing canvases of the multi-window output device:
//
//   OutputMappingCanvas  — carves the input texture into per-output source
//                          quads (UV space), with soft-edge blend handles,
//                          4-corner warp and per-output lock modes.
//   DesktopLayoutCanvas  — places the resulting output windows on the virtual
//                          desktop, snapping to screen and window borders.
//
// Both are QGraphicsView subclasses whose real logic (coordinate mapping,
// snapping, clamping, lock modes, re-indexing, warp) is pure geometry. Nothing
// here needs a GPU or a visible window: the items are driven by synthesising
// QGraphicsSceneMouseEvent / QGraphicsSceneHoverEvent and handing them to
// QGraphicsScene::sendEvent, which is exactly the path the scene itself uses.

#include <Gfx/Window/DesktopLayout.hpp>
#include <Gfx/Window/OutputMapping.hpp>
#include <Gfx/Window/TestCard.hpp>

#include <score_test/App.hpp>

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QScreen>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

using Catch::Approx;

namespace
{
template <typename Item>
std::vector<Item*> itemsOf(QGraphicsScene& scene)
{
  std::vector<Item*> out;
  for(auto* gi : scene.items())
    if(auto* it = dynamic_cast<Item*>(gi))
      out.push_back(it);
  std::sort(out.begin(), out.end(), [](auto* a, auto* b) {
    return a->outputIndex() < b->outputIndex();
  });
  return out;
}

// Item-local `pos`; `scenePos` defaults to the item-local point offset by the
// item position, which is what the scene would compute.
void pressItem(
    QGraphicsScene& scene, QGraphicsItem* item, QPointF pos,
    Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMousePress};
  ev.setPos(pos);
  ev.setScenePos(item->pos() + pos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
  ev.setModifiers(mods);
  scene.sendEvent(item, &ev);
}

void moveItem(
    QGraphicsScene& scene, QGraphicsItem* item, QPointF pos,
    Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseMove};
  ev.setPos(pos);
  ev.setScenePos(item->pos() + pos);
  ev.setButtons(Qt::LeftButton);
  ev.setModifiers(mods);
  scene.sendEvent(item, &ev);
}

void releaseItem(QGraphicsScene& scene, QGraphicsItem* item, QPointF pos)
{
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseRelease};
  ev.setPos(pos);
  ev.setScenePos(item->pos() + pos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::NoButton);
  scene.sendEvent(item, &ev);
}

Qt::CursorShape hoverCursor(QGraphicsScene& scene, QGraphicsItem* item, QPointF pos)
{
  QGraphicsSceneHoverEvent ev{QEvent::GraphicsSceneHoverMove};
  ev.setPos(pos);
  ev.setScenePos(item->pos() + pos);
  scene.sendEvent(item, &ev);
  return item->cursor().shape();
}

Gfx::OutputMapping uvMapping(QRectF uv)
{
  Gfx::OutputMapping m;
  m.sourceRect = uv;
  return m;
}

QRectF virtualDesktopBounds()
{
  QRect bounds;
  for(auto* s : qApp->screens())
    bounds = bounds.united(s->geometry());
  return QRectF(bounds);
}
}

// -----------------------------------------------------------------------------
// OutputMappingCanvas
// -----------------------------------------------------------------------------

TEST_CASE("OutputMappingCanvas aspect ratio", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;

    // The default canvas is 400x300 (4:3).
    CHECK(canvas.canvasWidth() == Approx(400.0));
    CHECK(canvas.canvasHeight() == Approx(300.0));

    SECTION("landscape pins the width")
    {
      canvas.updateAspectRatio(1920, 1080);
      CHECK(canvas.canvasWidth() == Approx(400.0));
      CHECK(canvas.canvasHeight() == Approx(225.0));
    }

    SECTION("portrait pins the height")
    {
      canvas.updateAspectRatio(1080, 1920);
      CHECK(canvas.canvasHeight() == Approx(400.0));
      CHECK(canvas.canvasWidth() == Approx(225.0));
    }

    SECTION("square")
    {
      canvas.updateAspectRatio(512, 512);
      CHECK(canvas.canvasWidth() == Approx(400.0));
      CHECK(canvas.canvasHeight() == Approx(400.0));
    }

    SECTION("a degenerate input size is ignored")
    {
      canvas.updateAspectRatio(1920, 1080);
      canvas.updateAspectRatio(0, 1080);
      canvas.updateAspectRatio(1920, 0);
      canvas.updateAspectRatio(-4, -4);
      CHECK(canvas.canvasWidth() == Approx(400.0));
      CHECK(canvas.canvasHeight() == Approx(225.0));
    }

    SECTION("existing mappings keep their UV rect across a resize")
    {
      // The whole point of the rescale in updateAspectRatio: the source quads
      // are stored in scene pixels but mean UV, so changing the input aspect
      // must not move them relative to the image.
      canvas.setMappings(
          {uvMapping({0.0, 0.0, 0.5, 0.5}), uvMapping({0.25, 0.5, 0.75, 0.5})});

      canvas.updateAspectRatio(1920, 1080);
      const auto after = canvas.getMappings();
      REQUIRE(after.size() == 2);
      CHECK(after[0].sourceRect.x() == Approx(0.0));
      CHECK(after[0].sourceRect.y() == Approx(0.0));
      CHECK(after[0].sourceRect.width() == Approx(0.5));
      CHECK(after[0].sourceRect.height() == Approx(0.5));
      CHECK(after[1].sourceRect.x() == Approx(0.25));
      CHECK(after[1].sourceRect.y() == Approx(0.5));
      CHECK(after[1].sourceRect.width() == Approx(0.75));
      CHECK(after[1].sourceRect.height() == Approx(0.5));
    }
  });
}

TEST_CASE("OutputMappingCanvas mapping round-trip", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;

    Gfx::OutputMapping a = uvMapping({0.0, 0.0, 0.5, 1.0});
    a.screenIndex = 1;
    a.windowPosition = {100, 200};
    a.windowSize = {800, 600};
    a.fullscreen = true;
    a.blendRight = {0.2f, 1.8f};
    a.cornerWarp.bottomRight = {0.9, 0.95};
    a.lockMode = Gfx::OutputLockMode::AspectRatio;
    a.rotation = 90;
    a.mirrorX = true;

    Gfx::OutputMapping b = uvMapping({0.5, 0.0, 0.5, 1.0});
    b.screenIndex = 2;
    b.lockMode = Gfx::OutputLockMode::FullLock;
    b.mirrorY = true;

    canvas.setMappings({a, b});
    const auto out = canvas.getMappings();

    REQUIRE(out.size() == 2);
    CHECK(out[0].sourceRect.x() == Approx(a.sourceRect.x()));
    CHECK(out[0].sourceRect.width() == Approx(a.sourceRect.width()));
    CHECK(out[0].screenIndex == 1);
    CHECK(out[0].windowPosition == QPoint{100, 200});
    CHECK(out[0].windowSize == QSize{800, 600});
    CHECK(out[0].fullscreen);
    CHECK(out[0].blendRight.width == Approx(0.2f));
    CHECK(out[0].blendRight.gamma == Approx(1.8f));
    CHECK(out[0].cornerWarp.bottomRight == QPointF{0.9, 0.95});
    CHECK(out[0].lockMode == Gfx::OutputLockMode::AspectRatio);
    CHECK(out[0].rotation == 90);
    CHECK(out[0].mirrorX);
    CHECK_FALSE(out[0].mirrorY);

    CHECK(out[1].sourceRect.x() == Approx(0.5));
    CHECK(out[1].screenIndex == 2);
    CHECK(out[1].lockMode == Gfx::OutputLockMode::FullLock);
    CHECK(out[1].mirrorY);

    SECTION("setMappings replaces rather than appends")
    {
      canvas.setMappings({uvMapping({0.0, 0.0, 1.0, 1.0})});
      CHECK(canvas.getMappings().size() == 1);
    }

    SECTION("an empty vector clears the canvas")
    {
      canvas.setMappings({});
      CHECK(canvas.getMappings().empty());
    }
  });
}

TEST_CASE("OutputMappingCanvas add and remove", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;

    int selected = -2;
    canvas.onSelectionChanged = [&](int i) { selected = i; };

    canvas.addOutput();
    canvas.addOutput();
    canvas.addOutput();

    auto items = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());
    REQUIRE(items.size() == 3);
    CHECK(items[0]->outputIndex() == 0);
    CHECK(items[1]->outputIndex() == 1);
    CHECK(items[2]->outputIndex() == 2);
    // addOutput auto-selects the item it just made.
    CHECK(selected == 2);

    SECTION("removing the selected output re-indexes the rest")
    {
      canvas.scene()->clearSelection();
      items[1]->setSelected(true);
      canvas.removeSelectedOutput();

      auto after = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());
      REQUIRE(after.size() == 2);
      CHECK(after[0]->outputIndex() == 0);
      CHECK(after[1]->outputIndex() == 1);
    }

    SECTION("removing with nothing selected is a no-op")
    {
      canvas.scene()->clearSelection();
      canvas.removeSelectedOutput();
      CHECK(itemsOf<Gfx::OutputMappingItem>(*canvas.scene()).size() == 3);
    }

    SECTION("clearing the selection reports -1")
    {
      canvas.scene()->clearSelection();
      CHECK(selected == -1);
    }
  });
}

TEST_CASE("OutputMappingCanvas snapping and clamping", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setMappings(
        {uvMapping({0.0, 0.0, 0.25, 0.25}), uvMapping({0.5, 0.5, 0.25, 0.25})});
    auto items = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());
    REQUIRE(items.size() == 2);
    auto* a = items[0];
    auto* b = items[1];

    SECTION("snapping is on by default and pulls to a neighbour's edges")
    {
      CHECK(canvas.snapEnabled());
      // b occupies scene (200,150,100,75); land 5/2 px short of its corner.
      a->setPos(195, 148);
      CHECK(a->pos().x() == Approx(200.0));
      CHECK(a->pos().y() == Approx(150.0));
    }

    SECTION("a zero-distance edge is returned unchanged")
    {
      const auto target = b->mapRectToScene(b->rect()).topLeft();
      CHECK(canvas.snapPosition(a, target) == target);
    }

    SECTION("beyond the threshold nothing is snapped")
    {
      const QPointF target{140.0, 100.0};
      CHECK(canvas.snapPosition(a, target) == target);
    }

    SECTION("snapping can be turned off")
    {
      canvas.setSnapEnabled(false);
      CHECK_FALSE(canvas.snapEnabled());
      a->setPos(195, 148);
      CHECK(a->pos().x() == Approx(195.0));
      CHECK(a->pos().y() == Approx(148.0));
    }

    SECTION("items are clamped to the canvas, not to the scene rect")
    {
      // The scene rect carries a 30px margin for the warp handles; a source
      // quad that escaped into it would sample outside the input texture.
      canvas.setSnapEnabled(false);
      a->setPos(5000, 5000);
      CHECK(a->pos().x() == Approx(canvas.canvasWidth() - a->rect().width()));
      CHECK(a->pos().y() == Approx(canvas.canvasHeight() - a->rect().height()));

      a->setPos(-5000, -5000);
      CHECK(a->pos().x() == Approx(0.0));
      CHECK(a->pos().y() == Approx(0.0));
    }
  });
}

TEST_CASE("OutputMappingItem edge resize", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setSnapEnabled(false);
    canvas.setMappings({uvMapping({0.0, 0.0, 1.0, 1.0})});
    auto items = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());
    REQUIRE(items.size() == 1);
    auto* item = items[0];
    auto& scene = *canvas.scene();
    REQUIRE(item->rect() == QRectF(0, 0, 400, 300));

    int changes = 0;
    canvas.onItemGeometryChanged = [&](int) { ++changes; };

    SECTION("dragging the left edge inwards")
    {
      pressItem(scene, item, {3, 150});
      moveItem(scene, item, {53, 150});
      releaseItem(scene, item, {53, 150});
      CHECK(item->rect().left() == Approx(50.0));
      CHECK(item->rect().width() == Approx(350.0));
      CHECK(changes > 0);
    }

    SECTION("the left edge stops at the minimum width")
    {
      pressItem(scene, item, {3, 150});
      moveItem(scene, item, {5000, 150});
      CHECK(item->rect().width() == Approx(10.0));
    }

    SECTION("the left edge stops at the scene bound")
    {
      pressItem(scene, item, {3, 150});
      moveItem(scene, item, {-5000, 150});
      CHECK(item->rect().left() == Approx(canvas.scene()->sceneRect().left()));
    }

    SECTION("ctrl scales the drag down by ten")
    {
      pressItem(scene, item, {3, 150}, Qt::ControlModifier);
      moveItem(scene, item, {103, 150}, Qt::ControlModifier);
      CHECK(item->rect().left() == Approx(10.0));
    }

    SECTION("dragging the bottom-right corner moves both edges")
    {
      pressItem(scene, item, {397, 297});
      moveItem(scene, item, {347, 247});
      CHECK(item->rect().right() == Approx(350.0));
      CHECK(item->rect().bottom() == Approx(250.0));
    }

    SECTION("a release ends the resize")
    {
      pressItem(scene, item, {3, 150});
      moveItem(scene, item, {53, 150});
      releaseItem(scene, item, {53, 150});
      const auto after = item->rect();
      // No press: this move is a plain (unstarted) drag and must not resize.
      moveItem(scene, item, {200, 150});
      CHECK(item->rect() == after);
    }

    SECTION("the interior is a move, not a resize")
    {
      const auto before = item->rect();
      pressItem(scene, item, {200, 150});
      moveItem(scene, item, {210, 160});
      CHECK(item->rect() == before);
    }
  });
}

TEST_CASE("OutputMappingItem blend handles", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setSnapEnabled(false);
    canvas.setMappings({uvMapping({0.0, 0.0, 1.0, 1.0})});
    auto items = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());
    REQUIRE(items.size() == 1);
    auto* item = items[0];
    auto& scene = *canvas.scene();

    // At width 0 the handles sit at a fixed 10px inset so they stay grabbable
    // without colliding with the 6px resize margin.
    SECTION("left")
    {
      pressItem(scene, item, {10, 150});
      moveItem(scene, item, {100, 150});
      CHECK(item->blendLeft.width == Approx(0.25f));
      CHECK(item->rect() == QRectF(0, 0, 400, 300));
    }

    SECTION("right")
    {
      pressItem(scene, item, {390, 150});
      moveItem(scene, item, {300, 150});
      CHECK(item->blendRight.width == Approx(0.25f));
    }

    SECTION("top")
    {
      pressItem(scene, item, {200, 10});
      moveItem(scene, item, {200, 90});
      CHECK(item->blendTop.width == Approx(0.3f));
    }

    SECTION("bottom")
    {
      pressItem(scene, item, {200, 290});
      moveItem(scene, item, {200, 210});
      CHECK(item->blendBottom.width == Approx(0.3f));
    }

    SECTION("the blend width is clamped to half the output")
    {
      pressItem(scene, item, {10, 150});
      moveItem(scene, item, {390, 150});
      CHECK(item->blendLeft.width == Approx(0.5f));
      releaseItem(scene, item, {390, 150});

      // The handle tracks the blend width, so it now sits at 0.5 * 400.
      pressItem(scene, item, {200, 150});
      moveItem(scene, item, {-400, 150});
      CHECK(item->blendLeft.width == Approx(0.0f));
    }

    SECTION("ctrl scales the blend drag down by ten")
    {
      pressItem(scene, item, {10, 150}, Qt::ControlModifier);
      moveItem(scene, item, {410, 150}, Qt::ControlModifier);
      // 10 + (410-10)*0.1 = 50 -> 50/400
      CHECK(item->blendLeft.width == Approx(0.125f));
    }

    SECTION("a release ends the blend drag")
    {
      pressItem(scene, item, {10, 150});
      moveItem(scene, item, {100, 150});
      releaseItem(scene, item, {100, 150});
      moveItem(scene, item, {200, 150});
      CHECK(item->blendLeft.width == Approx(0.25f));
    }
  });
}

TEST_CASE("OutputMappingItem lock modes", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setSnapEnabled(false);

    SECTION("FullLock refuses both move and resize")
    {
      auto m = uvMapping({0.25, 0.25, 0.5, 0.5});
      m.lockMode = Gfx::OutputLockMode::FullLock;
      canvas.setMappings({m});
      auto* item = itemsOf<Gfx::OutputMappingItem>(*canvas.scene()).front();
      auto& scene = *canvas.scene();

      CHECK_FALSE(item->flags().testFlag(QGraphicsItem::ItemIsMovable));
      CHECK(item->flags().testFlag(QGraphicsItem::ItemIsSelectable));
      CHECK_FALSE(item->acceptHoverEvents());

      const auto rect = item->rect();
      const auto pos = item->pos();
      pressItem(scene, item, {3, 10});
      moveItem(scene, item, {80, 80});
      CHECK(item->rect() == rect);
      CHECK(item->pos() == pos);

      // Same contract as DesktopLayoutItem: FullLock never advertises a
      // resize cursor its press handler refuses to act on.
      CHECK(hoverCursor(scene, item, {102, 77}) == Qt::ArrowCursor);
    }

    SECTION("Free is movable and hoverable")
    {
      auto m = uvMapping({0.25, 0.25, 0.5, 0.5});
      canvas.setMappings({m});
      auto* item = itemsOf<Gfx::OutputMappingItem>(*canvas.scene()).front();
      CHECK(item->flags().testFlag(QGraphicsItem::ItemIsMovable));
      CHECK(item->acceptHoverEvents());
    }
  });
}

TEST_CASE("OutputMappingItem hover cursors", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setMappings({uvMapping({0.0, 0.0, 1.0, 1.0})});
    auto* item = itemsOf<Gfx::OutputMappingItem>(*canvas.scene()).front();
    auto& scene = *canvas.scene();

    // The blend handles are tested before the resize edges, so the 10px inset
    // rows/columns win over the 6px edge margin.
    CHECK(hoverCursor(scene, item, {10, 150}) == Qt::SplitHCursor);
    CHECK(hoverCursor(scene, item, {390, 150}) == Qt::SplitHCursor);
    CHECK(hoverCursor(scene, item, {200, 10}) == Qt::SplitVCursor);
    CHECK(hoverCursor(scene, item, {200, 290}) == Qt::SplitVCursor);

    CHECK(hoverCursor(scene, item, {2, 2}) == Qt::SizeFDiagCursor);
    CHECK(hoverCursor(scene, item, {398, 298}) == Qt::SizeFDiagCursor);
    CHECK(hoverCursor(scene, item, {2, 298}) == Qt::SizeBDiagCursor);
    CHECK(hoverCursor(scene, item, {398, 2}) == Qt::SizeBDiagCursor);
    CHECK(hoverCursor(scene, item, {2, 150}) == Qt::SizeHorCursor);
    CHECK(hoverCursor(scene, item, {398, 150}) == Qt::SizeHorCursor);
    CHECK(hoverCursor(scene, item, {200, 2}) == Qt::SizeVerCursor);
    CHECK(hoverCursor(scene, item, {200, 298}) == Qt::SizeVerCursor);
    CHECK(hoverCursor(scene, item, {200, 150}) == Qt::ArrowCursor);
  });
}

TEST_CASE("OutputMappingCanvas warp mode", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setSnapEnabled(false);
    canvas.setMappings(
        {uvMapping({0.25, 0.25, 0.5, 0.5}), uvMapping({0.0, 0.0, 0.2, 0.2})});
    auto items = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());
    REQUIRE(items.size() == 2);

    CHECK_FALSE(canvas.inWarpMode());

    SECTION("entering disables editing, leaving restores it")
    {
      const int before = canvas.scene()->items().size();
      canvas.enterWarpMode(0);
      CHECK(canvas.inWarpMode());
      // 4 handles + the quad + the 4x4 subdivision grid.
      CHECK(canvas.scene()->items().size() > before);
      for(auto* it : items)
      {
        CHECK_FALSE(it->flags().testFlag(QGraphicsItem::ItemIsMovable));
        CHECK_FALSE(it->acceptHoverEvents());
      }

      canvas.exitWarpMode();
      CHECK_FALSE(canvas.inWarpMode());
      CHECK(canvas.scene()->items().size() == before);
      for(auto* it : items)
        CHECK(it->flags().testFlag(QGraphicsItem::ItemIsMovable));
    }

    SECTION("re-entering on the same output toggles back out")
    {
      canvas.enterWarpMode(1);
      CHECK(canvas.inWarpMode());
      canvas.enterWarpMode(1);
      CHECK_FALSE(canvas.inWarpMode());
    }

    SECTION("switching output stays in warp mode")
    {
      canvas.enterWarpMode(0);
      canvas.enterWarpMode(1);
      CHECK(canvas.inWarpMode());
    }

    SECTION("an unknown output index does not enter")
    {
      canvas.enterWarpMode(7);
      CHECK_FALSE(canvas.inWarpMode());
    }

    SECTION("exiting when not in warp mode is a no-op")
    {
      const int before = canvas.scene()->items().size();
      canvas.exitWarpMode();
      CHECK_FALSE(canvas.inWarpMode());
      CHECK(canvas.scene()->items().size() == before);
    }

    SECTION("escape leaves warp mode")
    {
      canvas.enterWarpMode(0);
      QKeyEvent ev{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
      QApplication::sendEvent(&canvas, &ev);
      CHECK_FALSE(canvas.inWarpMode());
    }

    SECTION("another key is passed through")
    {
      canvas.enterWarpMode(0);
      QKeyEvent ev{QEvent::KeyPress, Qt::Key_A, Qt::NoModifier};
      QApplication::sendEvent(&canvas, &ev);
      CHECK(canvas.inWarpMode());
    }
  });
}

TEST_CASE("OutputMappingCanvas warp reset", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setSnapEnabled(false);

    auto warped = uvMapping({0.25, 0.25, 0.5, 0.5});
    warped.cornerWarp.topLeft = {0.2, 0.3};
    canvas.setMappings({warped, uvMapping({0.0, 0.0, 0.2, 0.2})});
    auto items = itemsOf<Gfx::OutputMappingItem>(*canvas.scene());

    int notified = 0;
    canvas.onWarpChanged = [&] { ++notified; };

    SECTION("resets the output being warped")
    {
      canvas.enterWarpMode(0);
      canvas.resetWarp();
      CHECK(items[0]->cornerWarp.isIdentity());
      CHECK(notified == 1);
    }

    SECTION("outside warp mode it resets the selected output")
    {
      canvas.scene()->clearSelection();
      items[0]->setSelected(true);
      canvas.resetWarp();
      CHECK(items[0]->cornerWarp.isIdentity());
      CHECK(notified == 1);
    }

    SECTION("with no warp mode and no selection it does nothing")
    {
      canvas.scene()->clearSelection();
      canvas.resetWarp();
      CHECK_FALSE(items[0]->cornerWarp.isIdentity());
      CHECK(notified == 0);
    }
  });
}

TEST_CASE("OutputMappingCanvas warp handle drag", "[gfx][window][mappingcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::OutputMappingCanvas canvas;
    canvas.setSnapEnabled(false);
    canvas.setMappings({uvMapping({0.25, 0.25, 0.5, 0.5})});
    auto* item = itemsOf<Gfx::OutputMappingItem>(*canvas.scene()).front();

    // A 1:1 view transform keeps scene<->viewport mapping within a pixel, which
    // the 10-unit handle pick radius absorbs.
    canvas.resize(1000, 800);
    QApplication::processEvents();
    canvas.resetTransform();

    const QRectF r = item->mapRectToScene(item->rect());
    REQUIRE(r == QRectF(100, 75, 200, 150));

    int notified = 0;
    canvas.onWarpChanged = [&] { ++notified; };

    canvas.enterWarpMode(0);
    REQUIRE(canvas.inWarpMode());

    auto sendView = [&](QEvent::Type type, QPointF scenePos, Qt::MouseButton btn,
                        Qt::MouseButtons btns) {
      const QPointF vp = canvas.mapFromScene(scenePos);
      QMouseEvent ev{type, vp, canvas.viewport()->mapToGlobal(vp),
                     btn,  btns, Qt::NoModifier};
      QApplication::sendEvent(canvas.viewport(), &ev);
    };

    SECTION("dragging the top-left handle writes a UV corner")
    {
      sendView(
          QEvent::MouseButtonPress, r.topLeft(), Qt::LeftButton, Qt::LeftButton);
      sendView(
          QEvent::MouseMove, r.topLeft() + QPointF(50, 30), Qt::NoButton,
          Qt::LeftButton);
      CHECK(item->cornerWarp.topLeft.x() == Approx(0.25).margin(0.02));
      CHECK(item->cornerWarp.topLeft.y() == Approx(0.2).margin(0.02));
      CHECK(notified > 0);

      sendView(
          QEvent::MouseButtonRelease, r.topLeft() + QPointF(50, 30), Qt::LeftButton,
          Qt::NoButton);
      const auto after = item->cornerWarp.topLeft;
      sendView(QEvent::MouseMove, r.center(), Qt::NoButton, Qt::NoButton);
      CHECK(item->cornerWarp.topLeft == after);
    }

    SECTION("pressing away from any handle starts no drag")
    {
      sendView(QEvent::MouseButtonPress, r.center(), Qt::LeftButton, Qt::LeftButton);
      sendView(QEvent::MouseMove, r.center() + QPointF(40, 40), Qt::NoButton,
               Qt::LeftButton);
      CHECK(item->cornerWarp.isIdentity());
      CHECK(notified == 0);
    }

    SECTION("each of the four handles maps to its own corner")
    {
      const QPointF handles[4]
          = {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()};
      const QPointF* corners[4]
          = {&item->cornerWarp.topLeft, &item->cornerWarp.topRight,
             &item->cornerWarp.bottomLeft, &item->cornerWarp.bottomRight};
      for(int i = 0; i < 4; i++)
      {
        canvas.resetWarp();
        sendView(
            QEvent::MouseButtonPress, handles[i], Qt::LeftButton, Qt::LeftButton);
        sendView(
            QEvent::MouseMove, handles[i] + QPointF(20, 0), Qt::NoButton,
            Qt::LeftButton);
        sendView(
            QEvent::MouseButtonRelease, handles[i] + QPointF(20, 0), Qt::LeftButton,
            Qt::NoButton);

        // Only the dragged corner moved, and it moved by +20/200 in u.
        for(int j = 0; j < 4; j++)
        {
          const double expectedU
              = (j == i) ? (handles[j].x() - r.left()) / r.width() + 0.1
                         : (handles[j].x() - r.left()) / r.width();
          CHECK(corners[j]->x() == Approx(expectedU).margin(0.02));
        }
      }
    }
  });
}

// -----------------------------------------------------------------------------
// DesktopLayoutCanvas
// -----------------------------------------------------------------------------

TEST_CASE("DesktopLayoutCanvas coordinate mapping", "[gfx][window][desktopcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::DesktopLayoutCanvas canvas;
    const auto bounds = virtualDesktopBounds();
    REQUIRE(bounds.width() > 0);

    SECTION("desktop and scene coordinates are inverses")
    {
      for(QPointF p :
          {bounds.topLeft(), bounds.center(), bounds.bottomRight(),
           bounds.topLeft() + QPointF(37, 11)})
      {
        const auto rt = canvas.sceneToDesktop(canvas.desktopToScene(p));
        CHECK(rt.x() == Approx(p.x()).margin(1e-6));
        CHECK(rt.y() == Approx(p.y()).margin(1e-6));
      }
    }

    SECTION("sizes round-trip to within the integer truncation")
    {
      for(QSize s : {QSize{1920, 1080}, QSize{640, 480}, QSize{3840, 2160}})
      {
        const auto rt = canvas.sceneSizeToDesktop(canvas.desktopSizeToScene(s));
        CHECK(std::abs(rt.width() - s.width()) <= 1);
        CHECK(std::abs(rt.height() - s.height()) <= 1);
      }
    }

    SECTION("a degenerate scene size still yields at least one pixel")
    {
      const auto s = canvas.sceneSizeToDesktop(QSizeF{0.0, 0.0});
      CHECK(s.width() == 1);
      CHECK(s.height() == 1);
    }

    SECTION("the mapping is monotonic")
    {
      const auto a = canvas.desktopToScene(bounds.topLeft());
      const auto b = canvas.desktopToScene(bounds.bottomRight());
      CHECK(a.x() < b.x());
      CHECK(a.y() < b.y());
    }
  });
}

TEST_CASE("DesktopLayoutCanvas screen detection", "[gfx][window][desktopcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::DesktopLayoutCanvas canvas;
    const auto screens = qApp->screens();
    REQUIRE(!screens.isEmpty());

    SECTION("a window inside a screen reports that screen")
    {
      for(int i = 0; i < screens.size(); i++)
      {
        const auto g = screens[i]->geometry();
        const QRect w{g.center() - QPoint{10, 10}, QSize{20, 20}};
        CHECK(canvas.detectScreen(w) == i);
      }
    }

    SECTION("a window off the virtual desktop reports none")
    {
      const auto bounds = virtualDesktopBounds().toRect();
      CHECK(
          canvas.detectScreen(
              QRect{bounds.right() + 10000, bounds.bottom() + 10000, 100, 100})
          == -1);
    }

    SECTION("an empty intersection is not a match")
    {
      const auto g = screens.front()->geometry();
      // Zero-area rect on the screen: intersected() is empty, so no screen wins.
      CHECK(canvas.detectScreen(QRect{g.center(), QSize{0, 0}}) == -1);
    }

    SECTION("the largest overlap wins")
    {
      if(screens.size() < 2)
        SKIP("needs at least two screens");

      const auto a = screens[0]->geometry();
      const auto b = screens[1]->geometry();
      // Mostly on screen 1: take all of b, and a 2px sliver of a.
      QRect w = b;
      CHECK(canvas.detectScreen(w) == 1);
      w = a;
      CHECK(canvas.detectScreen(w) == 0);
    }
  });
}

TEST_CASE("DesktopLayoutCanvas window items", "[gfx][window][desktopcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::DesktopLayoutCanvas canvas;
    canvas.setSnapEnabled(false);
    const auto bounds = virtualDesktopBounds();
    const QPoint origin = bounds.topLeft().toPoint();

    Gfx::OutputMapping a;
    a.windowPosition = origin;
    a.windowSize = {640, 480};
    a.sourceRect = {0.0, 0.0, 0.5, 0.5};

    Gfx::OutputMapping b;
    b.windowPosition = origin + QPoint{700, 100};
    b.windowSize = {320, 240};
    b.lockMode = Gfx::OutputLockMode::AspectRatio;
    b.sourceRect = {0.0, 0.0, 1.0, 0.5};

    canvas.setWindowItems({a, b});
    auto items = itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene());
    REQUIRE(items.size() == 2);
    CHECK(items[0]->outputIndex() == 0);
    CHECK(items[1]->outputIndex() == 1);

    SECTION("the aspect ratio comes from the source rect")
    {
      // sourceRect 1.0 x 0.5 -> 2:1
      CHECK(items[1]->aspectRatio == Approx(2.0));
      CHECK(items[1]->lockMode == Gfx::OutputLockMode::AspectRatio);
    }

    SECTION("scene geometry reflects the desktop geometry")
    {
      const auto expectedPos = canvas.desktopToScene(QPointF(a.windowPosition));
      CHECK(items[0]->pos().x() == Approx(expectedPos.x()));
      CHECK(items[0]->pos().y() == Approx(expectedPos.y()));
      const auto expectedSize = canvas.desktopSizeToScene(a.windowSize);
      CHECK(items[0]->rect().width() == Approx(expectedSize.width()));
      CHECK(items[0]->rect().height() == Approx(expectedSize.height()));
    }

    SECTION("setWindowItems replaces the previous set")
    {
      canvas.setWindowItems({a});
      CHECK(itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene()).size() == 1);
    }

    SECTION("addOutput appends and selects")
    {
      int selected = -2;
      canvas.onSelectionChanged = [&](int i) { selected = i; };
      canvas.addOutput(origin + QPoint{200, 200}, QSize{800, 600});
      auto after = itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene());
      REQUIRE(after.size() == 3);
      CHECK(after[2]->outputIndex() == 2);
      CHECK(selected == 2);
    }

    SECTION("removeOutput re-indexes the remainder")
    {
      canvas.removeOutput(0);
      auto after = itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene());
      REQUIRE(after.size() == 1);
      CHECK(after[0]->outputIndex() == 0);
    }

    SECTION("removing an unknown index removes nothing")
    {
      canvas.removeOutput(42);
      CHECK(itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene()).size() == 2);
    }

    SECTION("updateItem repositions without re-entering the change callback")
    {
      // The callback is deliberately detached during the update: it is the
      // widget writing back into the canvas, and a notification here would
      // bounce straight back out.
      int changes = 0;
      canvas.onItemGeometryChanged = [&](int) { ++changes; };

      const QPoint newPos = origin + QPoint{1000, 400};
      canvas.updateItem(0, newPos, QSize{1280, 720});
      CHECK(changes == 0);

      const auto expected = canvas.desktopToScene(QPointF(newPos));
      CHECK(items[0]->pos().x() == Approx(expected.x()));
      CHECK(items[0]->pos().y() == Approx(expected.y()));
    }

    SECTION("updateItem on an unknown index is a no-op")
    {
      const auto before = items[0]->pos();
      canvas.updateItem(42, origin, QSize{100, 100});
      CHECK(items[0]->pos() == before);
    }

    SECTION("selectItem selects exactly one")
    {
      int selected = -2;
      canvas.onSelectionChanged = [&](int i) { selected = i; };
      canvas.selectItem(1);
      CHECK(selected == 1);
      CHECK(items[1]->isSelected());
      CHECK_FALSE(items[0]->isSelected());
    }

    SECTION("selectItem on an unknown index clears the selection")
    {
      canvas.selectItem(0);
      canvas.selectItem(42);
      CHECK_FALSE(items[0]->isSelected());
    }
  });
}

TEST_CASE("DesktopLayoutCanvas snapping", "[gfx][window][desktopcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::DesktopLayoutCanvas canvas;
    const auto bounds = virtualDesktopBounds();
    const QPoint origin = bounds.topLeft().toPoint();

    Gfx::OutputMapping a, b;
    a.windowPosition = origin + QPoint{100, 100};
    a.windowSize = {320, 240};
    b.windowPosition = origin + QPoint{900, 500};
    b.windowSize = {320, 240};
    canvas.setWindowItems({a, b});

    auto items = itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene());
    REQUIRE(items.size() == 2);

    SECTION("a zero-distance edge is returned unchanged")
    {
      CHECK(canvas.snapPosition(items[0], items[1]->pos()) == items[1]->pos());
    }

    SECTION("snapping is on by default and moves the item")
    {
      CHECK(canvas.snapEnabled());
      const QPointF target = items[1]->pos() + QPointF{2.0, 2.0};
      CHECK(canvas.snapPosition(items[0], target) != target);
    }

    SECTION("with snapping off the requested position is kept exactly")
    {
      canvas.setSnapEnabled(false);
      const QPointF target = items[1]->pos() + QPointF{2.0, 2.0};
      items[0]->setPos(target);
      CHECK(items[0]->pos() == target);
    }
  });
}

TEST_CASE("DesktopLayoutItem lock modes", "[gfx][window][desktopcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::DesktopLayoutCanvas canvas;
    canvas.setSnapEnabled(false);
    auto& scene = *canvas.scene();
    const QPoint origin = virtualDesktopBounds().topLeft().toPoint();

    auto makeItem = [&](Gfx::OutputLockMode mode, QRectF sourceRect) {
      Gfx::OutputMapping m;
      m.windowPosition = origin + QPoint{200, 200};
      m.windowSize = {640, 480};
      m.lockMode = mode;
      m.sourceRect = sourceRect;
      canvas.setWindowItems({m});
      return itemsOf<Gfx::DesktopLayoutItem>(scene).front();
    };

    SECTION("FullLock refuses move and resize and shows no resize cursor")
    {
      auto* item = makeItem(Gfx::OutputLockMode::FullLock, {0, 0, 1, 1});
      CHECK_FALSE(item->flags().testFlag(QGraphicsItem::ItemIsMovable));
      CHECK_FALSE(item->acceptHoverEvents());

      const auto rect = item->rect();
      const auto pos = item->pos();
      pressItem(scene, item, {2, 2});
      moveItem(scene, item, {60, 60});
      CHECK(item->rect() == rect);
      CHECK(item->pos() == pos);
      CHECK(hoverCursor(scene, item, {2, 2}) == Qt::ArrowCursor);
    }

    SECTION("OneToOne moves but never resizes")
    {
      auto* item = makeItem(Gfx::OutputLockMode::OneToOne, {0, 0, 1, 1});
      const auto rect = item->rect();
      const auto pos = item->pos();
      // Press right on the edge: a Free item would start a resize here.
      pressItem(scene, item, {2, 2});
      moveItem(scene, item, {40, 40});
      CHECK(item->rect() == rect);
      CHECK(item->pos() != pos);
      CHECK(hoverCursor(scene, item, {2, 2}) == Qt::SizeAllCursor);
    }

    SECTION("AspectRatio derives the height from the width while resizing")
    {
      auto* item = makeItem(Gfx::OutputLockMode::AspectRatio, {0, 0, 1.0, 0.5});
      REQUIRE(item->aspectRatio == Approx(2.0));
      const double w0 = item->rect().width();

      pressItem(scene, item, {w0 - 2, item->rect().height() / 2});
      moveItem(scene, item, {w0 + 48, item->rect().height() / 2});

      CHECK(item->rect().width() == Approx(w0 + 50));
      CHECK(item->rect().height() == Approx((w0 + 50) / 2.0));
    }

    SECTION("Free resizes freely and keeps its height")
    {
      auto* item = makeItem(Gfx::OutputLockMode::Free, {0, 0, 1.0, 0.5});
      const double w0 = item->rect().width();
      const double h0 = item->rect().height();

      pressItem(scene, item, {w0 - 2, h0 / 2});
      moveItem(scene, item, {w0 + 48, h0 / 2});

      CHECK(item->rect().width() == Approx(w0 + 50));
      CHECK(item->rect().height() == Approx(h0));
    }

    SECTION("the minimum size is enforced on every edge")
    {
      auto* item = makeItem(Gfx::OutputLockMode::Free, {0, 0, 1, 1});
      const auto r0 = item->rect();
      pressItem(scene, item, {2, r0.height() / 2});
      moveItem(scene, item, {r0.width() + 500, r0.height() / 2});
      CHECK(item->rect().width() == Approx(5.0));

      auto* item2 = makeItem(Gfx::OutputLockMode::Free, {0, 0, 1, 1});
      const auto r1 = item2->rect();
      pressItem(scene, item2, {r1.width() / 2, 2});
      moveItem(scene, item2, {r1.width() / 2, r1.height() + 500});
      CHECK(item2->rect().height() == Approx(5.0));
    }
  });
}

TEST_CASE("DesktopLayoutItem hover cursors", "[gfx][window][desktopcanvas]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Gfx::DesktopLayoutCanvas canvas;
    Gfx::OutputMapping m;
    m.windowPosition = virtualDesktopBounds().topLeft().toPoint() + QPoint{100, 100};
    m.windowSize = {1280, 720};
    canvas.setWindowItems({m});
    auto* item = itemsOf<Gfx::DesktopLayoutItem>(*canvas.scene()).front();
    auto& scene = *canvas.scene();
    const auto r = item->rect();
    REQUIRE(r.width() > 20.0);
    REQUIRE(r.height() > 20.0);

    CHECK(hoverCursor(scene, item, {2, 2}) == Qt::SizeFDiagCursor);
    CHECK(hoverCursor(scene, item, {r.width() - 2, r.height() - 2})
          == Qt::SizeFDiagCursor);
    CHECK(hoverCursor(scene, item, {2, r.height() - 2}) == Qt::SizeBDiagCursor);
    CHECK(hoverCursor(scene, item, {r.width() - 2, 2}) == Qt::SizeBDiagCursor);
    CHECK(hoverCursor(scene, item, {2, r.height() / 2}) == Qt::SizeHorCursor);
    CHECK(hoverCursor(scene, item, {r.width() / 2, 2}) == Qt::SizeVerCursor);
    CHECK(hoverCursor(scene, item, {r.width() / 2, r.height() / 2})
          == Qt::SizeAllCursor);
  });
}

// -----------------------------------------------------------------------------
// TestCard
// -----------------------------------------------------------------------------

TEST_CASE("Test card rendering", "[gfx][window][testcard]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    SECTION("a degenerate size yields a null image")
    {
      CHECK(Gfx::renderTestCard(0, 100).isNull());
      CHECK(Gfx::renderTestCard(100, 0).isNull());
      CHECK(Gfx::renderTestCard(-1, -1).isNull());
    }

    SECTION("the requested geometry and format are honoured")
    {
      const auto img = Gfx::renderTestCard(640, 480);
      REQUIRE_FALSE(img.isNull());
      CHECK(img.width() == 640);
      CHECK(img.height() == 480);
      CHECK(img.format() == QImage::Format_RGB32);
    }

    SECTION("every layer actually painted")
    {
      const auto img = Gfx::renderTestCard(640, 480);
      REQUIRE_FALSE(img.isNull());

      std::set<QRgb> colors;
      for(int y = 0; y < img.height(); y += 2)
        for(int x = 0; x < img.width(); x += 2)
          colors.insert(img.pixel(x, y));

      // The grey ramps alone contribute a long tail of distinct values; a card
      // that stopped at the initial fill would report one.
      CHECK(colors.size() > 50);
      CHECK(colors.count(qRgb(0, 0, 0)) <= 1);

      // The rainbow strip sits at 0.62 * h and spans the middle half. Every
      // other layer that reaches this row is greyscale (checkerboard, ramps,
      // circles), so saturation identifies the strip unambiguously -- a plain
      // distinct-colour count is satisfied by the antialiased grid labels alone.
      const int stripY = int(480 * 0.62) + qMax(8, int(480 * 0.04)) / 2;
      int saturated = 0;
      std::set<QRgb> hues;
      for(int x = 640 / 4; x < 3 * 640 / 4; x++)
      {
        const QRgb c = img.pixel(x, stripY);
        const int mx = std::max({qRed(c), qGreen(c), qBlue(c)});
        const int mn = std::min({qRed(c), qGreen(c), qBlue(c)});
        if(mx - mn > 60)
        {
          ++saturated;
          hues.insert(c);
        }
      }
      CHECK(saturated > 200);
      CHECK(hues.size() >= 10);
    }

    SECTION("rendering is deterministic")
    {
      const auto a = Gfx::renderTestCard(320, 240);
      const auto b = Gfx::renderTestCard(320, 240);
      REQUIRE_FALSE(a.isNull());
      CHECK(a == b);
    }

    SECTION("degenerate but legal geometries do not fall over")
    {
      // Every layer sizes itself off w/h with qMax floors; these are the sizes
      // that exercise them.
      for(QSize s : {QSize{1, 1}, QSize{3, 2}, QSize{2000, 10}, QSize{10, 2000}})
      {
        const auto img = Gfx::renderTestCard(s.width(), s.height());
        INFO(s.width() << "x" << s.height());
        REQUIRE_FALSE(img.isNull());
        CHECK(img.size() == s);
      }
    }
  });
}
