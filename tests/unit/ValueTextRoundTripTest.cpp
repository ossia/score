// The textual form of a value has to read back as the same value: it is what
// the list / map editors are, what Copy puts on the clipboard, and what a
// string carries once it has a newline in it.

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <catch2/catch_all.hpp>

#include <string>
#include <vector>

namespace
{
ossia::value roundtrip(const ossia::value& v)
{
  const auto text = State::convert::toPrettyString(v);
  INFO(text.toStdString());
  auto back = State::parseValue(text.toStdString());
  REQUIRE(back.has_value());
  return *back;
}

const std::vector<std::string>& awkwardStrings()
{
  static const std::vector<std::string> s{
      "plain",
      "",
      " leading and trailing ",
      R"(with "quotes")",
      R"(back\slash)",
      R"(\")",
      "two\nlines",
      "tab\there",
      "carriage\rreturn",
      "everything: \\ \" \n \r \t",
      "/an/osc/address"};
  return s;
}
}

TEST_CASE("escapeStringLiteral", "[state][value][text]")
{
  using State::convert::escapeStringLiteral;
  CHECK(escapeStringLiteral("plain") == "plain");
  CHECK(escapeStringLiteral("a\\b") == "a\\\\b");
  CHECK(escapeStringLiteral("a\"b") == "a\\\"b");
  CHECK(escapeStringLiteral("a\nb") == "a\\nb");
  CHECK(escapeStringLiteral("a\rb") == "a\\rb");
  CHECK(escapeStringLiteral("a\tb") == "a\\tb");
}

TEST_CASE("a string round-trips through its printed form", "[state][value][text]")
{
  for(const auto& s : awkwardStrings())
  {
    INFO(s);
    CHECK(roundtrip(ossia::value{s}) == ossia::value{s});
  }
}

TEST_CASE("a string nested in a list round-trips", "[state][value][text]")
{
  for(const auto& s : awkwardStrings())
  {
    INFO(s);
    const ossia::value v{std::vector<ossia::value>{1, s, 2.5f, s}};
    CHECK(roundtrip(v) == v);
  }
}

// The rows the editors live in are one line tall, so the printed form of a
// container has to stay on one line however its elements are spelled.
TEST_CASE("a multi-line string survives being a list element", "[state][value][text]")
{
  const ossia::value v{
      std::vector<ossia::value>{std::string{"first\nsecond"}, std::string{"third"}}};

  const auto text = State::convert::toPrettyString(v);
  CHECK_FALSE(text.contains('\n'));
  CHECK(roundtrip(v) == v);
}

TEST_CASE("toSingleLine says what it folded away", "[state][value][text]")
{
  using State::convert::isMultiLine;
  using State::convert::toSingleLine;

  CHECK_FALSE(isMultiLine("one line"));
  CHECK(toSingleLine("one line") == "one line");

  CHECK(isMultiLine("a\nb"));
  CHECK(toSingleLine("a\nb").startsWith("a"));
  CHECK_FALSE(toSingleLine("a\nb").contains('\n'));
  CHECK(toSingleLine("a\nb").contains("[+1 line]"));
  CHECK(toSingleLine("a\nb\nc").contains("[+2 lines]"));

  // No symbols: the marker is words in brackets, drawn italic by the delegate.
  CHECK_FALSE(toSingleLine("a\nb").contains(QChar(0x23CE)));

  // ... and the delegate gets the two halves apart to draw them differently.
  const auto split = State::convert::splitSingleLine("a\nb\nc");
  CHECK(split.head == "a");
  CHECK(split.marker == "[+2 lines]");
  CHECK(State::convert::splitSingleLine("plain").marker.isEmpty());

  // CRLF is one line ending, not two.
  CHECK(toSingleLine("a\r\nb") == toSingleLine("a\nb"));

  // A trailing break hides no line, but it is still a break the cell cannot
  // show -- and the field goes read-only over it, so it has to be marked.
  CHECK(toSingleLine("a\n").startsWith("a"));
  CHECK(toSingleLine("a\n") != "a");
  CHECK_FALSE(toSingleLine("a\n").contains('\n'));
}

