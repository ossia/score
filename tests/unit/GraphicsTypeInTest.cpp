// The type-in box a right-click raises over a control that holds more than one
// number: the xy and xyz pads and the range slider.
//
// One box can be closed on QAbstractSpinBox::editingFinished, which is what
// DefaultGraphicsSliderImpl does for a plain slider. Several of them cannot:
// that signal is also emitted when the focus merely leaves a box, and moving
// from x to y is such a focus-out. The pad's first version closed on it, so
// clicking the y box took the whole box down and only x was ever reachable.
//
// Everything here is driven through a real view: the box's lifetime is decided
// by the focus, and Qt delivers no focus event outside an active window.

#include <score/graphics/RightClickWidget.hpp>
#include <score/graphics/widgets/QGraphicsHSVChooser.hpp>
#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/graphics/widgets/QGraphicsXYChooser.hpp>
#include <score/graphics/widgets/QGraphicsXYZChooser.hpp>

#include <QDoubleSpinBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Keyboard.hpp>
#include <score_test/Mouse.hpp>

namespace
{
//! Every spin box of the type-in box currently in the scene, left to right.
std::vector<QDoubleSpinBox*> boxesIn(QGraphicsScene& scene)
{
  std::vector<QDoubleSpinBox*> out;
  auto* proxy = score::currentRightClickWidget().data();
  if(!proxy || !proxy->widget())
    return out;

  for(auto* b : proxy->widget()->findChildren<QDoubleSpinBox*>())
    out.push_back(b);

  std::sort(out.begin(), out.end(), [](QDoubleSpinBox* a, QDoubleSpinBox* b) {
    return a->geometry().x() < b->geometry().x();
  });
  return out;
}

bool boxIsUp()
{
  return score::currentRightClickWidget().data() != nullptr;
}

//! A control in a shown, activated view, with its two signals counted.
template <typename Item>
struct Harness
{
  QGraphicsScene scene;
  QGraphicsView view{&scene};
  Item item{nullptr};

  int moved{}, released{};

  Harness()
  {
    scene.setSceneRect(0, 0, 400, 300);
    scene.addItem(&item);
    item.setPos(10, 10);
    view.resize(420, 320);

    QObject::connect(&item, &Item::sliderMoved, &item, [this] { moved++; });
    QObject::connect(&item, &Item::sliderReleased, &item, [this] { released++; });
  }

  ~Harness()
  {
    score::closeRightClickWidget();
    scene.removeItem(&item);
  }

  //! False when the platform never activates the window; the focus cases skip.
  bool activate() { return score::test::showAndActivate(view); }

  QPoint inView(QPointF itemPos) const
  {
    return view.mapFromScene(item.mapToScene(itemPos));
  }

  void click(QPointF itemPos, Qt::MouseButton b)
  {
    score::test::mouseClick(*view.viewport(), inView(itemPos), b);
    // The box is raised, focused and taken down from the event loop, so that
    // each outlives the click that asked for it.
    QApplication::processEvents();
    QApplication::processEvents();
  }

  //! Replaces what a field holds, the way a user does: select, then type.
  void typeIn(QDoubleSpinBox& b, const QString& text)
  {
    score::test::keyClick(b, Qt::Key_A, Qt::ControlModifier);
    score::test::keyClicks(b, text);
    QApplication::processEvents();
  }

  //! Clicks the middle of a spin box of the type-in box, through the view.
  void clickBox(QDoubleSpinBox& b)
  {
    auto* proxy = score::currentRightClickWidget().data();
    REQUIRE(proxy != nullptr);
    const QPointF scenePos = proxy->mapToScene(QPointF{b.geometry().center()});
    score::test::mouseClick(*view.viewport(), view.mapFromScene(scenePos));
    QApplication::processEvents();
    QApplication::processEvents();
  }
};

using XY = Harness<score::QGraphicsXYChooser>;
using XYZ = Harness<score::QGraphicsXYZChooser>;
using Range = Harness<score::QGraphicsRangeSlider>;
}

TEST_CASE("a pad's type-in box has one field per coordinate", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XY h;
    h.item.setRange({0.f, 0.f}, {1.f, 1.f}, {});
    h.item.setValue({0.25f, 0.75f});
    REQUIRE(h.activate());

    REQUIRE_FALSE(boxIsUp());
    h.click({50, 50}, Qt::RightButton);

    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 2);
    CHECK(boxes[0]->value() == Catch::Approx(0.25));
    CHECK(boxes[1]->value() == Catch::Approx(0.75));

    // Right-clicking a pad used to move the point to where it was clicked.
    CHECK(h.item.value()[0] == Catch::Approx(0.25f));
    CHECK(h.item.value()[1] == Catch::Approx(0.75f));
    CHECK(h.moved == 0);
  });
}

// The regression: the y box was unreachable, because clicking it took the
// focus off x, and the box closed on x's focus-out.
TEST_CASE("clicking the second field of a pad keeps the box up", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XY h;
    h.item.setRange({0.f, 0.f}, {1.f, 1.f}, {});
    REQUIRE(h.activate());

    h.click({50, 50}, Qt::RightButton);
    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 2);

    h.clickBox(*boxes[1]);

    REQUIRE(boxIsUp());
    CHECK(boxesIn(h.scene).size() == 2);
    CHECK(boxes[1]->hasFocus());
  });
}

