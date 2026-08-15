// The one place where "how does one edit a value of this type" is answered:
// Explorer::make_value_widget, shared by the device explorer tree, the address
// panel and the address dialog.

#include <Device/Address/AddressSettings.hpp>

#include <Explorer/Explorer/ValueEditors.hpp>

#include <score/widgets/IntSlider.hpp>

#include <score_test/App.hpp>

#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QAbstractSlider>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

#include <catch2/catch_all.hpp>

#include <memory>

namespace
{
using namespace Explorer;

Device::AddressSettingsCommon param(ossia::value v)
{
  Device::AddressSettingsCommon s;
  s.value = std::move(v);
  s.ioType = ossia::access_mode::BI;
  return s;
}

struct Editor
{
  std::unique_ptr<AddressValueWidget> w;

  Editor(const Device::AddressSettingsCommon& s, ValueEditorSize size)
      : w{make_value_widget(s, nullptr, size)}
  {
  }

  explicit operator bool() const noexcept { return bool(w); }
  AddressValueWidget* operator->() const noexcept { return w.get(); }

  template <typename T>
  int countOf() const
  {
    return w ? w->findChildren<T>().size() : 0;
  }
};

// What the editor gives back after being shown a value.
ossia::value roundtrip(
    const Device::AddressSettingsCommon& s, ossia::value v,
    ValueEditorSize size = ValueEditorSize::Full)
{
  Editor e{s, size};
  REQUIRE(e);
  e->set(v);
  return e->get();
}
}

TEST_CASE("a vec parameter is edited component by component", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto s = param(ossia::vec3f{{0.f, 0.f, 0.f}});

    // Regression: with no editor for it, the tree opened an empty line edit and
    // committing wrote [0, 0, 0] over the parameter.
    Editor e{s, ValueEditorSize::Compact};
    REQUIRE(e);
    CHECK(e.countOf<QDoubleSpinBox*>() == 3);

    const auto out = roundtrip(s, ossia::vec3f{{1.f, -2.f, 3.5f}});
    REQUIRE(out.get_type() == ossia::val_type::VEC3F);
    const auto v = *out.target<ossia::vec3f>();
    CHECK(v[0] == Catch::Approx(1.f));
    CHECK(v[1] == Catch::Approx(-2.f));
    CHECK(v[2] == Catch::Approx(3.5f));
  });
}

TEST_CASE("vec2 and vec4 get as many fields as they have components",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    CHECK(Editor{param(ossia::vec2f{}), ValueEditorSize::Compact}
              .countOf<QDoubleSpinBox*>()
          == 2);
    CHECK(Editor{param(ossia::vec4f{}), ValueEditorSize::Compact}
              .countOf<QDoubleSpinBox*>()
          == 4);
  });
}

TEST_CASE("a bounded number gets a slider only where there is room",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(5);
    s.domain = ossia::make_domain(0, 10);

    // A row of the tree is one line tall: the slider is dropped, the field stays.
    Editor compact{s, ValueEditorSize::Compact};
    REQUIRE(compact);
    CHECK(compact.countOf<QSpinBox*>() == 1);
    CHECK(compact.countOf<score::IntSlider*>() == 0);

    Editor full{s, ValueEditorSize::Full};
    REQUIRE(full);
    CHECK(full.countOf<score::IntSlider*>() == 1);

    CHECK(roundtrip(s, 7, ValueEditorSize::Full) == ossia::value{7});
    CHECK(roundtrip(s, 7, ValueEditorSize::Compact) == ossia::value{7});

    // A value the domain does not cover is shown as it is rather than clamped:
    // an editor must never change a value just by displaying it.
    CHECK(roundtrip(s, 42, ValueEditorSize::Compact) == ossia::value{42});
    CHECK(roundtrip(s, -42, ValueEditorSize::Compact) == ossia::value{-42});
  });
}

TEST_CASE("an unbounded float keeps its precision", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto s = param(0.f);
    const auto out = roundtrip(s, 1234.5f, ValueEditorSize::Compact);
    REQUIRE(out.get_type() == ossia::val_type::FLOAT);
    CHECK(*out.target<float>() == Catch::Approx(1234.5f));
  });
}

