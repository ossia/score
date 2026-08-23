// The multi-line "pretty" rendering of values, as shown by the Value display
// process: nested structures spread over indented lines, flat ones kept
// readable on a single line.

#include <State/ValuePrettyPrint.hpp>

#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value.hpp>

#include <catch2/catch_all.hpp>

#include <string>
#include <vector>

namespace
{
std::string pretty(const ossia::value& v, State::PrettyPrintOptions o = {})
{
  std::string s;
  State::prettyPrintValue(s, v, 0, o);
  return s;
}

std::string single(const ossia::value& v)
{
  std::string s;
  State::printValue(s, v);
  return s;
}

ossia::value list(std::vector<ossia::value> v)
{
  return ossia::value{std::move(v)};
}

ossia::value ints(int from, int to)
{
  std::vector<ossia::value> v;
  for(int i = from; i < to; i++)
    v.push_back(i);
  return ossia::value{std::move(v)};
}

ossia::value map(std::initializer_list<std::pair<std::string, ossia::value>> kv)
{
  ossia::value_map_type m;
  for(auto& [k, v] : kv)
    m.emplace_back(k, v);
  return ossia::value{std::move(m)};
}
}

TEST_CASE("Pretty: scalars are the single-line tokens", "[state][pretty]")
{
  for(const ossia::value& v :
      {ossia::value{1}, ossia::value{2.5f}, ossia::value{true},
       ossia::value{std::string{"hello"}}, ossia::value{ossia::impulse{}},
       ossia::value{ossia::vec3f{1, 2, 3}}, ossia::value{}})
  {
    CHECK(pretty(v) == single(v));
    CHECK(pretty(v) == fmt::format("{}", v));
    CHECK(pretty(v).find('\n') == std::string::npos);
  }
}

TEST_CASE("Pretty: a flat list stays on one line", "[state][pretty]")
{
  const auto v = ints(0, 3);
  CHECK(pretty(v) == single(v));
  CHECK(pretty(v) == "list: [int: 0, int: 1, int: 2]");

  CHECK(pretty(list({})) == "list: []");
  CHECK(pretty(map({})) == "map: {}");
}

TEST_CASE("Pretty: a flat map stays on one line", "[state][pretty]")
{
  const auto v = map({{"a", 1}, {"b", 2.5f}});
  CHECK(pretty(v) == single(v));
  CHECK(pretty(v) == "map: {\"a\": int: 1, \"b\": float: 2.50}");
}

TEST_CASE("Pretty: a matrix is one row per line", "[state][pretty]")
{
  // [[0, 1, 2], [3, 4, 5]]: not one number per line, one sub-array per line.
  const auto v = list({ints(0, 3), ints(3, 6)});
  CHECK(
      pretty(v)
      == "list: [\n"
         "  list: [int: 0, int: 1, int: 2],\n"
         "  list: [int: 3, int: 4, int: 5]\n"
         "]");
}

TEST_CASE("Pretty: mixed scalars and containers spread out", "[state][pretty]")
{
  const auto v = list({1, ints(0, 2), std::string{"x"}});
  CHECK(
      pretty(v)
      == "list: [\n"
         "  int: 1,\n"
         "  list: [int: 0, int: 1],\n"
         "  string: \"x\"\n"
         "]");
}

TEST_CASE("Pretty: arbitrary nesting indents per level", "[state][pretty]")
{
  const auto v = map(
      {{"pos", ossia::vec2f{1, 2}},
       {"children", list({map({{"id", 1}, {"tags", list({std::string{"a"}})}}), ints(0, 2)})}});
  CHECK(
      pretty(v)
      == "map: {\n"
         "  \"pos\": vec2f: [1.00, 2.00],\n"
         "  \"children\": list: [\n"
         "    map: {\n"
         "      \"id\": int: 1,\n"
         "      \"tags\": list: [string: \"a\"]\n"
         "    },\n"
         "    list: [int: 0, int: 1]\n"
         "  ]\n"
         "}");

  // Deep nesting: every level one step further in, no limit
  ossia::value deep = ints(0, 1);
  for(int i = 0; i < 40; i++)
    deep = list({deep, 0});
  const auto s = pretty(deep);
  CHECK(s.find(std::string(80, ' ') + "list: [int: 0]") != std::string::npos);
  CHECK(s.find(std::string(82, ' ')) == std::string::npos);
}

TEST_CASE("Pretty: a long flat list wraps a few elements per line", "[state][pretty]")
{
  State::PrettyPrintOptions o;
  o.maxInlineElements = 4;
  o.elementsPerLine = 3;

  CHECK(pretty(ints(0, 4), o) == "list: [int: 0, int: 1, int: 2, int: 3]");
  CHECK(
      pretty(ints(0, 7), o)
      == "list: [\n"
         "  int: 0, int: 1, int: 2,\n"
         "  int: 3, int: 4, int: 5,\n"
         "  int: 6\n"
         "]");

  // Wrapped inside a nested structure: indentation follows the depth
  CHECK(
      pretty(list({ints(0, 5), 1}), o)
      == "list: [\n"
         "  list: [\n"
         "    int: 0, int: 1, int: 2,\n"
         "    int: 3, int: 4\n"
         "  ],\n"
         "  int: 1\n"
         "]");

  // Defaults: 16 fits, 17 wraps 8 per line
  CHECK(pretty(ints(0, 16)).find('\n') == std::string::npos);
  const auto s = pretty(ints(0, 17));
  CHECK(s.find('\n') != std::string::npos);
  CHECK(s.find("int: 7,\n  int: 8") != std::string::npos);
}

TEST_CASE("Pretty: indentation is configurable and starts at depth", "[state][pretty]")
{
  State::PrettyPrintOptions o;
  o.indent = 4;
  CHECK(
      pretty(list({ints(0, 1), ints(1, 2)}), o)
      == "list: [\n"
         "    list: [int: 0],\n"
         "    list: [int: 1]\n"
         "]");

  std::string s = "> ";
  State::prettyPrintValue(s, list({ints(0, 1), 2}), 1);
  CHECK(
      s
      == "> list: [\n"
         "    list: [int: 0],\n"
         "    int: 2\n"
         "  ]");
}

TEST_CASE("Pretty: appends to the buffer without clearing it", "[state][pretty]")
{
  std::string s = "first\n";
  State::prettyPrintValue(s, 1);
  s.push_back('\n');
  State::prettyPrintValue(s, ints(0, 2));
  CHECK(s == "first\nint: 1\nlist: [int: 0, int: 1]");

  // A reused buffer keeps its capacity
  s.clear();
  const auto cap = s.capacity();
  State::prettyPrintValue(s, ints(0, 2));
  CHECK(s.capacity() == cap);
}
