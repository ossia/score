// Timeouts for the blocking libav I/O of the video inputs.
//
// A camera that stops answering, or a stream that accepts the connection and
// then goes silent, blocks avformat_open_input / av_read_frame indefinitely.
// Video::LibavInterrupt is the deadline libav polls to give up, and the escape
// hatch close_file() uses so that joining the buffer thread terminates.

#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
#include <Video/CameraInput.hpp>
#include <Video/LibavInterrupt.hpp>
#include <Video/LibavStreamInput.hpp>

#include <QTcpServer>
#include <QTcpSocket>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <catch2/catch_test_macros.hpp>

using namespace std::literals;
using clock_type = std::chrono::steady_clock;

namespace
{
bool has_tcp_protocol()
{
  void* opaque = nullptr;
  while(const char* name = avio_enum_protocols(&opaque, 0))
  {
    if(std::string_view{name} == "tcp")
      return true;
  }
  return false;
}

//! Accepts connections and then never sends anything, like a wedged device.
struct SilentServer
{
  SilentServer()
  {
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
      clients.push_back(server.nextPendingConnection());
    });
  }

  ~SilentServer()
  {
    for(auto* c : clients)
      c->abort();
  }

  std::string url() const
  {
    return "tcp://127.0.0.1:" + std::to_string(server.serverPort());
  }

  QTcpServer server;
  std::vector<QTcpSocket*> clients;
};

//! Everything the call under test touches, kept in one heap block
template <typename Input>
struct RunState
{
  Input input;
  bool result{true};
  std::atomic_bool done{};
};

//! Runs f(state) on a thread and reports whether it returned within the delay.
//!
//! A blocked libav call cannot be cancelled: when it does not return, the
//! thread is detached and the state it keeps writing to is leaked on purpose,
//! so that a failing run reports the timeout instead of a use-after-free.
template <typename Input, typename F>
bool completes_within(
    std::chrono::milliseconds delay, std::unique_ptr<RunState<Input>>& state, F f)
{
  auto* st = state.get();
  std::thread th{[st, f] {
    st->result = f(st->input);
    st->done.store(true, std::memory_order_release);
  }};

  const auto deadline = clock_type::now() + delay;
  while(!st->done.load(std::memory_order_acquire) && clock_type::now() < deadline)
    std::this_thread::sleep_for(5ms);

  if(!st->done.load(std::memory_order_acquire))
  {
    th.detach();
    (void)state.release();
    return false;
  }

  th.join();
  return true;
}
}

TEST_CASE("A fresh interrupt lets libav run", "[libav][interrupt]")
{
  Video::LibavInterrupt itr;
  REQUIRE(!itr.expired());
  REQUIRE(!itr.aborted());

  AVFormatContext ctx{};
  itr.install(ctx);
  REQUIRE(ctx.interrupt_callback.callback != nullptr);
  REQUIRE(ctx.interrupt_callback.opaque == &itr);
  REQUIRE(ctx.interrupt_callback.callback(ctx.interrupt_callback.opaque) == 0);
}

TEST_CASE("An armed interrupt fires once its deadline passes", "[libav][interrupt]")
{
  Video::LibavInterrupt itr;
  AVFormatContext ctx{};
  itr.install(ctx);

  itr.arm(60s);
  REQUIRE(!itr.expired());
  REQUIRE(ctx.interrupt_callback.callback(ctx.interrupt_callback.opaque) == 0);

  itr.arm(1ms);
  std::this_thread::sleep_for(20ms);
  REQUIRE(itr.expired());
  REQUIRE(ctx.interrupt_callback.callback(ctx.interrupt_callback.opaque) == 1);

  itr.disarm();
  REQUIRE(!itr.expired());
}

// close_file() aborts, then joins. The buffer thread it is waiting for owns a
// scoped timeout whose destructor disarms: if that cleared the abort, the next
// read would block again and the join would never return.
TEST_CASE("Aborting outlives the scoped timeouts", "[libav][interrupt]")
{
  Video::LibavInterrupt itr;

  {
    Video::LibavTimeout timeout{itr, 60s};
    REQUIRE(!itr.expired());

    itr.abort();
    REQUIRE(itr.expired());
  }

  REQUIRE(itr.aborted());
  REQUIRE(itr.expired());

  Video::LibavTimeout timeout{itr, 60s};
  REQUIRE(itr.expired());
}

TEST_CASE("Only reset clears an abort", "[libav][interrupt]")
{
  Video::LibavInterrupt itr;
  itr.abort();
  REQUIRE(itr.expired());

  itr.reset();
  REQUIRE(!itr.aborted());
  REQUIRE(!itr.expired());
}

// What unblocks a buffer thread sitting in av_read_frame: libav polls the
// callback from the reading thread while another one aborts.
TEST_CASE("Aborting unblocks a polling reader", "[libav][interrupt]")
{
  Video::LibavInterrupt itr;
  AVFormatContext ctx{};
  itr.install(ctx);

  std::atomic_bool interrupted{};
  std::thread reader{[&] {
    while(ctx.interrupt_callback.callback(ctx.interrupt_callback.opaque) == 0)
      std::this_thread::sleep_for(1ms);
    interrupted.store(true, std::memory_order_release);
  }};

  std::this_thread::sleep_for(20ms);
  REQUIRE(!interrupted.load(std::memory_order_acquire));

  itr.abort();
  reader.join();
  REQUIRE(interrupted.load(std::memory_order_acquire));
}

TEST_CASE("Opening a device that never answers gives up", "[libav][camera]")
{
  if(!has_tcp_protocol())
    SKIP("this libavformat has no tcp protocol");

  SilentServer server;

  auto state = std::make_unique<RunState<Video::CameraInput>>();
  REQUIRE(
      state->input.load("mpegts", server.url(), 640, 480, 30., AV_CODEC_ID_NONE, -1));

  const auto start = clock_type::now();
  const bool finished = completes_within(
      30s, state, [](Video::CameraInput& camera) { return camera.start(); });
  const auto elapsed = clock_type::now() - start;

  REQUIRE(finished);
  REQUIRE(!state->result);
  REQUIRE(elapsed < 20s);
  // Anything faster means it failed before ever reaching the device, which
  // would make this test pass without exercising the timeout at all.
  REQUIRE(elapsed > 500ms);
}

TEST_CASE("Probing a stream that never answers gives up", "[libav][stream]")
{
  if(!has_tcp_protocol())
    SKIP("this libavformat has no tcp protocol");

  SilentServer server;

  auto state = std::make_unique<RunState<Video::LibavStreamInput>>();
  REQUIRE(state->input.load(server.url(), {{"format", "mpegts"}}));

  const auto start = clock_type::now();
  const bool finished = completes_within(
      60s, state, [](Video::LibavStreamInput& stream) { return stream.probe(); });
  const auto elapsed = clock_type::now() - start;

  REQUIRE(finished);
  REQUIRE(!state->result);
  REQUIRE(elapsed < 40s);
  REQUIRE(elapsed > 500ms);
}
#endif
