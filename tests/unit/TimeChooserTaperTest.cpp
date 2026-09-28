// The time chooser's free mode through halp::time_chooser_mapper: short times
// get most of the knob's travel, 0 and the maximum are both reachable.

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <halp/mappers.hpp>

#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsTimeChooser.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using Taper = WidgetFactory::MappedNormalizer<halp::time_chooser_mapper>;

TEST_CASE("time chooser taper: the cube of the travel", "[time_chooser]")
{
  // 0 .. 5 s
  CHECK(Taper::from01(0., 5., 0.) == 0.);
  CHECK(Taper::from01(0., 5., 1.) == Approx(5.));
  CHECK(Taper::from01(0., 5., 0.1) == Approx(0.005)); // 5 ms at a tenth
  CHECK(Taper::from01(0., 5., 0.2) == Approx(0.04));
  CHECK(Taper::from01(0., 5., 0.5) == Approx(0.625));

  // And back
  for(double s : {0., 0.001, 0.02, 0.3, 2., 5.})
    CHECK(Taper::from01(0., 5., Taper::to01(0., 5., s)) == Approx(s).margin(1e-9));

  // Out of range values stay on the knob
  CHECK(Taper::to01(0., 5., -1.) == 0.);
  CHECK(Taper::to01(0., 5., 9.) == Approx(1.));
}

TEST_CASE("time chooser taper: a control starting at 0 reaches 0", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Process::TimeChooser attack{0.f, 5.f, 0.001f, QStringLiteral("Attack"), Id<Process::Port>{0}, nullptr};
    WidgetFactory::FixedNormalizer<Taper> norm{attack};
    CHECK(norm.from01(0.) == 0.);
    CHECK(norm.from01(1.) == Approx(5.));
    // The default, 1 ms, is a tenth of the travel away from 0, not a pixel
    CHECK(norm.to01(0.001) == Approx(0.0585).margin(1e-3));
  });
}

TEST_CASE("time chooser readout: the unit always fits", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsTimeChooser item{nullptr};
    item.setRange(0., 1., 0.);
    auto text = [&](float secs) {
      item.setValue(ossia::vec2f{secs, 0.f}); // linear, no normalizer set
      return item.freeText();
    };
    // ADSR's default attack, 0.01f, is 9.99999977 ms: it reads "10 ms"
    CHECK(text(0.01f) == QStringLiteral("10 ms"));
    CHECK(text(0.0005f) == QStringLiteral("0.5 ms"));
    CHECK(text(0.009f) == QStringLiteral("9.0 ms"));
    CHECK(text(0.25f) == QStringLiteral("250 ms"));
    CHECK(text(0.9999f) == QStringLiteral("1.00 s"));
  });
}

TEST_CASE("time chooser: a number is seconds, free-running", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Process::TimeChooser t{0.f, 5.f, 0.1f, QStringLiteral("Decay"), Id<Process::Port>{0}, nullptr};
    // What a preset saved when the control was a slider holds
    t.setValue(0.3f);
    CHECK(t.value() == ossia::value{ossia::vec2f{0.3f, 0.f}});
    t.setValue(2);
    CHECK(t.value() == ossia::value{ossia::vec2f{2.f, 0.f}});
    // A time chooser's own value is kept, synced included
    t.setValue(ossia::vec2f{0.25f, 1.f});
    CHECK(t.value() == ossia::value{ossia::vec2f{0.25f, 1.f}});
  });
}

TEST_CASE("time chooser: the footprint of a knob", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Rows center their items: a shorter time chooser sits lower than the
    // knobs next to it.
    score::QGraphicsTimeChooser t{nullptr};
    score::QGraphicsKnob k{nullptr};
    CHECK(t.boundingRect() == k.boundingRect());
  });
}

TEST_CASE("time chooser: where the time is musical, a note value", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto make = [](float init) {
      return std::make_unique<Process::TimeChooser>(
          0.f, 60.f, init, QStringLiteral("t"), Id<Process::Port>{0}, nullptr);
    };
    // 0.5 s at 120 BPM is a quarter note: synced to it, value and init
    auto quarter = make(0.5f);
    CHECK(quarter->preferSynced(120.));
    CHECK(quarter->value() == ossia::value{ossia::vec2f{0.25f, 1.f}});
    CHECK(quarter->init() == ossia::value{ossia::vec2f{0.25f, 1.f}});
    // 1 s: a half note
    auto half = make(1.f);
    CHECK(half->preferSynced(120.));
    CHECK(half->value() == ossia::value{ossia::vec2f{0.5f, 1.f}});
    // A 10 ms attack is nowhere near a note value (1/64 is 31 ms): kept
    auto attack = make(0.01f);
    CHECK(!attack->preferSynced(120.));
    CHECK(attack->value() == ossia::value{ossia::vec2f{0.01f, 0.f}});
    // Already synced: left alone
    auto synced = make(0.5f);
    synced->setValue(ossia::vec2f{0.125f, 1.f});
    CHECK(!synced->preferSynced(120.));
  });
}
