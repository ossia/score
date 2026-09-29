// Buffer queue: every output mode, data choice and Pop setting on scripted
// input sequences, with the exact output of every tick.

#include <ossia/network/value/format_value.hpp>

#include <AvndProcesses/Queue.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

template <>
struct Catch::StringMaker<ossia::value>
{
  static std::string convert(const ossia::value& v) { return fmt::format("{}", v); }
};

namespace
{
using Queue = avnd_tools::Queue;
using out = std::optional<ossia::value>;
using list = std::vector<ossia::value>;

struct Step
{
  std::optional<int> input;
  bool clear{};
  bool lock{};
  bool bang{};
};

std::vector<out> run(
    Queue::OutputMode mode, Queue::OutputData data, bool pop,
    const std::vector<Step>& script, int length = 3)
{
  Queue q;
  q.inputs.mode.value = mode;
  q.inputs.data.value = data;
  q.inputs.pop.value = pop;
  q.inputs.length.value = length;
  q.inputs.length.update(q);

  std::vector<out> res;
  for(const auto& s : script)
  {
    if(s.input)
      q.inputs.input.value = ossia::value{*s.input};
    q.inputs.clear.value = s.clear;
    q.inputs.lock.value = s.lock;
    if(s.bang)
      q.inputs.bang.update(q);
    q();
    res.push_back(q.outputs.output.value);
  }
  return res;
}

void check(const std::vector<out>& actual, const std::vector<out>& expected)
{
  REQUIRE(actual.size() == expected.size());
  for(std::size_t i = 0; i < actual.size(); i++)
  {
    INFO("tick " << i);
    CHECK(actual[i] == expected[i]);
  }
}

const out none{};
out L(std::initializer_list<int> v)
{
  list l;
  for(int x : v)
    l.push_back(x);
  return ossia::value{std::move(l)};
}
out V(int v)
{
  return ossia::value{v};
}
// A send from an empty queue of a single value: engaged but invalid, which the
// binding does not write to the outlet.
const out empty_single{ossia::value{}};

// Max length 3. Buffer after each tick in the comments.
const std::vector<Step> script{
    {.input = 1},                // 0  [1]
    {},                          // 1  [1]
    {.input = 2},                // 2  [1 2]
    {.input = 3},                // 3  [1 2 3] full
    {},                          // 4  [1 2 3] full
    {.input = 4},                // 5  [2 3 4] full, 1 evicted
    {.input = 5, .lock = true},  // 6  [2 3 4] input dropped
    {.bang = true},              // 7  [2 3 4]
    {.clear = true},             // 8  []
    {.input = 6},                // 9  [6]
    {.input = 7, .clear = true}, // 10 [] input dropped, 6 cleared
    {.clear = true},             // 11 [] nothing to clear
};
}

TEST_CASE("Buffer queue modes: when the whole buffer is sent", "[avnd][queue]")
{
  const auto W = Queue::WholeBuffer;
  SECTION("Every tick")
  {
    check(
        run(Queue::EveryTick, W, false, script),
        {L({1}), L({1}), L({1, 2}), L({1, 2, 3}), L({1, 2, 3}), L({2, 3, 4}),
         L({2, 3, 4}), L({2, 3, 4}), L({}), L({6}), L({}), L({})});
  }
  SECTION("When full: every tick while full")
  {
    check(
        run(Queue::WhenFull, W, false, script),
        {none, none, none, L({1, 2, 3}), L({1, 2, 3}), L({2, 3, 4}), L({2, 3, 4}),
         L({2, 3, 4}), none, none, none, none});
  }
  SECTION("On bang only")
  {
    check(
        run(Queue::OnBang, W, false, script),
        {none, none, none, none, none, none, none, L({2, 3, 4}), none, none, none,
         none});
  }
  SECTION("On input: when a message arrives, even dropped")
  {
    check(
        run(Queue::OnInput, W, false, script),
        {L({1}), none, L({1, 2}), L({1, 2, 3}), none, L({2, 3, 4}), L({2, 3, 4}),
         L({2, 3, 4}), none, L({6}), L({}), none});
  }
  SECTION("On change: when the contents change")
  {
    check(
        run(Queue::OnChange, W, false, script),
        {L({1}), none, L({1, 2}), L({1, 2, 3}), none, L({2, 3, 4}), none,
         L({2, 3, 4}), L({}), L({6}), L({}), none});
  }
  SECTION("On input, when full")
  {
    check(
        run(Queue::OnInputWhenFull, W, false, script),
        {none, none, none, L({1, 2, 3}), none, L({2, 3, 4}), L({2, 3, 4}),
         L({2, 3, 4}), none, none, none, none});
  }
  SECTION("On change, when full")
  {
    check(
        run(Queue::OnChangeWhenFull, W, false, script),
        {none, none, none, L({1, 2, 3}), none, L({2, 3, 4}), none, L({2, 3, 4}),
         none, none, none, none});
  }
}