// A lone backslash is not one of the printer's escapes; text written by hand
// (a Windows path in an expression) must still read rather than fail to parse.
TEST_CASE("an unknown escape is taken literally", "[state][value][text]")
{
  auto parse = [](const char* s) { return State::parseValue(s); };

  auto v = parse(R"("C:\dir\file")");
  REQUIRE(v.has_value());
  CHECK(*v == ossia::value{std::string{R"(C:\dir\file)"}});

  // ... while the escapes the printer does write keep their meaning.
  auto e = parse(R"("a\nb\\c")");
  REQUIRE(e.has_value());
  CHECK(*e == ossia::value{std::string{"a\nb\\c"}});
}

// The printer used to decode keys as Latin-1 while every reader treats them as
// UTF-8, so a non-ASCII key came back as mojibake. The grammar has no rule for
// a map either, so this went nowhere near the parser until parseMap.
TEST_CASE("a map round-trips, keys and all", "[state][value][text]")
{
  const ossia::value v{ossia::value_map_type{
      {"clé", ossia::value{1}},
      {"a key with spaces", ossia::value{std::string{"and, a comma"}}},
      {"nested", ossia::value{std::vector<ossia::value>{1, 2}}}}};

  const auto text = State::convert::toPrettyString(v);
  INFO(text.toStdString());
  CHECK(text.contains("clé"));

  const auto back = State::parseValue(text.toStdString());
  REQUIRE(back.has_value());

  const auto* m = back->target<ossia::value_map_type>();
  REQUIRE(m != nullptr);
  REQUIRE(m->size() == 3);

  auto entry = [&](const char* key) -> const ossia::value* {
    for(const auto& [k, v] : *m)
      if(k == key)
        return &v;
    return nullptr;
  };

  REQUIRE(entry("clé") != nullptr);
  CHECK(*entry("clé") == ossia::value{1});

  // A comma inside a quoted value is not an entry separator.
  REQUIRE(entry("a key with spaces") != nullptr);
  CHECK(*entry("a key with spaces") == ossia::value{std::string{"and, a comma"}});

  CHECK(entry("nested") != nullptr);
}

// A map *inside* a list does not read back: the list rule is the grammar.s,
// and the grammar has no rule for a map -- parseValue only reaches parseMap
// for one that starts the input. Pinned so the limit is known rather than
// discovered.
TEST_CASE("a map nested in a list is a known gap", "[state][value][text]")
{
  const ossia::value v{std::vector<ossia::value>{
      ossia::value{1}, ossia::value{ossia::value_map_type{{"k", ossia::value{2}}}}}};

  const auto text = State::convert::toPrettyString(v);
  INFO(text.toStdString());
  CHECK(text.contains("{"));

  CHECK_FALSE(State::parseValue(text.toStdString()).has_value());
}

// Text that names no value must not parse as the part of it that does.
TEST_CASE("trailing junk is not a value", "[state][value][text]")
{
  CHECK_FALSE(State::parseValue(R"("abc" junk)").has_value());
  CHECK_FALSE(State::parseValue("12 34").has_value());
  CHECK_FALSE(State::parseValue("[1, 2] tail").has_value());

  // Surrounding space is not junk.
  CHECK(State::parseValue("  12  ").has_value());
}

// ossia's STRING is a std::string, so a device is free to put bytes that are
// not text in one. Decoding those as UTF-8 to show them replaces every bad
// byte with U+FFFD, and committing writes the replacements back.
TEST_CASE("binary strings are recognised as such", "[state][value][text]")
{
  using State::convert::isBinary;

  CHECK_FALSE(isBinary(QByteArray{"plain text"}));
  CHECK_FALSE(isBinary(QByteArray{"two\nlines\twith\ttabs"}));
  CHECK_FALSE(isBinary(QByteArray::fromStdString("accentué")));

  // A PNG header: invalid UTF-8 and full of control bytes.
  CHECK(isBinary(QByteArray::fromHex("89504e470d0a1a0a")));
  // A lone NUL is enough.
  CHECK(isBinary(QByteArray{"a\0b", 3}));
  // Valid UTF-8 but not text.
  CHECK(isBinary(QByteArray{"\x01\x02\x03", 3}));
}

