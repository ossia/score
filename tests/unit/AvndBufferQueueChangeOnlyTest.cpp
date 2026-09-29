// Buffer queue: in the Always and When full modes the output goes out when the
// queue changed, not again at every tick -- a whole buffer of up to 100000
// values is not copied out at every tick for nothing.

#include <AvndProcesses/Queue.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
struct Rig
{
  avnd_tools::Queue q;
  Rig(avnd_tools::Queue::OutputMode mode, int length)
  {
    q.inputs.mode.value = mode;
    q.inputs.data.value = avnd_tools::Queue::WholeBuffer;
    q.inputs.length.value = length;
    q.inputs.length.update(q);
  }
  std::optional<ossia::value> tick(std::optional<ossia::value> in, bool clear = false)
  {
    q.inputs.input.value = std::move(in);
    q.inputs.clear.value = clear;
    q();
    return q.outputs.output.value;
  }
};
using list = std::vector<ossia::value>;
}

TEST_CASE("Buffer queue: Always sends on every change, and only then", "[avnd][queue]")
{
  Rig r{avnd_tools::Queue::Always, 3};
  CHECK(r.tick(ossia::value{1}) == ossia::value{list{1}});
  CHECK_FALSE(r.tick(std::nullopt));
  CHECK_FALSE(r.tick(std::nullopt));
  CHECK(r.tick(ossia::value{2}) == ossia::value{list{1, 2}});
  // A clear is a change: the empty buffer goes out once
  CHECK(r.tick(std::nullopt, true) == ossia::value{list{}});
  CHECK_FALSE(r.tick(std::nullopt, true));
}

TEST_CASE("Buffer queue: When full sends each input once it is full", "[avnd][queue]")
{
  Rig r{avnd_tools::Queue::WhenFull, 2};
  CHECK_FALSE(r.tick(ossia::value{1}));
  CHECK(r.tick(ossia::value{2}) == ossia::value{list{1, 2}});
  CHECK_FALSE(r.tick(std::nullopt));
  CHECK(r.tick(ossia::value{3}) == ossia::value{list{2, 3}});
}