TEST_CASE("Buffer queue modes: which value is sent", "[avnd][queue]")
{
  SECTION("Oldest")
  {
    check(
        run(Queue::EveryTick, Queue::Oldest, false, script),
        {V(1), V(1), V(1), V(1), V(1), V(2), V(2), V(2), empty_single, V(6),
         empty_single, empty_single});
  }
  SECTION("Newest")
  {
    check(
        run(Queue::EveryTick, Queue::Newest, false, script),
        {V(1), V(1), V(2), V(3), V(3), V(4), V(4), V(4), empty_single, V(6),
         empty_single, empty_single});
  }
}

TEST_CASE("Buffer queue modes: Pop removes what was sent", "[avnd][queue]")
{
  SECTION("On change, oldest: the removal is not a change that sends again")
  {
    check(
        run(Queue::OnChange, Queue::Oldest, true, script),
        {V(1), none, V(2), V(3), none, V(4), none, empty_single, none, V(6), none,
         none});
  }
  SECTION("When full, whole buffer: chunks of Max length")
  {
    check(
        run(Queue::WhenFull, Queue::WholeBuffer, true, script),
        {none, none, none, L({1, 2, 3}), none, none, none, L({4}), none, none, none,
         none});
  }
  SECTION("On change when full, whole buffer: chunks of Max length")
  {
    const std::vector<Step> in{{.input = 1}, {.input = 2}, {}, {.input = 3},
                               {.input = 4}, {.input = 5}, {.input = 6}};
    check(
        run(Queue::OnChangeWhenFull, Queue::WholeBuffer, true, in),
        {none, none, none, L({1, 2, 3}), none, none, L({4, 5, 6})});
  }
}

TEST_CASE("Buffer queue modes: banging a queue of 1 2 3", "[avnd][queue]")
{
  const std::vector<Step> bangs{
      {.input = 1},    {.input = 2},    {.input = 3},   {.bang = true},
      {.bang = true},  {.bang = true},  {.bang = true},
  };
  const auto sent = [&](Queue::OutputMode mode, Queue::OutputData data, bool pop) {
    auto res = run(mode, data, pop, bangs);
    for(int i = 0; i < 3; i++)
      CHECK_FALSE(res[i]);
    return std::vector<out>(res.begin() + 3, res.end());
  };
  CHECK(sent(Queue::OnBang, Queue::Oldest, false)
        == std::vector<out>{V(1), V(1), V(1), V(1)});
  CHECK(sent(Queue::OnBang, Queue::Oldest, true)
        == std::vector<out>{V(1), V(2), V(3), empty_single});
  CHECK(sent(Queue::OnBang, Queue::Newest, true)
        == std::vector<out>{V(3), V(2), V(1), empty_single});
  CHECK(sent(Queue::OnBang, Queue::WholeBuffer, true)
        == std::vector<out>{L({1, 2, 3}), L({}), L({}), L({})});

  // On bang, pop oldest: each output removes the oldest value, whatever is
  // sent.
  CHECK(sent(Queue::OnBangPopOldest, Queue::Oldest, false)
        == std::vector<out>{V(1), V(2), V(3), empty_single});
  CHECK(sent(Queue::OnBangPopOldest, Queue::WholeBuffer, false)
        == std::vector<out>{L({1, 2, 3}), L({2, 3}), L({3}), L({})});
  CHECK(sent(Queue::OnBangPopOldest, Queue::Newest, false)
        == std::vector<out>{V(3), V(3), V(3), empty_single});
  // With Pop, it removes what it sent
  CHECK(sent(Queue::OnBangPopOldest, Queue::Newest, true)
        == std::vector<out>{V(3), V(2), V(1), empty_single});
}

TEST_CASE("Buffer queue modes: the combo boxes label the enumerators in order", "[avnd][queue]")
{
  using mode_control = decltype(std::declval<Queue&>().inputs.mode);
  using data_control = decltype(std::declval<Queue&>().inputs.data);
  constexpr auto modes = mode_control::range{};
  constexpr auto data = data_control::range{};
  STATIC_CHECK(std::size(modes.values) == Queue::OnBangPopOldest + 1);
  STATIC_CHECK(std::size(data.values) == Queue::WholeBuffer + 1);
  STATIC_CHECK(modes.values[Queue::EveryTick] == "Every tick");
  STATIC_CHECK(modes.values[Queue::OnInput] == "On input");
  STATIC_CHECK(modes.values[Queue::OnChange] == "On change");
  STATIC_CHECK(modes.values[Queue::WhenFull] == "When full");
  STATIC_CHECK(modes.values[Queue::OnInputWhenFull] == "On input, when full");
  STATIC_CHECK(modes.values[Queue::OnChangeWhenFull] == "On change, when full");
  STATIC_CHECK(modes.values[Queue::OnBang] == "On bang");
  STATIC_CHECK(modes.values[Queue::OnBangPopOldest] == "On bang, pop oldest");
  STATIC_CHECK(data.values[Queue::Oldest] == "Oldest");
  STATIC_CHECK(data.values[Queue::Newest] == "Newest");
  STATIC_CHECK(data.values[Queue::WholeBuffer] == "Whole buffer");
  STATIC_CHECK(modes.init == Queue::EveryTick);
  STATIC_CHECK(data.init == Queue::Oldest);
}
