// Smoke unit test: exercises the new test harness (Catch2 + score_add_test)
// against a small, pure piece of score logic (State::Address parsing).
// No application context required.

#include <State/Address.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("State::Address parses a well-formed address", "[state][address]")
{
  const auto addr = State::Address::fromString("device:/foo/bar");

  REQUIRE(addr.has_value());
  CHECK(addr->device == "device");
  REQUIRE(addr->path.size() == 2);
  CHECK(addr->path[0] == "foo");
  CHECK(addr->path[1] == "bar");
  CHECK(addr->isSet());
}

TEST_CASE("State::Address round-trips through a string", "[state][address]")
{
  const auto addr = State::Address::fromString("dev:/a/b/c");
  REQUIRE(addr.has_value());

  const auto reparsed = State::Address::fromString(addr->toString());
  REQUIRE(reparsed.has_value());
  CHECK(*reparsed == *addr);
}

TEST_CASE("State::Address rejects malformed input", "[state][address]")
{
  CHECK_FALSE(State::Address::fromString("not an address").has_value());
  CHECK_FALSE(State::Address::fromString("").has_value());
}
