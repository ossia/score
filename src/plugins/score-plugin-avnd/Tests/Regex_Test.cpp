#include <ossia/dataflow/execution_state.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <AvndProcesses/Regex.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
using list = std::vector<ossia::value>;

struct RegexFixture
{
  ao::Regex object;
  ossia::execution_state state;
  ossia::value_port input, match, groups, matched, unmatched, error;
  std::vector<ossia::value_port> captures;

  RegexFixture()
  {
    state.bufferSize = 4096;
    state.modelToSamplesRatio = 1.;
    state.samplesToModelRatio = 1.;
    object.ossia_state = {&state};
    object.inputs.input.value = &input;
    object.outputs.match.value = &match;
    object.outputs.groups.value = &groups;
    object.outputs.matched.value = &matched;
    object.outputs.unmatched.value = &unmatched;
    object.outputs.error.value = &error;
  }

  //! What the host does on an edit of the pattern: the rows, then the ports.
  halp::string_list_value pattern(std::string p, ao::Regex::Mode mode = ao::Regex::Search)
  {
    object.inputs.mode.value = mode;
    object.inputs.pattern.value = std::move(p);
    halp::string_list_value rows;
    object.outputs.captures.request_port_rows
        = [&](const halp::string_list_value& r) { rows = r; };
    decltype(object.inputs.pattern)::on_controller_interaction()(
        object, object.inputs.pattern.value);
    object.inputs.pattern.update(object);
    captures.resize(rows.size());
    object.outputs.captures.ports.resize(rows.size());
    for(std::size_t i = 0; i < rows.size(); ++i)
      object.outputs.captures.ports[i].value = &captures[i];
    return rows;
  }

  void run(int start = 0, int frames = 256)
  {
    for(auto* p : {&match, &groups, &matched, &unmatched, &error})
      p->get_data().clear();
    for(auto& p : captures)
      p.get_data().clear();
    ao::Regex::tick t;
    t.prev_date = ossia::time_value{start};
    t.date = ossia::time_value{start + frames};
    t.start_sample = start;
    t.length_sample = frames;
    object(t);
    input.get_data().clear();
  }

  static std::vector<ossia::value> values(ossia::value_port& p)
  {
    std::vector<ossia::value> v;
    for(auto& e : p.get_data())
      v.push_back(e.value);
    return v;
  }
};
}

TEST_CASE("Regex: search, groups on their outlets, matched and unmatched", "[avnd][regex]")
{
  RegexFixture f;
  auto rows = f.pattern(R"((\w+)=(\d+))");
  REQUIRE(rows.size() == 2);
  f.input.write_value(std::string{"set x=42 now"}, 3);
  f.input.write_value(std::string{"nothing"}, 5);
  f.run();

  CHECK(f.values(f.match) == list{std::string{"x=42"}});
  CHECK(f.values(f.groups) == list{list{std::string{"x"}, std::string{"42"}}});
  CHECK(f.values(f.captures[0]) == list{std::string{"x"}});
  CHECK(f.values(f.captures[1]) == list{std::string{"42"}});
  CHECK(f.captures[0].get_data()[0].timestamp == 3);
  CHECK(f.values(f.matched) == list{true, false});
  CHECK(f.values(f.unmatched) == list{std::string{"nothing"}});
}

TEST_CASE("Regex: Match needs the whole input", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern(R"(\d+)", ao::Regex::Match);
  f.input.write_value(std::string{"123"}, 0);
  f.input.write_value(std::string{"a123"}, 1);
  f.run();
  CHECK(f.values(f.matched) == list{true, false});
}

TEST_CASE("Regex: search all, one value per match on each group outlet", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern(R"((\d+))", ao::Regex::SearchAll);
  f.input.write_value(std::string{"1 22 333"}, 0);
  f.run();
  CHECK(
      f.values(f.match)
      == list{list{std::string{"1"}, std::string{"22"}, std::string{"333"}}});
  CHECK(
      f.values(f.captures[0])
      == list{std::string{"1"}, std::string{"22"}, std::string{"333"}});
}

TEST_CASE("Regex: replace, first or all, with \\1, $1 or %1", "[avnd][regex]")
{
  RegexFixture f;
  f.object.inputs.replacement.value = "<$1>";
  f.pattern(R"((\d))", ao::Regex::Replace);
  f.input.write_value(std::string{"a1b2"}, 0);
  f.run();
  CHECK(f.values(f.match) == list{std::string{"a<1>b<2>"}});

  f.object.inputs.global.value = false;
  f.object.inputs.replacement.value = "[%1]";
  f.object.inputs.replacement.update(f.object);
  f.input.write_value(std::string{"a1b2"}, 0);
  f.run();
  CHECK(f.values(f.match) == list{std::string{"a[1]b2"}});

  // A replacement naming a group the pattern has not: an error, no output
  f.object.inputs.replacement.value = R"(\3)";
  f.object.inputs.replacement.update(f.object);
  f.input.write_value(std::string{"a1"}, 0);
  f.run();
  CHECK(f.values(f.match).empty());
  REQUIRE(f.error.get_data().size() == 1);
  CHECK(!ossia::convert<std::string>(f.error.get_data()[0].value).empty());
}