TEST_CASE("an impulse gets something to press", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Editor e{param(ossia::impulse{}), ValueEditorSize::Compact};
    REQUIRE(e);
    REQUIRE(e.countOf<QPushButton*>() == 1);

    // It acts instead of holding a value, so it commits as soon as it is used.
    CHECK(e->commitsImmediately());

    int sent{};
    QObject::connect(
        e.w.get(), &AddressValueWidget::changed, e.w.get(),
        [&](const ossia::value& v) {
      if(v.get_type() == ossia::val_type::IMPULSE)
        sent++;
        });

    e.w->findChild<QPushButton*>()->click();
    CHECK(sent == 1);
    CHECK(e->get().get_type() == ossia::val_type::IMPULSE);
  });
}

TEST_CASE("a colour parameter gets a colour picker", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(ossia::vec4f{{0.f, 0.f, 0.f, 1.f}});
    s.unit = ossia::unit_t{ossia::rgba_u{}};

    Editor e{s, ValueEditorSize::Compact};
    REQUIRE(e);
    CHECK(e.countOf<QPushButton*>() == 1);

    const auto out = roundtrip(s, ossia::vec4f{{1.f, 0.5f, 0.f, 0.25f}});
    REQUIRE(out.get_type() == ossia::val_type::VEC4F);
    const auto v = *out.target<ossia::vec4f>();
    CHECK(v[0] == Catch::Approx(1.f).margin(0.01));
    CHECK(v[1] == Catch::Approx(0.5f).margin(0.01));
    CHECK(v[2] == Catch::Approx(0.f).margin(0.01));
    CHECK(v[3] == Catch::Approx(0.25f).margin(0.01));
  });
}

TEST_CASE("a 2-D position gets a pad to drag in", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(ossia::vec2f{{0.f, 0.f}});
    s.unit = ossia::unit_t{ossia::cartesian_2d_u{}};

    const auto out = roundtrip(s, ossia::vec2f{{0.25f, 0.75f}}, ValueEditorSize::Full);
    REQUIRE(out.get_type() == ossia::val_type::VEC2F);
    const auto v = *out.target<ossia::vec2f>();
    CHECK(v[0] == Catch::Approx(0.25f));
    CHECK(v[1] == Catch::Approx(0.75f));

    // In a tree row there is no room for a pad; the components still are.
    Editor compact{s, ValueEditorSize::Compact};
    REQUIRE(compact);
    CHECK(compact.countOf<QDoubleSpinBox*>() == 2);
  });
}

TEST_CASE("composite values round-trip through their text", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto lst = ossia::value{std::vector<ossia::value>{1, 2, 3}};
    CHECK(roundtrip(param(lst), lst) == lst);

    const auto map = ossia::value{
        ossia::value_map_type{{"a", ossia::value{1}}, {"b", ossia::value{2}}}};
    CHECK(roundtrip(param(map), map) == map);
  });
}

TEST_CASE("unparseable text leaves the value alone", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto lst = ossia::value{std::vector<ossia::value>{1, 2, 3}};
    Editor e{param(lst), ValueEditorSize::Full};
    REQUIRE(e);
    e->set(lst);

    auto* line = e.w->findChild<QLineEdit*>();
    REQUIRE(line != nullptr);
    line->setText("not a value at all");

    // Rather than replacing the list with an empty one.
    CHECK(e->get() == lst);
  });
}

TEST_CASE("an enumerated parameter gets the editable combo box",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(std::string{"a"});
    s.domain = ossia::make_domain(
        std::vector<ossia::value>{std::string{"a"}, std::string{"b"}});

    CHECK(hasValueList(s));

    Editor e{s, ValueEditorSize::Compact};
    REQUIRE(e);
    auto* cb = e.w->findChild<QComboBox*>();
    REQUIRE(cb != nullptr);
    REQUIRE(cb->count() == 2);
    CHECK(cb->isEditable());

    e->set(ossia::value{std::string{"b"}});
    CHECK(cb->currentIndex() == 1);
    CHECK(e->get() == ossia::value{std::string{"b"}});
  });
}