// A one-line cell has to summarise without decoding what it cannot show, and
// the marker it appends has to be findable again by the delegate that paints
// it -- the models store the *collapsed* form, so that is what splitSingleLine
// is handed.
TEST_CASE("what a one-line cell shows", "[state][value][text]")
{
  using namespace State::convert;

  SECTION("text that fits is itself, with no marker")
  {
    CHECK(stringCellText("hello") == "hello");
    CHECK(stringCellToolTip("hello").isEmpty());
    CHECK(splitSingleLine("hello").marker.isEmpty());
  }

  SECTION("text that does not fit says how much is missing")
  {
    const auto cell = stringCellText("one\ntwo\nthree");
    CHECK(cell.startsWith("one"));
    CHECK(cell.contains("2"));

    // The delegate is handed the collapsed cell, not the original.
    const auto split = splitSingleLine(cell);
    CHECK(split.head == "one");
    CHECK_FALSE(split.marker.isEmpty());
    CHECK(split.marker.startsWith('['));

    // And the tooltip carries the whole of it.
    CHECK(stringCellToolTip("one\ntwo\nthree") == "one\ntwo\nthree");
  }

  SECTION("bytes that are not text are never decoded")
  {
    const auto png = QByteArray::fromHex("89504e470d0a1a0a0000000d49484452");

    const auto cell = stringCellText(png);
    CHECK(cell.startsWith("89 50 4e"));
    CHECK(cell.contains("16"));
    CHECK_FALSE(cell.contains(QChar{QChar::ReplacementCharacter}));

    // A blob can be megabytes: the tooltip is not the place for it.
    CHECK(stringCellToolTip(png).isEmpty());

    // Even one holding a line break, which used to make it look like prose.
    auto withBreak = png;
    withBreak.append('\n');
    CHECK(stringCellToolTip(withBreak).isEmpty());
  }

  SECTION("a value cannot forge the marker")
  {
    // The head is concatenated, not substituted: a "%2" in the value used to
    // rewrite the count.
    const auto cell = stringCellText("a %2 b\nc\nd");
    CHECK(cell.startsWith("a %2 b"));
    CHECK(cell.contains("2"));
    CHECK(splitSingleLine(cell).head == "a %2 b");
  }

  SECTION("a trailing line break is not a hidden line")
  {
    const auto cell = stringCellText("a\n");
    CHECK(cell.startsWith("a"));
    CHECK_FALSE(cell.contains("1 line"));
    CHECK_FALSE(splitSingleLine(cell).marker.isEmpty());
  }
}

// "1" is an int. A whole float printed that way came back as one, and in a
// list the changed element type was committed to the device.
TEST_CASE("a whole float stays a float", "[state][value][text]")
{
  for(float f : {1.f, 0.f, -3.f, 2.5f, 1e-4f})
  {
    INFO(f);
    const auto back = roundtrip(ossia::value{f});
    CHECK(back.get_type() == ossia::val_type::FLOAT);
    CHECK(back == ossia::value{f});
  }

  // Including as an element, where the type change used to go unnoticed: the
  // reader checks the type of the list, not of what is in it.
  const ossia::value v{std::vector<ossia::value>{1.f, 2.5f}};
  const auto back = roundtrip(v);
  const auto* l = back.target<std::vector<ossia::value>>();
  REQUIRE(l != nullptr);
  REQUIRE(l->size() == 2);
  CHECK((*l)[0].get_type() == ossia::val_type::FLOAT);

  // An int is still an int.
  CHECK(roundtrip(ossia::value{3}).get_type() == ossia::val_type::INT);
}