TEST_CASE("Regex: split", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern(R"(\s*,\s*)", ao::Regex::Split);
  f.input.write_value(std::string{"a, b ,c,"}, 0);
  f.run();
  CHECK(
      f.values(f.match)
      == list{list{std::string{"a"}, std::string{"b"}, std::string{"c"}, std::string{}}});
}

TEST_CASE("Regex: named groups name their outlets, keys follow the names", "[avnd][regex]")
{
  RegexFixture f;
  auto a = f.pattern(R"((?P<temp>\d+);(\d+))");
  REQUIRE(a.size() == 2);
  CHECK(a[0].second == "temp");
  CHECK(a[1].second.empty()); // "Group {}"
  CHECK(a[1].first == 10002);

  // "temp" moves: same key. The unnamed group is now number 1.
  auto b = f.pattern(R"((\d+);(?P<temp>\d+))");
  CHECK(b[1].first == a[0].first);
  CHECK(b[0].first == 10001);

  // Appending a group keeps the others
  auto c = f.pattern(R"((?P<temp>\d+);(\d+);(\d+))");
  CHECK(c[0].first == a[0].first);
  CHECK(c[1].first == a[1].first);
  CHECK(c.size() == 3);
}

TEST_CASE("Regex: an invalid pattern keeps the outlets and says why", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern(R"((a)(b))");
  REQUIRE(f.captures.size() == 2);
  f.run();
  CHECK(f.error.get_data().empty()); // valid from the start: nothing to say

  // A typo (the last parenthesis): the same groups, read from the text, so
  // the same outlets and cables.
  const auto valid = ao::Regex::captureRows(R"re((a)(?P<b>b))re");
  CHECK(ao::Regex::captureRows(R"re((a)(?P<b>b)re") == valid);
  CHECK(ao::Regex::captureRows(R"re((a)(?P<b>b)[)re") == valid);
  CHECK(ao::Regex::captureRows(R"re((?:x)(?i)\(a)[(](a)(?P<b>b)))re").size() == 2);
  f.pattern("(a");
  f.input.write_value(std::string{"ab"}, 0);
  f.run();
  CHECK(f.values(f.matched).empty());
  REQUIRE(f.error.get_data().size() == 1);
  CHECK(!ossia::convert<std::string>(f.error.get_data()[0].value).empty());
  f.run();
  CHECK(f.error.get_data().empty()); // said once

  f.pattern(R"((a)(b))");
  f.run();
  CHECK(f.values(f.error) == list{std::string{}});
  CHECK(f.values(f.matched).empty());
}

TEST_CASE("Regex: numbers and lists are matched as text; impulse repeats", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern(R"(^(\S+) (\S+))");
  f.input.write_value(list{1, 2.5f, std::string{"x"}}, 0);
  f.run();
  CHECK(f.values(f.match) == list{std::string{"1 2.5"}});

  f.pattern(R"(4(\d))");
  f.input.write_value(42, 0);
  f.run();
  CHECK(f.values(f.captures[0]) == list{std::string{"2"}});
  f.input.write_value(ossia::impulse{}, 1);
  f.run();
  CHECK(f.values(f.captures[0]) == list{std::string{"2"}});
}

TEST_CASE("Regex: Numbers sends numeric groups as numbers", "[avnd][regex]")
{
  RegexFixture f;
  f.object.inputs.numbers.value = true;
  f.pattern(R"(T=(\S+);H=(\S+);N=(\S+))");
  f.input.write_value(std::string{"T=23.5;H=40;N=abc"}, 0);
  f.run();
  CHECK(f.values(f.captures[0]) == list{23.5f});
  CHECK(f.values(f.captures[1]) == list{40});
  CHECK(f.values(f.captures[2]) == list{std::string{"abc"}});
}

TEST_CASE("Regex: ignore case and multiline", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern("^abc$");
  f.input.write_value(std::string{"x\nABC\ny"}, 0);
  f.run();
  CHECK(f.values(f.matched) == list{false});

  f.object.inputs.ignore_case.value = true;
  f.object.inputs.multiline.value = true;
  f.object.inputs.multiline.update(f.object);
  f.input.write_value(std::string{"x\nABC\ny"}, 0);
  f.run();
  CHECK(f.values(f.matched) == list{true});
}

TEST_CASE("Regex: a pathological pattern stays linear", "[avnd][regex]")
{
  RegexFixture f;
  f.pattern("(a*)*b");
  f.input.write_value(std::string(20000, 'a'), 0);
  const auto t0 = std::chrono::steady_clock::now();
  f.run();
  const auto t1 = std::chrono::steady_clock::now();
  CHECK(f.values(f.matched) == list{false});
  CHECK(t1 - t0 < std::chrono::seconds(1));
}
