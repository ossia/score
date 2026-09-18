// Unit test: how Process::ComboBox and Process::Enum resolve an incoming value
// to one of their entries.
//
// An entry is addressed by its own value. When the entries are names, an
// integer addresses them by position instead -- nothing can be both, so the two
// readings never compete. Entries that are themselves numbers are matched by
// value only: several lists in score hold numbers that are not positions, and
// these tests pin each of them down.

#include <Process/Dataflow/WidgetInlets.hpp>

#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
using Alts = std::vector<std::pair<QString, ossia::value>>;

Process::ComboBox make_combo(QObject& parent, Alts a, ossia::value init)
{
  return Process::ComboBox{
      std::move(a), std::move(init), "combo", Id<Process::Port>{0}, &parent};
}

Process::Enum make_enum(QObject& parent, std::vector<std::string> names)
{
  return Process::Enum{names, {}, names.front(), "enum", Id<Process::Port>{0}, &parent};
}
}

// ---------------------------------------------------------------------------
// Name-valued lists: the folder-backed combobox, and anything whose entries are
// strings. A number cannot be one of those, so it reads as a position.

TEST_CASE("A combobox of names takes a value or a position", "[combobox][index]")
{
  QObject parent;
  auto combo = make_combo(
      parent,
      {{"kick.wav", std::string("kick.wav")},
       {"snare.wav", std::string("snare.wav")},
       {"hat.wav", std::string("hat.wav")}},
      std::string("kick.wav"));

  CHECK(combo.indexOfValue(std::string("snare.wav")) == 1);
  CHECK(combo.indexOfValue(2) == 2);
  CHECK(combo.indexOfValue(0) == 0);

  // Nothing selects nothing: no falling back to the first entry.
  CHECK(combo.indexOfValue(3) == -1);
  CHECK(combo.indexOfValue(-1) == -1);
  CHECK(combo.indexOfValue(std::string("tom.wav")) == -1);
  CHECK(combo.indexOfValue(ossia::value{}) == -1);

  CHECK(combo.valueAtIndex(1) == ossia::value{std::string("snare.wav")});
  CHECK_FALSE(combo.valueAtIndex(3).valid());
  CHECK_FALSE(combo.valueAtIndex(-1).valid());
}

// ---------------------------------------------------------------------------
// Number-valued lists. The value reading is the only one: reading a number as a
// position would pick a different entry, and there is no way to tell which the
// sender meant.

// Threedim's texture projections list all of 0..7, but not in that order.
TEST_CASE("A shuffled numeric list is addressed by value", "[combobox][index]")
{
  QObject parent;
  auto combo = make_combo(
      parent,
      {{"Texture coordinates", 0},
       {"Spherical", 2},
       {"View-space", 4},
       {"Barycentric", 5},
       {"Funky A", 1},
       {"Funky B", 3},
       {"Light", 6},
       {"Color", 7}},
      0);

  // 1 is "Funky A"'s value, not "Spherical"'s position.
  CHECK(combo.indexOfValue(1) == 4);
  CHECK(combo.indexOfValue(2) == 1);
  CHECK(combo.valueAtIndex(4) == ossia::value{1});
}

// The note-duration tables: floats, several of them inside [0, N).
TEST_CASE("A float-valued list is not truncated to a position", "[combobox][index]")
{
  QObject parent;
  auto combo = make_combo(
      parent,
      {{"Inf", -1.f}, {"Whole", 1.f}, {"Half", 0.5f}, {"4th", 0.25f}, {"None", 0.f}},
      1.f);

  CHECK(combo.indexOfValue(0.25f) == 3);
  CHECK(combo.indexOfValue(0.f) == 4);

  // A value between the declared ones is not silently rounded onto an entry.
  CHECK(combo.indexOfValue(0.3f) == -1);
  CHECK(combo.indexOfValue(2.f) == -1);
}

