#include <Gfx/StreamRequests.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>

TEST_CASE("A held stream outlives the execution, and stops with its last request", "[gfx][camera]")
{
  int starts = 0, stops = 0;
  std::optional<Gfx::StreamRequests> s{std::in_place, [&] { starts++; }, [&] { stops++; }};

  auto a = s->hold();
  CHECK(starts == 1);
  auto b = s->hold();
  CHECK(starts == 1);

  s->startExecution();
  CHECK(starts == 1);
  s->stopExecution();
  CHECK(stops == 0);
  CHECK(s->running());

  a.reset();
  CHECK(stops == 0);
  b.reset();
  CHECK(stops == 1);
  CHECK_FALSE(s->running());

  s->startExecution();
  CHECK(starts == 2);
  auto c = s->hold();
  c.reset();
  CHECK(stops == 1);
  s->stopExecution();
  CHECK(stops == 2);

  auto d = s->hold();
  CHECK(starts == 3);
  s.reset();
  CHECK(d->expired());
  d.reset();
  CHECK(stops == 2);
}

TEST_CASE("Stopping the execution stops a stream nobody holds", "[gfx][camera]")
{
  int stops = 0;
  Gfx::StreamRequests s{[] {}, [&] { stops++; }};
  s.stopExecution();
  CHECK(stops == 1);
}
