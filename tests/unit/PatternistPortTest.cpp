// The pattern-select port is the selector: the model property follows it, so
// the grid the layer draws changes with the port rather than only the sound.
// Before, the port reached the executor alone, and moving it did nothing at all
// in the editor.

#include <score_test/App.hpp>

#include <Patternist/PatternModel.hpp>

#include <Process/Dataflow/Port.hpp>

#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
std::unique_ptr<Patternist::ProcessModel> makeProcess()
{
  return std::make_unique<Patternist::ProcessModel>(
      TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr);
}
}

TEST_CASE("patternist: the port selects the current pattern", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    REQUIRE(proc->patternSelect);
    REQUIRE(proc->currentPattern() == 0);

    proc->patternSelect->setValue(2);
    CHECK(proc->currentPattern() == 2);

    proc->patternSelect->setValue(0);
    CHECK(proc->currentPattern() == 0);
  });
}

TEST_CASE("patternist: selecting past the end grows the list", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    const auto before = std::ssize(proc->patterns());
    proc->patternSelect->setValue(int(before) + 1);

    CHECK(std::ssize(proc->patterns()) == before + 2);
    CHECK(proc->currentPattern() == int(before) + 1);
  });
}

TEST_CASE("patternist: a negative selection is clamped", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    proc->patternSelect->setValue(-5);
    CHECK(proc->currentPattern() == 0);
  });
}

TEST_CASE(
    "patternist: the switch quantization port holds an ossia rate", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    REQUIRE(proc->switchQuantification);

    // The scale QuantificationWidget uses: a rate fed straight to
    // get_quantification_dates(), never the inverse "fraction of a whole note".
    const double init = ossia::convert<float>(proc->switchQuantification->value());
    CHECK(init == 1.); // one bar
  });
}