// A shader enumeration declaring its own VALUES, none of them a position.
TEST_CASE("A sparse integer list is addressed by value", "[combobox][index]")
{
  QObject parent;
  auto combo = make_combo(parent, {{"1", 1}, {"2", 2}, {"4", 4}, {"8", 8}}, 1);

  CHECK(combo.indexOfValue(4) == 2);
  CHECK(combo.indexOfValue(8) == 3);
  // 3 is nobody's value; it is not entry 3 either.
  CHECK(combo.indexOfValue(3) == -1);
}

// Deuterium's envelope source: two boolean entries, and ossia compares an
// integer against a boolean numerically.
TEST_CASE("A boolean list is addressed by value", "[combobox][index]")
{
  QObject parent;
  auto combo = make_combo(parent, {{"From file", true}, {"Custom", false}}, true);

  CHECK(combo.indexOfValue(true) == 0);
  CHECK(combo.indexOfValue(1) == 0);
  CHECK(combo.indexOfValue(0) == 1);
}

// ---------------------------------------------------------------------------
// Process::Enum. Its entries are always names, so a position always reads.

TEST_CASE("An enum takes a name or a position", "[enum][index]")
{
  QObject parent;
  auto e = make_enum(parent, {"Sine", "Square", "Saw"});

  CHECK(e.indexOfValue(std::string("Square")) == 1);
  CHECK(e.indexOfValue(2) == 2);
  CHECK(e.indexOfValue(std::string("Triangle")) == -1);
  CHECK(e.indexOfValue(3) == -1);

  CHECK(e.valueAtIndex(2) == ossia::value{std::string("Saw")});
  CHECK_FALSE(e.valueAtIndex(3).valid());
}

// The exec port is typed STRING, so an integer coming down a cable arrives as
// its digits and would otherwise match no name at all.
TEST_CASE("An enum reads the digits a coerced integer becomes", "[enum][index]")
{
  QObject parent;
  auto e = make_enum(parent, {"Sine", "Square", "Saw"});

  CHECK(e.indexOfValue(std::string("2")) == 2);
  CHECK(e.indexOfValue(std::string("99")) == -1);
  CHECK(e.indexOfValue(std::string("1.5")) == -1);
  CHECK(e.indexOfValue(std::string(" 1")) == -1);
}

// An entry may be named with digits; its own name wins over the position.
TEST_CASE("An enum named with digits matches by name first", "[enum][index]")
{
  QObject parent;
  auto e = make_enum(parent, {"2", "1", "0"});

  CHECK(e.indexOfValue(std::string("2")) == 0);
  CHECK(e.indexOfValue(std::string("0")) == 2);
}

// A float reaching an enum over the network is read like the same number
// spelled as a string: whole numbers in range, nothing else. Truncating
// instead would fold everything in (-1, 1) onto the first entry, and
// narrowing a huge or NaN float to int32_t is undefined.
TEST_CASE("An enum reads a float position exactly, or not at all", "[enum][index]")
{
  QObject parent;
  auto e = make_enum(parent, {"Sine", "Square", "Saw"});

  CHECK(e.indexOfValue(2.f) == 2);
  CHECK(e.indexOfValue(0.f) == 0);
  CHECK(e.indexOfValue(2.9f) == -1);
  CHECK(e.indexOfValue(-0.5f) == -1);
  CHECK(e.indexOfValue(-1.f) == -1);
  CHECK(e.indexOfValue(3.f) == -1);
  CHECK(e.indexOfValue(1e30f) == -1);
  CHECK(e.indexOfValue(std::numeric_limits<float>::quiet_NaN()) == -1);
  CHECK(e.indexOfValue(std::numeric_limits<float>::infinity()) == -1);
}

// Two invalid values compare equal, and convert<std::string>() turns an
// invalid one into the empty string, so without a guard an invalid value would
// select an entry rather than none.
TEST_CASE("An invalid value selects no entry", "[enum][index]")
{
  QObject parent;
  auto e = make_enum(parent, {"", "One", "Two"});

  CHECK(e.indexOfValue(ossia::value{}) == -1);
  CHECK(e.indexOfValue(std::string("")) == 0);
  CHECK(e.indexOfValue(std::string("Two")) == 2);
}
