// Double-clicking a control puts it back to the default the process declared.
//
// Every graphics-view control routes that through DefaultGraphicsSliderImpl /
// DefaultGraphicsSpinboxImpl, but QGraphicsIntSlider never installed the
// handler, so the integer sliders of e.g. "Pulse to Midi" (Pitch shift, Pitch
// random, Vel. random) could not be reset the way its spinboxes could.
//
// The inspector shows the same controls as plain QWidgets (score::IntSlider,
// score::DoubleSlider), which had no reset at all. Those also carry the
// linear map()/unmap() the right-click type-in box builds its range from.

#include <score/graphics/widgets/QGraphicsIntSlider.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/IntSlider.hpp>

#include <score_test/App.hpp>
#include <score_test/Mouse.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_all.hpp>

namespace
{
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

//! Press, release, double-click, release: what the scene sends on a real
//! double click.
template <typename Item>
void doubleClick(Scene& scene, Item& item, QPointF pos = {1., 1.})
{
  for(auto type :
      {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease,
       QEvent::GraphicsSceneMouseDoubleClick, QEvent::GraphicsSceneMouseRelease})
  {
    QGraphicsSceneMouseEvent ev{type};
    ev.setButton(Qt::LeftButton);
    ev.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
                                                            : Qt::LeftButton);
    ev.setPos(pos);
    ev.setScenePos(pos);
    ev.setScreenPos(pos.toPoint());
    ev.setLastScreenPos(pos.toPoint());
    ev.setButtonDownScreenPos(Qt::LeftButton, pos.toPoint());
    scene.sendEvent(&item, &ev);
  }
  qApp->processEvents();
}

void widgetDoubleClick(QWidget& w, QPoint pos)
{
  score::test::mouseEvent(
      w, QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  score::test::mouseEvent(
      w, QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  score::test::mouseEvent(
      w, QEvent::MouseButtonDblClick, pos, Qt::LeftButton, Qt::LeftButton,
      Qt::NoModifier);
  score::test::mouseEvent(
      w, QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  qApp->processEvents();
}
}

TEST_CASE("double-clicking a graphics int slider restores its default")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsIntSlider item{nullptr};
    scene.addItem(&item);

    // "Pitch shift" of Pulse to Midi: octave_slider<-5, 5>, default 0.
    item.setRange(-60, 60, 0);
    item.setValue(37);
    REQUIRE(item.value() == 37);

    // The press that opens the double click moves the value too, so what
    // matters is what the last submit carried.
    int lastMoved{-1}, released{};
    QObject::connect(&item, &score::QGraphicsIntSlider::sliderMoved, &item, [&] {
      lastMoved = item.value();
    });
    QObject::connect(
        &item, &score::QGraphicsIntSlider::sliderReleased, &item, [&] { released++; });

    doubleClick(scene, item);

    CHECK(item.value() == 0);
    // The value has to reach the model: moved submits, released commits.
    CHECK(lastMoved == 0);
    CHECK(released >= 1);
  });
}

// The float slider already did this; it is here so the two stay in step.
TEST_CASE("double-clicking a graphics float slider restores its default")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsSlider item{nullptr};
    scene.addItem(&item);

    item.setRange(0., 1., 0.8);
    item.setValue(0.1);

    doubleClick(scene, item);
    CHECK(item.value() == Catch::Approx(0.8));
  });
}

TEST_CASE("double-clicking an inspector int slider restores its default")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::ValueSlider sl{nullptr};
    sl.resize(200, 20);
    sl.setRange(-60, 60, 0);
    sl.setValue(42);
    REQUIRE(sl.value() == 42);

    int moved{}, released{};
    QObject::connect(&sl, &score::IntSlider::sliderMoved, &sl, [&] { moved++; });
    QObject::connect(&sl, &score::IntSlider::sliderReleased, &sl, [&] { released++; });

    widgetDoubleClick(sl, {180, 10});

    CHECK(sl.value() == 0);
    CHECK(moved >= 1);
    CHECK(released >= 1);
  });
}

TEST_CASE("double-clicking an inspector double slider restores its default")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::ValueDoubleSlider sl{nullptr};
    sl.resize(200, 20);
    sl.setRange(0., 1., 0.8);
    sl.setValue(0.1);

    int moved{}, released{};
    QObject::connect(&sl, &score::DoubleSlider::sliderMoved, &sl, [&](double) { moved++; });
    QObject::connect(&sl, &score::DoubleSlider::sliderReleased, &sl, [&] { released++; });

    widgetDoubleClick(sl, {180, 10});

    // The widget stores a normalized position; the default is in model units.
    CHECK(sl.value() == Catch::Approx(0.8));
    CHECK(moved >= 1);
    CHECK(released >= 1);
  });
}

// The transport speed slider carries no domain at all: x1 is its default.
TEST_CASE("double-clicking the speed slider goes back to x1")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::SpeedSlider sl{nullptr};
    sl.resize(200, 20);
    sl.setSpeed(3.);
    REQUIRE(sl.speed() == Catch::Approx(3.));

    widgetDoubleClick(sl, {10, 10});
    CHECK(sl.speed() == Catch::Approx(1.));
  });
}

// A range that does not start at zero: unmap() had min applied on the wrong
// side of the division, so the default landed nowhere near where it belongs.
TEST_CASE("an inspector double slider maps a shifted range")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::ValueDoubleSlider sl{nullptr};
    sl.setRange(-10., 30., 10.);

    CHECK(sl.unmap(-10.) == Catch::Approx(0.));
    CHECK(sl.unmap(30.) == Catch::Approx(1.));
    CHECK(sl.unmap(10.) == Catch::Approx(0.5));

    sl.setValue(0.25);
    CHECK(sl.map(sl.value()) == Catch::Approx(0.));

    // What createPopup() asks for when the user right-clicks: the two ends of
    // the range, which used to come back as the same number.
    CHECK(sl.map(0.) == Catch::Approx(-10.));
    CHECK(sl.map(1.) == Catch::Approx(30.));
  });
}
