#include <score/tools/CancellationToken.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

TEST_CASE("a cancelled token runs nothing", "[cancellation]")
{
  auto token = score::CancellationToken::create();
  int calls = 0;
  CHECK(token->run([&] { calls++; }));
  CHECK(calls == 1);
  CHECK_FALSE(token->cancelled());

  token->cancel();
  CHECK(token->cancelled());
  CHECK_FALSE(token->run([&] { calls++; }));
  CHECK(calls == 1);
}

TEST_CASE("cancel waits for the call in progress", "[cancellation]")
{
  auto token = score::CancellationToken::create();
  std::atomic_bool inside{false};
  std::atomic_bool finished{false};
  std::thread worker{[&] {
    token->run([&] {
      inside = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      finished = true;
    });
  }};
  while(!inside)
    std::this_thread::yield();

  token->cancel();
  CHECK(finished);
  worker.join();
}

TEST_CASE("cancelAll cancels every live token", "[cancellation]")
{
  auto a = score::CancellationToken::create();
  auto b = score::CancellationToken::create();
  score::CancellationToken::create();

  score::CancellationToken::cancelAll();
  CHECK(a->cancelled());
  CHECK(b->cancelled());

  auto c = score::CancellationToken::create();
  CHECK_FALSE(c->cancelled());
}