TEST_CASE("a domain bound is edited as the parameter's type, unbounded",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(5);
    s.domain = ossia::make_domain(0, 10);

    std::unique_ptr<AddressValueWidget> w{make_bound_widget(s, nullptr)};
    REQUIRE(w);

    // A bound is not itself confined to the domain it describes.
    w->set(ossia::value{9999});
    CHECK(w->get() == ossia::value{9999});
  });
}

// Coverage: a value type without an editor is a type the user cannot change.
TEST_CASE("every value type has an editor", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    constexpr std::array<ossia::val_type, 10> allTypes{
        ossia::val_type::FLOAT,  ossia::val_type::INT,    ossia::val_type::VEC2F,
        ossia::val_type::VEC3F,  ossia::val_type::VEC4F,  ossia::val_type::IMPULSE,
        ossia::val_type::BOOL,   ossia::val_type::STRING, ossia::val_type::LIST,
        ossia::val_type::MAP};

    for(auto t : allTypes)
    {
      INFO("val_type " << (int)t);
      const auto s = param(ossia::init_value(t));
      REQUIRE(s.value.get_type() == t);

      for(auto size : {ValueEditorSize::Compact, ValueEditorSize::Full})
      {
        Editor e{s, size};
        REQUIRE(e);

        // ... and it gives back a value of that same type.
        e->set(s.value);
        CHECK(e->get().get_type() == t);
      }
    }

    // A parameter that holds nothing has nothing to edit.
    Device::AddressSettingsCommon none;
    CHECK(make_value_widget(none, nullptr) == nullptr);
  });
}

TEST_CASE("a boolean and a string are editors of their own", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    CHECK(roundtrip(param(false), ossia::value{true}) == ossia::value{true});

    // A string that looks like a list is still a string.
    const auto s = ossia::value{std::string{"[1, 2]"}};
    CHECK(roundtrip(param(std::string{}), s) == s);
  });
}

TEST_CASE("a vec with a per-component domain keeps its components",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(ossia::vec3f{{0.f, 0.f, 0.f}});
    ossia::vecf_domain<3> dom;
    dom.min = {0.f, -10.f, 0.f};
    dom.max = {1.f, 10.f, 360.f};
    s.domain = ossia::domain{dom};

    // What the domain actually reports, so that the editor choice is checked
    // against the real shape and not an assumed one.
    CHECK(ossia::get_min(s.domain.get()).get_type() == ossia::val_type::VEC3F);
    CHECK_FALSE(hasValueList(s));

    const auto out = roundtrip(s, ossia::vec3f{{0.5f, -5.f, 180.f}});
    REQUIRE(out.get_type() == ossia::val_type::VEC3F);
    const auto v = *out.target<ossia::vec3f>();
    CHECK(v[0] == Catch::Approx(0.5f));
    CHECK(v[1] == Catch::Approx(-5.f));
    CHECK(v[2] == Catch::Approx(180.f));
  });
}

// An editor that was only opened and closed must write nothing back: the
// delegates ask it whether the user did anything.
TEST_CASE("an untouched editor reports no edit", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto bounded = param(5);
    bounded.domain = ossia::make_domain(0, 10);

    auto enumerated = param(std::string{"a"});
    enumerated.domain = ossia::make_domain(
        std::vector<ossia::value>{std::string{"a"}, std::string{"b"}});

    for(const auto& s : {param(0.f), param(ossia::vec3f{}), bounded, enumerated,
                         param(std::string{"x"}), param(false)})
    {
      for(auto size : {ValueEditorSize::Compact, ValueEditorSize::Full})
      {
        Editor e{s, size};
        REQUIRE(e);
        e->set(s.value);
        CHECK_FALSE(e->edited());
      }
    }

    // ... and reports one as soon as it is used.
    Editor e{param(0.f), ValueEditorSize::Compact};
    REQUIRE(e);
    e->set(ossia::value{1.f});
    REQUIRE_FALSE(e->edited());
    e.w->findChild<QDoubleSpinBox*>()->setValue(2.);
    CHECK(e->edited());
  });
}

