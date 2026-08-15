// Which control a device parameter dropped on a Control surface turns into.

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Device/Address/AddressSettings.hpp>

#include <ControlSurface/Process.hpp>

#include <score_test/App.hpp>

#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_all.hpp>

#include <memory>

namespace
{
std::unique_ptr<Process::ControlInlet> make(const Device::FullAddressAccessorSettings& s)
{
  return std::unique_ptr<Process::ControlInlet>{
      ControlSurface::makeControlFromType(Id<Process::Port>{0}, s, nullptr)};
}
}

TEST_CASE("a parameter with a value list becomes a combo box")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::FullAddressAccessorSettings s;
    s.value = std::string{"b"};
    s.domain
        = ossia::make_domain(std::vector<ossia::value>{std::string{"a"}, std::string{"b"},
                                                      std::string{"c"}});

    auto ctl = make(s);
    auto* combo = dynamic_cast<Process::ComboBox*>(ctl.get());
    REQUIRE(combo != nullptr);

    REQUIRE(combo->count() == 3);
    CHECK(combo->getValues()[0].second == ossia::value{std::string{"a"}});
    CHECK(combo->getValues()[2].second == ossia::value{std::string{"c"}});
    // Labelled as the user reads them, not as the parser writes them.
    CHECK(combo->getValues()[0].first == "a");
    CHECK(combo->getValues()[2].first == "c");
    CHECK(combo->value() == ossia::value{std::string{"b"}});
  });
}

TEST_CASE("an int enumeration becomes a combo box too")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::FullAddressAccessorSettings s;
    s.value = 2;
    s.domain = ossia::make_domain(std::vector<ossia::value>{1, 2, 3});

    auto ctl = make(s);
    auto* combo = dynamic_cast<Process::ComboBox*>(ctl.get());
    REQUIRE(combo != nullptr);
    REQUIRE(combo->count() == 3);
    CHECK(combo->getValues()[1].second == ossia::value{2});
  });
}

TEST_CASE("a plain min/max parameter keeps its slider")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::FullAddressAccessorSettings s;
    s.value = 0.5f;
    s.domain = ossia::make_domain(0.f, 1.f);

    auto ctl = make(s);
    CHECK(dynamic_cast<Process::ComboBox*>(ctl.get()) == nullptr);
    CHECK(dynamic_cast<Process::FloatSlider*>(ctl.get()) != nullptr);
  });
}

// What an editable combo box does with text the enumeration does not list.
TEST_CASE("free combo-box text is read as the enumeration's type")
{
  using WidgetFactory::ComboBox;
  const std::vector<std::pair<QString, ossia::value>> strings{
      {"a", std::string{"a"}}, {"b", std::string{"b"}}};
  const std::vector<std::pair<QString, ossia::value>> ints{{"1", 1}, {"2", 2}};
  const std::vector<std::pair<QString, ossia::value>> floats{{"1.5", 1.5f}};

  CHECK(ComboBox::parseFreeValue("zzz", strings) == ossia::value{std::string{"zzz"}});
  CHECK(ComboBox::parseFreeValue("12", ints) == ossia::value{12});
  CHECK(ComboBox::parseFreeValue("-0.25", floats) == ossia::value{-0.25f});

  // Nonsense for the type is dropped rather than turned into a zero.
  CHECK_FALSE(ComboBox::parseFreeValue("zzz", ints).has_value());
  CHECK_FALSE(ComboBox::parseFreeValue("zzz", floats).has_value());
}

// A vecf_domain lists bounds per component, not values the parameter takes:
// reading them as an enumeration turns a 3-D position into a list of floats.
TEST_CASE("a vec with a per-component domain is not an enumeration")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::FullAddressAccessorSettings s;
    s.value = ossia::vec3f{{0.f, 0.f, 0.f}};

    ossia::vecf_domain<3> dom;
    dom.min = {0.f, 0.f, 0.f};
    dom.max = {1.f, 1.f, 1.f};
    s.domain = ossia::domain{dom};

    auto ctl = make(s);
    CHECK(dynamic_cast<Process::ComboBox*>(ctl.get()) == nullptr);
    CHECK(dynamic_cast<Process::XYZSlider*>(ctl.get()) != nullptr);
  });
}