TEST_CASE("both fields of a pad can be typed into", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XY h;
    h.item.setRange({0.f, 0.f}, {1.f, 1.f}, {});
    h.item.setValue({0.f, 0.f});
    REQUIRE(h.activate());

    h.click({50, 50}, Qt::RightButton);
    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 2);

    h.typeIn(*boxes[0], "0.25");
    CHECK(h.item.value()[0] == Catch::Approx(0.25f));

    h.clickBox(*boxes[1]);
    REQUIRE(boxIsUp());

    h.typeIn(*boxes[1], "0.75");
    CHECK(h.item.value()[1] == Catch::Approx(0.75f));
    CHECK(h.item.value()[0] == Catch::Approx(0.25f));

    // One release per field edited: each is its own undo step, as a drag is.
    CHECK(h.released == 1);
    CHECK(h.moved > 0);
  });
}

TEST_CASE("a pad's type-in box closes when the focus leaves it", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XY h;
    h.item.setRange({0.f, 0.f}, {1.f, 1.f}, {});
    REQUIRE(h.activate());

    h.click({50, 50}, Qt::RightButton);
    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 2);

    h.typeIn(*boxes[0], "0.5");

    // Somewhere in the scene that is not the box.
    score::test::mouseClick(*h.view.viewport(), h.view.mapFromScene(350, 250));
    QApplication::processEvents();
    QApplication::processEvents();

    CHECK_FALSE(boxIsUp());
    CHECK(h.released == 1);
  });
}

TEST_CASE("enter closes a pad's type-in box", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XY h;
    h.item.setRange({0.f, 0.f}, {1.f, 1.f}, {});
    REQUIRE(h.activate());

    h.click({50, 50}, Qt::RightButton);
    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 2);

    h.typeIn(*boxes[0], "0.5");
    score::test::keyClick(*boxes[0], Qt::Key_Return);
    QApplication::processEvents();
    QApplication::processEvents();

    CHECK_FALSE(boxIsUp());
    CHECK(h.item.value()[0] == Catch::Approx(0.5f));
    CHECK(h.released == 1);
  });
}

TEST_CASE("an xyz pad types into all three coordinates", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XYZ h;
    h.item.setRange({0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {});
    h.item.setValue({0.1f, 0.2f, 0.3f});
    REQUIRE(h.activate());

    h.click({50, 50}, Qt::RightButton);

    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 3);
    CHECK(boxes[2]->value() == Catch::Approx(0.3));

    // The right-click itself moved nothing: it used to drag the point.
    CHECK(h.item.value()[0] == Catch::Approx(0.1f));
    CHECK(h.moved == 0);

    h.clickBox(*boxes[2]);
    REQUIRE(boxIsUp());
    h.typeIn(*boxes[2], "0.9");

    CHECK(h.item.value()[2] == Catch::Approx(0.9f));
    CHECK(h.item.value()[0] == Catch::Approx(0.1f));
  });
}

// The z the pad keeps between presses is normalized, and a value set from
// outside never reached it: the next drag in the xy square put the old z back.
TEST_CASE("an xyz pad keeps a z that came from outside", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    XYZ h;
    h.item.setRange({0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {});
    h.item.setValue({0.5f, 0.5f, 0.8f});
    REQUIRE(h.activate());

    // A drag in the xy square, which reads x and y off the click and keeps z.
    h.click({20, 20}, Qt::LeftButton);

    CHECK(h.item.value()[2] == Catch::Approx(0.8f));
  });
}

TEST_CASE("a range slider types into its two bounds", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Range h;
    h.item.setRange(0., 10., {0.f, 10.f});
    h.item.setValue({2.f, 8.f});
    REQUIRE(h.activate());

    h.click({10, 5}, Qt::RightButton);

    auto boxes = boxesIn(h.scene);
    REQUIRE(boxes.size() == 2);
    CHECK(boxes[0]->value() == Catch::Approx(2.));
    CHECK(boxes[1]->value() == Catch::Approx(8.));

    // A right press grabs no handle, so it must not report a drag either.
    CHECK(h.moved == 0);
    CHECK(h.released == 0);

    h.clickBox(*boxes[1]);
    REQUIRE(boxIsUp());
    h.typeIn(*boxes[1], "5");

    CHECK(h.item.value()[1] == Catch::Approx(5.f));
    CHECK(h.item.value()[0] == Catch::Approx(2.f));
  });
}

// The colour pad took any button, so a right click repainted the colour on the
// way to the dialog it now opens.
TEST_CASE("right-clicking an hsv pad leaves the colour alone", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Harness<score::QGraphicsHSVChooser> h;
    h.item.setRgbaValue({0.2f, 0.4f, 0.6f, 1.f});
    REQUIRE(h.activate());

    const auto before = h.item.rgbaValue();

    // Press only: the release would open the dialog, which owns a loop.
    score::test::mouseEvent(
        *h.view.viewport(), QEvent::MouseButtonPress, h.inView({50, 50}),
        Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QApplication::processEvents();

    CHECK(h.item.rgbaValue() == before);
    CHECK(h.moved == 0);
  });
}
