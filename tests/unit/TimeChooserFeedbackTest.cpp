#include <score/graphics/widgets/QGraphicsTimeChooser.hpp>

#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_all.hpp>

namespace
{
// DeviceRecorder's chooser
constexpr double range_min = 0.00001;
constexpr double range_max = 5.;

double from01(double v) noexcept
{
  return range_min + v * (range_max - range_min);
}

// What the control widget factory feeds setExecutionValue() when the executor
// pushes a plain duration in seconds.
ossia::vec2f secondsFeedback(double secs) noexcept
{
  return {float((secs - range_min) / (range_max - range_min)), 0.f};
}
}

TEST_CASE("time chooser feedback lands where the knob is")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsTimeChooser item{nullptr};
    item.setRange(range_min, range_max, range_min);

    for(const float pos : {0.f, 0.1f, 0.5f, 1.f})
    {
      item.setValue(ossia::vec2f{pos, 0.f});
      REQUIRE(item.value()[0] == Catch::Approx(pos).margin(1e-5));

      item.setExecutionValue(secondsFeedback(from01(pos)));
      REQUIRE(item.executionPosition() == Catch::Approx(pos).margin(1e-5));
    }
  });
}

TEST_CASE("time chooser feedback does not snap a free value onto a division")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsTimeChooser item{nullptr};
    item.setRange(range_min, range_max, range_min);

    item.setValue(ossia::vec2f{0.1f, 0.f});
    item.setExecutionValue(secondsFeedback(0.5));

    REQUIRE(item.executionPosition() == Catch::Approx(0.1).margin(1e-5));
    // 0.5 read as a whole-note fraction is the "1/2" detent, at 2/3 of the arc
    REQUIRE(item.executionPosition() < 0.5);
  });
}

TEST_CASE("time chooser feedback ignores the sync flag of the value it is given")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsTimeChooser item{nullptr};
    item.setRange(range_min, range_max, range_min);

    // A scalar converted to vec2f gets both of its components filled
    const auto filled = ossia::convert<ossia::vec2f>(ossia::value{0.5f});
    REQUIRE(filled[1] == 0.5f);

    item.setValue(ossia::vec2f{0.5f, 0.f});
    item.setExecutionValue(filled);
    REQUIRE(item.executionPosition() == Catch::Approx(0.5));
  });
}

TEST_CASE("time chooser feedback follows the division detents when synced")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::QGraphicsTimeChooser item{nullptr};
    item.setRange(range_min, range_max, range_min);

    item.setValue(ossia::vec2f{0.25f, 1.f});
    REQUIRE(item.value()[0] == Catch::Approx(0.25));

    item.setExecutionValue(ossia::vec2f{0.25f, 1.f});
    REQUIRE(item.executionPosition() == Catch::Approx(11. / 21.));

    // Not in the division domain: kept as a plain arc position rather than
    // being rounded onto a detent
    item.setExecutionValue(ossia::vec2f{0.1f, 0.f});
    REQUIRE(item.executionPosition() == Catch::Approx(0.1).margin(1e-5));
  });
}

// A click on the readout cycles free, straight, dotted, triplet; the note
// stays the same across the three feels.
TEST_CASE("time chooser readout cycles free, straight, dotted and triplet")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    using Feel = score::QGraphicsTimeChooser::Feel;
    score::QGraphicsTimeChooser item{nullptr};
    item.setRange(0.001, 5., 0.1);
    item.setValue(ossia::vec2f{1.f / 8.f, 1.f}); // 1/8
    REQUIRE(item.synced());
    REQUIRE(item.feel() == Feel::Straight);

    item.cycleMode();
    CHECK(item.feel() == Feel::Dotted);
    CHECK(item.value()[0] == Catch::Approx(3.f / 16.f)); // 1/8.
    CHECK(item.value()[1] != 0.f);

    item.cycleMode();
    CHECK(item.feel() == Feel::Triplet);
    CHECK(item.value()[0] == Catch::Approx(1.f / 12.f)); // 1/8T

    item.cycleMode();
    CHECK(!item.synced());
    CHECK(item.value()[1] == 0.f);

    // Back to synced: the same note, straight
    item.cycleMode();
    CHECK(item.synced());
    CHECK(item.feel() == Feel::Straight);
    CHECK(item.value()[0] == Catch::Approx(1.f / 8.f));

    // A value given as dotted is in the dotted feel
    item.setValue(ossia::vec2f{3.f / 8.f, 1.f}); // 1/4.
    CHECK(item.feel() == Feel::Dotted);
  });
}
