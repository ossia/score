// A combo box whose entries have values takes an entry's label for its value:
// presets name the entry ("Toggle") where the port holds its value (1).

#include <Process/Dataflow/WidgetInlets.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("Combo box: a label selects its entry's value", "[process][combobox]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Process::ComboBox c{
        {{"Off", 0}, {"Toggle", 1}, {"Momentary", 2}}, 0, "Latch", Id<Process::Port>{0},
        nullptr};
    c.setValue(std::string("Toggle"));
    CHECK(c.value() == ossia::value{1});
    CHECK(c.indexOfValue(c.value()) == 1);
    CHECK(c.indexOfValue(std::string("Momentary")) == 2);

    c.setValue(2);
    CHECK(c.value() == ossia::value{2});
    // Not a label: kept as it is, and selects nothing
    c.setValue(std::string("Sideways"));
    CHECK(c.indexOfValue(c.value()) == -1);

    // Entries whose values are strings keep the string given
    Process::ComboBox s{
        {{"First", std::string("a.wav")}, {"Second", std::string("b.wav")}},
        std::string("a.wav"), "File", Id<Process::Port>{1}, nullptr};
    s.setValue(std::string("b.wav"));
    CHECK(s.value() == ossia::value{std::string("b.wav")});
    s.setValue(std::string("First"));
    CHECK(s.value() == ossia::value{std::string("a.wav")});
  });
}
