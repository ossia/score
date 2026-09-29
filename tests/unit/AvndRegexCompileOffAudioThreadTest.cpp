// Regex: an edit of the pattern that reaches the object on the audio thread is
// compiled by the worker, not there; results that come back out of order do
// not replace a later pattern; and away from the audio thread (setting the
// node up) the pattern is compiled right away.

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/thread.hpp>

#include <AvndProcesses/Regex.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
struct Request
{
  ao::Regex::Spec spec;
  uint64_t generation{};
};

struct AudioThreadScope
{
  AudioThreadScope() { ossia::set_thread_pinned(ossia::thread_type::Audio, 0); }
  ~AudioThreadScope() { ossia::set_thread_pinned(ossia::thread_type::Ui, 0); }
};
}

TEST_CASE("Regex: the audio thread never compiles a pattern", "[avnd][regex][realtime]")
{
  ao::Regex r;
  std::vector<Request> requests;
  r.worker.request = [&](ao::Regex::Spec s, uint64_t g) {
    requests.push_back({std::move(s), g});
  };

  SECTION("off the audio thread: at once")
  {
    r.inputs.pattern.value = "a(b)";
    r.inputs.pattern.update(r);
    CHECK(requests.empty());
    REQUIRE(r.re);
    CHECK(r.re->pattern() == "a(b)");
    CHECK(r.submatches.size() == 2);
  }

  SECTION("on the audio thread: through the worker, the latest edit wins")
  {
    {
      AudioThreadScope audio;
      r.inputs.pattern.value = "first";
      r.inputs.pattern.update(r);
      r.inputs.pattern.value = "(second)";
      r.inputs.pattern.update(r);
    }
    CHECK(!r.re); // nothing compiled on the audio thread
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].spec.pattern == "first");
    CHECK(requests[1].spec.pattern == "(second)");

    // The worker threads finish in the other order
    auto late = ao::Regex::worker::work(requests[1].spec, requests[1].generation);
    auto early = ao::Regex::worker::work(requests[0].spec, requests[0].generation);
    late(r);
    early(r);
    REQUIRE(r.re);
    CHECK(r.re->pattern() == "(second)");
    CHECK(r.submatches.size() == 2);
  }

  SECTION("a pattern that does not compile reports why, from the worker too")
  {
    {
      AudioThreadScope audio;
      r.inputs.pattern.value = "(unclosed";
      r.inputs.pattern.update(r);
    }
    REQUIRE(requests.size() == 1);
    ao::Regex::worker::work(requests[0].spec, requests[0].generation)(r);
    CHECK(!r.re);
    CHECK(r.error_pending);
    CHECK(!r.error_text.empty());
  }
}

TEST_CASE("Regex: a pattern RE2 finds too big is an error, not a stall", "[avnd][regex]")
{
  // Nested counted repetitions blow up the compiled program: RE2 refuses it
  // within its memory budget instead of spending unbounded time or memory.
  ao::Regex r;
  r.inputs.pattern.value = "((((a{100}){100}){100}){100})";
  r.inputs.pattern.update(r);
  CHECK(!r.re);
  CHECK(!r.error_text.empty());
}
