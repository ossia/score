// Double-click on the time chooser's knob resets it: synced, to the default
// note, straight (the knob then steps through the straight values again);
// free, to the control's default through the taper, at both ends of the range.

#include <score/graphics/widgets/QGraphicsTimeChooser.hpp>

#include <score_test/App.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
void doubleClickKnob(QGraphicsScene& scene, score::QGraphicsTimeChooser& item)
{
  const QPointF pos{17., 12.}; // on the dial, above the readout
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseDoubleClick};
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
  ev.setPos(pos);
  ev.setScenePos(item.mapToScene(pos));
  scene.sendEvent(&item, &ev);
}
}

TEST_CASE("time chooser: a double-click on a triplet goes back to 1/8, straight", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* item = new score::QGraphicsTimeChooser{nullptr};
    scene.addItem(item);

    item->setValue(ossia::vec2f{0.25f, 1.f});
    item->cycleMode(); // dotted
    item->cycleMode(); // triplet
    REQUIRE(item->feel() == score::QGraphicsTimeChooser::Feel::Triplet);

    doubleClickKnob(scene, *item);
    CHECK(item->synced());
    CHECK(item->value() == ossia::vec2f{0.125f, 1.f});
    CHECK(item->feel() == score::QGraphicsTimeChooser::Feel::Straight);
  });
}

TEST_CASE("time chooser: a free double-click reaches both ends of a tapered range", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* item = new score::QGraphicsTimeChooser{nullptr};
    scene.addItem(item);
    // The cube taper of the control factory, on 0 .. 60 s
    item->positionToSeconds = [](double p) { return 60. * p * p * p; };
    item->secondsToPosition = [](double s) { return std::cbrt(s / 60.); };

    for(auto [init, pos] : {std::pair{0., 0.}, {60., 1.}, {7.5, 0.5}})
    {
      INFO("default " << init << " s");
      item->setRange(0., 60., init);
      item->setValue(ossia::vec2f{0.3f, 0.f});
      doubleClickKnob(scene, *item);
      CHECK(!item->synced());
      CHECK(item->value()[0] == Catch::Approx(pos).margin(1e-6));
    }

    // Past the range: stays on the knob
    item->setRange(0., 60., 100.);
    doubleClickKnob(scene, *item);
    CHECK(item->value()[0] == 1.f);

    // The readout at both ends
    item->setValue(ossia::vec2f{0.f, 0.f});
    CHECK(item->freeText() == QStringLiteral("0.0 ms"));
    item->setValue(ossia::vec2f{1.f, 0.f});
    CHECK(item->freeText() == QStringLiteral("60.0 s"));
  });
}
