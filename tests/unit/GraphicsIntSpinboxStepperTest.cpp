// The +/- strip of the int spinbox (and the combo box's, which shares it):
// a click on a half steps the value by one within the range, and a held half
// flashes in the text's orange.

#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>
#include <score/graphics/widgets/Stepper.hpp>
#include <score/model/Skin.hpp>

#include <score_test/App.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_all.hpp>

namespace
{
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

QPointF onHalf(const QRectF& box, int dir)
{
  const QRectF strip = score::Stepper::rect(box);
  return {
      strip.center().x(), dir > 0 ? strip.top() + strip.height() / 4.
                                  : strip.bottom() - strip.height() / 4.};
}

void send(Scene& scene, QGraphicsItem& item, QEvent::Type type, QPointF at)
{
  QGraphicsSceneMouseEvent ev{type};
  ev.setButton(Qt::LeftButton);
  ev.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
  ev.setScreenPos({500, 500});
  ev.setLastScreenPos({500, 500});
  ev.setButtonDownScreenPos(Qt::LeftButton, {500, 500});
  ev.setScenePos(at);
  ev.setPos(at);
  scene.sendEvent(&item, &ev);
}

void click(Scene& scene, QGraphicsItem& item, int dir)
{
  const auto at = onHalf(item.boundingRect(), dir);
  send(scene, item, QEvent::GraphicsSceneMousePress, at);
  send(scene, item, QEvent::GraphicsSceneMouseRelease, at);
}

QImage render(QGraphicsItem& item)
{
  const QRectF r = item.boundingRect();
  QImage img{r.size().toSize(), QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);
  QPainter p{&img};
  QStyleOptionGraphicsItem opt;
  item.paint(&p, &opt, nullptr);
  return img;
}

// A point of the upper half away from the + glyph.
QColor upperHalfBackground(QGraphicsItem& item)
{
  const QRectF strip = score::Stepper::rect(item.boundingRect());
  const auto img = render(item);
  return img.pixelColor(QPoint(int(strip.left()) + 1, int(strip.top()) + 2));
}
}

TEST_CASE("the int spinbox steps by one within its range", "[widgets][stepper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsIntSpinbox item{nullptr};
    scene.addItem(&item);
    item.setRange(0, 3, 0);
    item.setValue(1);

    int moved{}, released{};
    QObject::connect(&item, &score::QGraphicsIntSpinbox::sliderMoved, &item, [&] { moved++; });
    QObject::connect(
        &item, &score::QGraphicsIntSpinbox::sliderReleased, &item, [&] { released++; });

    click(scene, item, +1);
    CHECK(item.value() == 2);
    click(scene, item, +1);
    click(scene, item, +1);
    CHECK(item.value() == 3); // stops at the maximum
    click(scene, item, -1);
    CHECK(item.value() == 2);
    // Each step is one edit; the one past the maximum is none.
    CHECK(moved == 3);
    CHECK(released == 3);
    CHECK_FALSE(item.dragging());
  });
}

TEST_CASE("leaving the int spinbox's strip before releasing cancels the step", "[widgets][stepper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsIntSpinbox item{nullptr};
    scene.addItem(&item);
    item.setRange(0, 10, 0);
    item.setValue(5);
    send(scene, item, QEvent::GraphicsSceneMousePress, onHalf(static_cast<QGraphicsItem&>(item).boundingRect(), +1));
    send(scene, item, QEvent::GraphicsSceneMouseMove, {1., 1.});
    send(scene, item, QEvent::GraphicsSceneMouseRelease, {1., 1.});
    CHECK(item.value() == 5);
  });
}

TEST_CASE("a held half of the strip is orange, not blue", "[widgets][stepper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QColor orange = score::Skin::instance().Base4.color();
    Scene scene;

    score::QGraphicsIntSpinbox spin{nullptr};
    scene.addItem(&spin);
    spin.setRange(0, 10, 0);
    CHECK(upperHalfBackground(spin) != orange);
    send(scene, spin, QEvent::GraphicsSceneMousePress, onHalf(static_cast<QGraphicsItem&>(spin).boundingRect(), +1));
    CHECK(upperHalfBackground(spin) == orange);
    send(scene, spin, QEvent::GraphicsSceneMouseRelease, onHalf(static_cast<QGraphicsItem&>(spin).boundingRect(), +1));

    score::QGraphicsCombo combo{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&combo);
    const auto at = onHalf(combo.boundingRect().adjusted(1, 1, -1, -1), +1);
    send(scene, combo, QEvent::GraphicsSceneMousePress, at);
    const QRectF strip = combo.stepperRect();
    const auto img = render(combo);
    CHECK(img.pixelColor(QPoint(int(strip.left()) + 1, int(strip.top()) + 2)) == orange);
    send(scene, combo, QEvent::GraphicsSceneMouseRelease, at);
  });
}

// Buffer Queue's "Max length": 0 to 100000. The value is stored normalized;
// truncating it on the way back makes + from 100 give 100.99999 -> 100.
TEST_CASE("the int spinbox steps on a large range", "[widgets][stepper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsIntSpinbox item{nullptr};
    scene.addItem(&item);
    item.setRange(0, 100000, 100);
    for(int v : {100, 777, 99999})
    {
      CAPTURE(v);
      item.setValue(v);
      CHECK(item.value() == v);
      click(scene, item, +1);
      CHECK(item.value() == v + 1);
      click(scene, item, -1);
      CHECK(item.value() == v);
    }
  });
}
