// oscr::GpuWorker: the result of a GPU object's worker reaches the object at
// its next tick (drainWorker) even when the Qt event loop does not run, as in
// GfxContext::renderFrames; and the result of an object whose node was
// destroyed while it was computed is dropped, and freed, on the worker thread.
// A tick of a render driven by the step clock waits for the jobs in flight, so
// that the frame does not depend on the worker's speed; the work of an object
// that is gone is not done.

#include <Crousti/GpuUtils.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace
{
std::atomic_bool g_release{true};
std::atomic<std::thread::id> g_freed_on{};

struct Payload
{
  ~Payload() { g_freed_on = std::this_thread::get_id(); }
};

std::atomic_int g_works{};

struct Object
{
  int applied = 0;
  struct worker
  {
    std::function<void(int)> request;
    static std::function<void(Object&)> work(int v)
    {
      ++g_works;
      while(!g_release)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return [v, p = std::make_shared<Payload>()](Object& o) { o.applied = v; };
    }
  } worker;
};

struct Node : oscr::GpuWorker
{
};

// Each result asks for the next value, down to 0.
struct Countdown
{
  std::vector<int> applied;
  struct worker
  {
    std::function<void(int)> request;
    static std::function<void(Countdown&)> work(int v)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      return [v](Countdown& o) {
        o.applied.push_back(v);
        if(v > 0)
          o.worker.request(v - 1);
      };
    }
  } worker;
};

template <typename F>
bool waitFor(F&& done)
{
  for(int i = 0; i < 5000 && !done(); i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  return done();
}

QCoreApplication& app()
{
  static int argc = 1;
  static char arg0[] = "test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication a{argc, argv};
  return a;
}
}

TEST_CASE("GPU worker: a result reaches the object at its tick without the event loop", "[avnd][gfx][worker]")
{
  app();
  g_release = true;
  Node node;
  auto state = std::make_shared<Object>();
  node.initWorker(state);

  state->worker.request(42);
  REQUIRE(waitFor([&] { return node.m_workerResults->queue.size_approx() > 0; }));
  CHECK(state->applied == 0); // not applied from the worker thread
  node.drainWorker();
  CHECK(state->applied == 42);

  // The event loop's delivery finds it applied already.
  state->applied = 0;
  QCoreApplication::processEvents();
  CHECK(state->applied == 0);
}

TEST_CASE("GPU worker: the event loop still delivers between ticks", "[avnd][gfx][worker]")
{
  app();
  g_release = true;
  Node node;
  auto state = std::make_shared<Object>();
  node.initWorker(state);

  state->worker.request(7);
  REQUIRE(waitFor([&] {
    QCoreApplication::processEvents();
    return state->applied == 7;
  }));
}

TEST_CASE("GPU worker: the result of a destroyed node is dropped on the worker", "[avnd][gfx][worker]")
{
  app();
  g_release = false;
  g_freed_on = std::thread::id{};
  auto state = std::make_shared<Object>();
  {
    Node node;
    node.initWorker(state);
    state->worker.request(1);
  }
  g_release = true;
  REQUIRE(waitFor([] { return g_freed_on.load() != std::thread::id{}; }));
  CHECK(g_freed_on.load() != std::this_thread::get_id());
  QCoreApplication::processEvents();
  CHECK(state->applied == 0);
}

TEST_CASE("GPU worker: results from different pool threads apply in the order they completed", "[avnd][gfx][worker]")
{
  // Pool threads stay alive between jobs, each with its own producer queue.
  oscr::GpuWorker::WorkerResults results;
  std::vector<int> applied;
  std::atomic_int turn{0};
  std::atomic_bool done{false};
  std::vector<std::thread> pool;
  for(int i = 0; i < 3; i++)
    pool.emplace_back([&, i] {
      while(turn != i)
        std::this_thread::yield();
      results.push([&applied, i] { applied.push_back(i); });
      turn++;
      while(!done)
        std::this_thread::yield();
    });
  while(turn != 3)
    std::this_thread::yield();
  results.drain();
  done = true;
  for(auto& t : pool)
    t.join();
  CHECK(applied == std::vector<int>{0, 1, 2});
}

TEST_CASE("GPU worker: a stepped tick waits for the jobs in flight", "[avnd][gfx][worker]")
{
  app();
  g_release = false;
  Node node;
  auto state = std::make_shared<Object>();
  node.initWorker(state);

  state->worker.request(5);
  node.drainWorker(); // a live tick does not wait
  CHECK(state->applied == 0);

  std::thread release{[] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    g_release = true;
  }};
  node.drainWorker(true);
  CHECK(state->applied == 5);
  release.join();
}

TEST_CASE("GPU worker: a stepped tick waits for the jobs its results request", "[avnd][gfx][worker]")
{
  app();
  Node node;
  auto state = std::make_shared<Countdown>();
  node.initWorker(state);
  state->worker.request(3);
  node.drainWorker(true);
  CHECK(state->applied == std::vector<int>{3, 2, 1, 0});
}

TEST_CASE("GPU worker: the work of an object that is gone is not done", "[avnd][gfx][worker]")
{
  app();
  g_release = true;
  Node node;
  auto state = std::make_shared<Object>();
  node.initWorker(state);
  auto request = state->worker.request;
  state.reset();
  g_works = 0;
  request(1);
  node.drainWorker(true); // the job has run
  CHECK(g_works == 0);
}
