// Coverage guard for the add / edit address dialog: every value type a
// parameter can hold must have a form, and that form must give back a value of
// the type the user asked for.

#include <Device/Address/AddressSettings.hpp>

#include <Explorer/Common/AddressSettings/AddressSettingsFactory.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>

#include <score_test/App.hpp>
#include <score_test/Keyboard.hpp>

#include <ossia/network/common/parameter_properties.hpp>
#include <ossia/network/domain/domain.hpp>

#include <Explorer/Explorer/ValueEditors.hpp>

#include <QLineEdit>

#include <catch2/catch_all.hpp>

#include <array>
#include <memory>

namespace
{
// Everything ossia::val_type can be, in its own order. NONE is not a value a
// parameter holds; it is the absence of one, and has its own form.
constexpr std::array<ossia::val_type, 10> allTypes{
    ossia::val_type::FLOAT,  ossia::val_type::INT,    ossia::val_type::VEC2F,
    ossia::val_type::VEC3F,  ossia::val_type::VEC4F,  ossia::val_type::IMPULSE,
    ossia::val_type::BOOL,   ossia::val_type::STRING, ossia::val_type::LIST,
    ossia::val_type::MAP};

std::unique_ptr<Explorer::AddressSettingsWidget> formFor(ossia::val_type t)
{
  return std::unique_ptr<Explorer::AddressSettingsWidget>{
      Explorer::AddressSettingsFactory{}(t)};
}
}

TEST_CASE("every value type has a form in the address dialog", "[explorer][dialog]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(auto t : allTypes)
    {
      INFO("val_type " << (int)t);
      auto form = formFor(t);
      REQUIRE(form != nullptr);
    }
  });
}

TEST_CASE("a form gives back the type it was asked for", "[explorer][dialog]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(auto t : allTypes)
    {
      INFO("val_type " << (int)t);
      auto form = formFor(t);
      REQUIRE(form != nullptr);

      // Regression: MAP had no form, so the dialog fell back to "Int by
      // default" and quietly turned the address into an int with value 0.
      CHECK(form->getSettings().value.get_type() == t);
      CHECK(form->getDefaultSettings().value.get_type() == t);
    }
  });
}

TEST_CASE("a form reads back the settings it was given", "[explorer][dialog]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::AddressSettings in;
    in.name = "p";
    in.ioType = ossia::access_mode::GET;
    in.clipMode = ossia::bounding_mode::CLIP;

    for(auto t : allTypes)
    {
      INFO("val_type " << (int)t);
      auto form = formFor(t);
      REQUIRE(form != nullptr);

      auto settings = in;
      settings.value = form->getDefaultSettings().value;
      settings.domain = form->getDefaultSettings().domain;
      form->setSettings(settings);

      const auto out = form->getSettings();
      CHECK(out.ioType == in.ioType);
      CHECK(out.clipMode == in.clipMode);
      CHECK(out.value.get_type() == t);
    }
  });
}

// Regression: getDefaultSettings() built a settings object and then returned a
// default-constructed one, so a new vec address got no domain.
TEST_CASE("a vec form has a default domain", "[explorer][dialog]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(auto t : {ossia::val_type::VEC2F, ossia::val_type::VEC3F,
                  ossia::val_type::VEC4F})
    {
      INFO("val_type " << (int)t);
      auto form = formFor(t);
      REQUIRE(form != nullptr);
      CHECK(bool(form->getDefaultSettings().domain.get()));
    }
  });
}

TEST_CASE("a list or map form keeps what was typed into it", "[explorer][dialog]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::AddressSettings in;
    in.name = "p";
    in.ioType = ossia::access_mode::BI;

    in.value = std::vector<ossia::value>{1, 2, 3};
    auto lst = formFor(ossia::val_type::LIST);
    REQUIRE(lst != nullptr);
    lst->setSettings(in);
    CHECK(lst->getSettings().value == in.value);

    in.value = ossia::value_map_type{{"a", ossia::value{1}}};
    auto map = formFor(ossia::val_type::MAP);
    REQUIRE(map != nullptr);
    map->setSettings(in);
    CHECK(map->getSettings().value == in.value);
  });
}

// A typo in the value field used to replace the address's contents with an
// empty list: `5` parses fine, just not as a list.
TEST_CASE("a typo in a list form does not empty the address", "[explorer][dialog]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::AddressSettings in;
    in.name = "p";
    in.ioType = ossia::access_mode::BI;
    in.value = std::vector<ossia::value>{1, 2, 3};

    auto form = formFor(ossia::val_type::LIST);
    REQUIRE(form != nullptr);
    form->setSettings(in);

    auto* line = form->findChild<QLineEdit*>();
    REQUIRE(line != nullptr);
    line->selectAll();
    score::test::keyClicks(*line, "5");

    CHECK(form->getSettings().value == in.value);
  });
}