// A bound row on a parameter with no domain used to show 0 and commit it,
// inventing a domain the parameter never had.
TEST_CASE("a bound editor on an unbounded parameter reports no edit",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto s = param(0.f);
    std::unique_ptr<AddressValueWidget> w{make_bound_widget(s, nullptr)};
    REQUIRE(w);
    w->set(ossia::get_min(s.domain.get()));
    CHECK_FALSE(w->edited());
  });
}

// State::convert::convert reports success for anything, yielding 0 from
// garbage: the free-value path cannot lean on it.
TEST_CASE("free text that is not of the enumeration's type is refused",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(1);
    s.domain = ossia::make_domain(std::vector<ossia::value>{1, 2, 3});

    Editor e{s, ValueEditorSize::Compact};
    REQUIRE(e);
    auto* cb = e.w->findChild<QComboBox*>();
    REQUIRE(cb != nullptr);

    cb->setCurrentText("12");
    CHECK(e->get() == ossia::value{12});

    cb->setCurrentText("auto");
    CHECK_FALSE(e->get().valid());
  });
}

// A colour picker speaks [0; 1] RGB(A); the other colour spaces do not.
TEST_CASE("only rgb colours get the colour picker", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto rgba = param(ossia::vec4f{{0.f, 0.f, 0.f, 1.f}});
    rgba.unit = ossia::unit_t{ossia::rgba_u{}};
    CHECK(Editor{rgba, ValueEditorSize::Full}.countOf<QPushButton*>() == 1);

    // 0..255, and the alpha is component 0: not what QColor::fromRgbF takes.
    auto argb8 = param(ossia::vec4f{{255.f, 128.f, 64.f, 255.f}});
    argb8.unit = ossia::unit_t{ossia::argb8_u{}};

    Editor e{argb8, ValueEditorSize::Full};
    REQUIRE(e);
    CHECK(e.countOf<QPushButton*>() == 0);
    CHECK(e.countOf<QDoubleSpinBox*>() == 4);

    const auto out = roundtrip(argb8, ossia::vec4f{{255.f, 128.f, 64.f, 255.f}});
    const auto v = *out.target<ossia::vec4f>();
    CHECK(v[1] == Catch::Approx(128.f));
  });
}

// cartesian_2d runs over [-1; 1], not [0; 1].
TEST_CASE("the position pad covers the whole cartesian range",
          "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(ossia::vec2f{{0.f, 0.f}});
    s.unit = ossia::unit_t{ossia::cartesian_2d_u{}};

    const auto out = roundtrip(s, ossia::vec2f{{-0.5f, 0.3f}}, ValueEditorSize::Full);
    const auto v = *out.target<ossia::vec2f>();
    CHECK(v[0] == Catch::Approx(-0.5f));
    CHECK(v[1] == Catch::Approx(0.3f));
  });
}

// ossia reports {false, true} for a bool domain that nobody filled in.
TEST_CASE("a bool is not an enumeration", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto s = param(false);
    s.domain = ossia::init_domain(ossia::val_type::BOOL);

    CHECK_FALSE(hasValueList(s));
    Editor e{s, ValueEditorSize::Compact};
    REQUIRE(e);
    CHECK(e->get().get_type() == ossia::val_type::BOOL);
  });
}

// State::parseValue has no rule for the map form its own printer emits.
TEST_CASE("a map can be typed in", "[explorer][editors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto map = ossia::value{
        ossia::value_map_type{{"a", ossia::value{1}}, {"b", ossia::value{2}}}};

    Editor e{param(map), ValueEditorSize::Full};
    REQUIRE(e);
    e->set(map);

    auto* line = e.w->findChild<QLineEdit*>();
    REQUIRE(line != nullptr);
    line->setText(R"({"x": 4, "y": [1, 2]})");

    const auto out = e->get();
    REQUIRE(out.get_type() == ossia::val_type::MAP);
    const auto& m = *out.target<ossia::value_map_type>();
    REQUIRE(m.size() == 2);
    auto it = m.begin();
    CHECK(it->first == "x");
    CHECK(it->second == ossia::value{4});

    // Nonsense keeps the previous value rather than emptying it.
    line->setText("not a map");
    CHECK(e->get() == map);
  });
}
