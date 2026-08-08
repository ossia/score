#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsNoteChooser.hpp>

#include <score_test/App.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_all.hpp>

namespace
{
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

void setPositions(
    QGraphicsSceneMouseEvent& ev, QPoint screen, QPoint lastScreen, QPoint down)
{
  ev.setScreenPos(screen);
  ev.setLastScreenPos(lastScreen);
  ev.setButtonDownScreenPos(Qt::LeftButton, down);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
}

QPoint pressPoint()
{
  return score::InfiniteScroller::currentGeometry.center().toPoint();
}

void press(Scene& scene, score::QGraphicsNoteChooser& item, QPoint at)
{
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMousePress};
  setPositions(ev, at, at, at);
  ev.setPos(QPointF{1., 1.});
  scene.sendEvent(&item, &ev);
}

void move(Scene& scene, score::QGraphicsNoteChooser& item, QPoint from, QPoint to)
{
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseMove};
  setPositions(ev, to, from, from);
  scene.sendEvent(&item, &ev);
}

void release(Scene& scene, score::QGraphicsNoteChooser& item, QPoint at)
{
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseRelease};
  setPositions(ev, at, at, at);
  scene.sendEvent(&item, &ev);
}
}

TEST_CASE("note chooser follows the drag instead of warping the cursor")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsNoteChooser item{nullptr};
    scene.addItem(&item);
    item.setValue(64);

    score::InfiniteScroller::cancel();

    press(scene, item, QPoint{500, 500});
    const QPoint p0 = pressPoint();

    // Ten pixels of upwards travel is worth one note.
    move(scene, item, p0, p0 - QPoint{0, 20});
    REQUIRE(item.value() == 66);

    move(scene, item, p0 - QPoint{0, 20}, p0 - QPoint{0, 50});
    REQUIRE(item.value() == 69);

    move(scene, item, p0 - QPoint{0, 50}, p0 - QPoint{0, 10});
    REQUIRE(item.value() == 65);

    release(scene, item, p0 - QPoint{0, 10});
    REQUIRE(item.value() == 65);

    scene.removeItem(&item);
  });
}

TEST_CASE("note chooser keeps the dragged value across the release")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsNoteChooser item{nullptr};
    scene.addItem(&item);
    item.setValue(30);

    score::InfiniteScroller::cancel();

    press(scene, item, QPoint{500, 500});
    const QPoint p0 = pressPoint();

    move(scene, item, p0, p0 + QPoint{0, 100});
    REQUIRE(item.value() == 20);

    release(scene, item, p0 + QPoint{0, 100});
    REQUIRE(item.value() == 20);

    scene.removeItem(&item);
  });
}

TEST_CASE("note chooser clamps to its range and turns around immediately")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsNoteChooser item{nullptr};
    scene.addItem(&item);
    item.setValue(120);

    score::InfiniteScroller::cancel();

    press(scene, item, QPoint{500, 500});
    const QPoint p0 = pressPoint();

    move(scene, item, p0, p0 - QPoint{0, 300});
    REQUIRE(item.value() == 127);

    // Going back down must start moving again straight away rather than having
    // to unwind the travel that went past the top of the range.
    move(scene, item, p0 - QPoint{0, 300}, p0 - QPoint{0, 280});
    REQUIRE(item.value() == 125);

    release(scene, item, p0 - QPoint{0, 280});
    REQUIRE(item.value() == 125);

    scene.removeItem(&item);
  });
}
