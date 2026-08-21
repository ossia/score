// Video::LibavStreamInput against real network peers.
//
// The FFmpeg input device is what score points at an RTSP camera, an SRT
// contribution feed or an HLS playlist, and a local file never exercises the
// branch that matters: urlIsLiveSource() sets AVFMT_FLAG_NOBUFFER and low_delay
// for anything with a scheme, which changes how avformat_find_stream_info()
// behaves, and a file never goes away mid-play.
//
// Every peer here is an ffmpeg or python http.server process this harness spawns
// and kills, serving the same master clip the codec matrix uses, so the
// assertion is that the picture off the wire is one of the known master frames,
// per pixel, over block interiors.
//
// A protocol the host cannot serve is an attributed SKIP naming what was
// missing, never a green pass: RTSP needs a server and WHIP/WHEP need the
// GStreamer webrtc elements.

#include <Video/LibavStreamInput.hpp>

#include <score_test/StreamServer.hpp>
#include <score_test/VideoMaster.hpp>

#include <QFileInfo>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

using namespace std::chrono_literals;
using namespace score::test::video;
using namespace score::test::stream;

namespace
{
using Options = std::map<std::string, std::string>;
using InputPtr = std::unique_ptr<Video::LibavStreamInput>;

struct Peer
{
  std::string label;
  // argv of the process that serves the stream, port already substituted
  std::vector<std::string> argv;
  std::string url;
  Options options;
  bool tcpPort{};
};

std::string port(int p)
{
  return std::to_string(p);
}

// One row per protocol. The media is always the same master clip, so the
// picture that comes back is comparable across all of them.
std::vector<Peer> peers()
{
  const auto h264 = matrixPath("codec-mpegts-h264-lossy.ts");
  const auto mpeg2 = matrixPath("stream-mpeg2.ts");
  const auto sdp = matrixPath("rtp-session.sdp");
  const auto hls = matrixDir().toStdString() + "/hls";

  std::vector<Peer> out;

  {
    const int p = freeUdpPort();
    out.push_back(
        {"udp/mpegts",
         {"ffmpeg", "-nostdin", "-loglevel", "quiet", "-re", "-stream_loop", "-1",
          "-i", mpeg2, "-c", "copy", "-f", "mpegts",
          "udp://127.0.0.1:" + port(p) + "?pkt_size=1316"},
         "udp://127.0.0.1:" + port(p) + "?fifo_size=1000000&overrun_nonfatal=1"
                                        "&timeout=8000000",
         {{"format", "mpegts"},
          {"analyzeduration", "5000000"},
          {"probesize", "5000000"}}});
  }
  {
    const int p = freeUdpPort();
    out.push_back(
        {"rtp/h264",
         {"ffmpeg", "-nostdin", "-loglevel", "quiet", "-y", "-re", "-stream_loop",
          "-1", "-i", h264, "-an", "-c", "copy", "-f", "rtp", "-sdp_file", sdp,
          "rtp://127.0.0.1:" + port(p)},
         sdp,
         {{"protocol_whitelist", "file,udp,rtp"},
          {"analyzeduration", "5000000"},
          {"probesize", "5000000"}}});
  }
  {
    const int p = freeUdpPort();
    out.push_back(
        {"srt/mpegts",
         {"ffmpeg", "-nostdin", "-loglevel", "quiet", "-re", "-stream_loop", "-1",
          "-i", h264, "-c", "copy", "-f", "mpegts",
          "srt://127.0.0.1:" + port(p) + "?mode=listener"},
         "srt://127.0.0.1:" + port(p) + "?mode=caller&latency=200000",
         {{"analyzeduration", "5000000"}, {"probesize", "5000000"}}});
  }
  {
    const int p = freeTcpPort();
    out.push_back(
        {"rtmp/flv",
         {"ffmpeg", "-nostdin", "-loglevel", "quiet", "-re", "-stream_loop", "-1",
          "-i", h264, "-c", "copy", "-f", "flv", "-listen", "1",
          "rtmp://127.0.0.1:" + port(p) + "/live/x"},
         "rtmp://127.0.0.1:" + port(p) + "/live/x",
         {},
         true});
  }
  {
    const int p = freeTcpPort();
    out.push_back(
        {"hls/http",
         {"python3", "-m", "http.server", port(p), "--bind", "127.0.0.1",
          "--directory", hls},
         "http://127.0.0.1:" + port(p) + "/index.m3u8",
         {},
         true});
  }

  return out;
}

// RIST is built separately and never enters peers(): opening a rist:// URL more
// than once in a process crashes inside librist, so every case that touches it
// runs in a forked child. See the dedicated case at the bottom of this file.
Peer ristPeer()
{
  const auto h264 = matrixPath("codec-mpegts-h264-lossy.ts");
  const int p = freeUdpPort();
  return {"rist/mpegts",
          {"ffmpeg", "-nostdin", "-loglevel", "quiet", "-re", "-stream_loop", "-1",
           "-i", h264, "-c", "copy", "-f", "mpegts",
           "rist://127.0.0.1:" + port(p)},
          "rist://@127.0.0.1:" + port(p),
          {{"format", "mpegts"},
           {"analyzeduration", "5000000"},
           {"probesize", "5000000"}}};
}

// Opens the input, retrying while the peer is still coming up. Returns the
// number of frames whose picture matched a master frame.
struct Received
{
  bool opened{};
  bool started{};
  int frames{};
  int matched{};
  int width{}, height{};
  int worstMaxDev{};
  std::vector<int> masterIndices;
};

Received receiveFrom(
    Video::LibavStreamInput& in, const Peer& peer, const Master& m, int block,
    int wanted, std::chrono::milliseconds budget)
{
  Received r;

  const auto deadline = std::chrono::steady_clock::now() + budget;
  while(std::chrono::steady_clock::now() < deadline)
  {
    REQUIRE(in.load(peer.url, peer.options));
    if(in.probe())
    {
      r.opened = true;
      break;
    }
    std::this_thread::sleep_for(200ms);
  }
  if(!r.opened)
    return r;

  r.width = in.width;
  r.height = in.height;
  r.started = in.start();
  if(!r.started)
    return r;

  while(r.matched < wanted && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
    {
      r.frames++;
      const auto best = bestMatch(*f, m, block);
      if(best.index >= 0 && best.maxDev <= kToleranceLossy)
      {
        r.matched++;
        r.masterIndices.push_back(best.index);
        r.worstMaxDev = std::max(r.worstMaxDev, best.maxDev);
      }
      in.release_frame(f);
    }
    else
    {
      std::this_thread::sleep_for(2ms);
    }
  }
  return r;
}

bool haveProtocol(const char* name)
{
  void* opaque = nullptr;
  while(const char* n = avio_enum_protocols(&opaque, 0))
    if(std::string_view{n} == name)
      return true;
  return false;
}
} // namespace

TEST_CASE("the network fixtures are present", "[video][stream][media]")
{
  // Negative control on the sweep: without the media the peers serve, every
  // case below would report "the peer never came up" instead of testing score.
  REQUIRE(QFileInfo::exists(
      QString::fromStdString(matrixPath("codec-mpegts-h264-lossy.ts"))));
  REQUIRE(
      QFileInfo::exists(QString::fromStdString(matrixPath("stream-mpeg2.ts"))));
  REQUIRE(QFileInfo(matrixDir() + QLatin1String("/hls/index.m3u8")).isFile());

  const auto m = loadMaster();
  CHECK(m.frames >= 4);

  // And on the comparison: a master frame must not match its neighbour, or
  // "the picture off the wire is a master frame" would be vacuous.
  const int block = blockSize();
  const auto d = blockDiff(m.frame(0), m.frame(1), m.width, m.height, block);
  CHECK(d.maxDev > 2 * kLargestTolerance);
}

#if !defined(_WIN32)
TEST_CASE("every protocol delivers the master picture", "[video][stream][media]")
{
  const auto m = loadMaster();
  const int block = blockSize();

  std::vector<std::string> failed;
  for(const auto& peer : peers())
  {
    INFO("peer " << peer.label << " url " << peer.url);

    Process server{peer.argv};
    // rtmp and http are connection-oriented: the peer must be listening before
    // the input tries. The retry loop in receiveFrom() covers the rest.
    std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);

    Video::LibavStreamInput in;
    const auto r = receiveFrom(in, peer, m, block, 3, 25s);
    in.stop();

    INFO("opened " << r.opened << " started " << r.started << " frames "
                   << r.frames << " matched " << r.matched << " geometry "
                   << r.width << "x" << r.height << " worst deviation "
                   << r.worstMaxDev);
    CHECK(r.opened);
    CHECK(r.width == m.width);
    CHECK(r.height == m.height);
    CHECK(r.matched >= 3);
    if(!r.opened || r.matched < 3)
      failed.push_back(peer.label);
  }

  INFO("protocols that did not deliver: " << [&] {
    std::string s;
    for(const auto& f : failed)
      s += f + " ";
    return s;
  }());
  CHECK(failed.empty());
}

TEST_CASE("a peer that goes away mid-stream stops cleanly",
          "[video][stream][media][lifecycle]")
{
  // The case that hangs: the demux thread is blocked in av_read_frame on a
  // socket that will never produce another byte, and something has to unblock it
  // so that stop() can join. Every step runs under a hard deadline: a hang here
  // must FAIL, not wedge the suite.
  const auto m = loadMaster();
  const int block = blockSize();

  for(const auto& peer : peers())
  {
    INFO("peer " << peer.label);

    auto server = std::make_unique<Process>(peer.argv);
    std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);

    // The input lives behind a pointer inside the deadline-guarded state, so
    // that its DESTRUCTOR can be run under a deadline too -- that is the call
    // that has to join the demux thread.
    auto run = std::make_unique<Run<InputPtr>>();
    run->state = std::make_unique<Video::LibavStreamInput>();

    const bool got = finishesWithin(30s, run, [&](InputPtr& in) {
      return receiveFrom(*in, peer, m, block, 2, 25s).matched >= 2;
    });
    REQUIRE(got);
    REQUIRE(run != nullptr);
    CHECK(run->ok);

    // The peer disappears without closing the session.
    server->terminate();
    server.reset();

    // stop() must return: it aborts the interrupt before joining the demux
    // thread, and that is the only thing that can unblock a read on a dead
    // socket.
    const bool stopped = finishesWithin(15s, run, [](InputPtr& in) {
      in->stop();
      return true;
    });
    INFO("stop() after the peer disappeared");
    CHECK(stopped);
    if(!stopped)
      continue;

    const bool destroyed = finishesWithin(15s, run, [](InputPtr& in) {
      in.reset();
      return true;
    });
    INFO("~LibavStreamInput() after the peer disappeared");
    CHECK(destroyed);
  }
}

TEST_CASE("an input destroyed while streaming does not hang",
          "[video][stream][media][lifecycle]")
{
  // No stop() first: the destructor is the only thing between a running demux
  // thread blocked on a live socket and the caller. It is also the ordering the
  // device layer takes when a document is closed mid-play.
  const auto m = loadMaster();
  const int block = blockSize();

  for(const auto& peer : peers())
  {
    INFO("peer " << peer.label);

    Process server{peer.argv};
    std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);

    auto run = std::make_unique<Run<InputPtr>>();
    run->state = std::make_unique<Video::LibavStreamInput>();

    const bool got = finishesWithin(30s, run, [&](InputPtr& in) {
      return receiveFrom(*in, peer, m, block, 2, 25s).matched >= 2;
    });
    REQUIRE(got);
    REQUIRE(run != nullptr);
    CHECK(run->ok);

    // The peer is still alive and still sending: the demux thread is inside
    // av_read_frame right now.
    const bool destroyed = finishesWithin(15s, run, [](InputPtr& in) {
      in.reset();
      return true;
    });
    CHECK(destroyed);
  }
}

TEST_CASE("an input destroyed before its first frame does not hang",
          "[video][stream][media][lifecycle]")
{
  // start() succeeded against a peer that then produced nothing: the demux
  // thread never left its first read.
  for(const auto& peer : peers())
  {
    if(peer.label != "srt/mpegts" && peer.label != "udp/mpegts")
      continue;
    INFO("peer " << peer.label);

    auto server = std::make_unique<Process>(peer.argv);
    std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);

    auto run = std::make_unique<Run<InputPtr>>();
    run->state = std::make_unique<Video::LibavStreamInput>();

    const bool started = finishesWithin(25s, run, [&](InputPtr& in) {
      if(!in->load(peer.url, peer.options))
        return false;
      if(!in->probe())
        return false;
      return in->start();
    });
    REQUIRE(started);
    REQUIRE(run != nullptr);
    if(!run->ok)
    {
      INFO("the peer never opened, nothing to destroy mid-read");
      continue;
    }

    server->terminate();
    server.reset();

    const bool destroyed = finishesWithin(15s, run, [](InputPtr& in) {
      in.reset();
      return true;
    });
    CHECK(destroyed);
  }
}

TEST_CASE("two inputs on one URL do not interfere",
          "[video][stream][media][lifecycle]")
{
  // Two devices pointed at the same address is a normal editing accident.
  //
  // Which of the two receives is the transport's business, not score's: an HTTP
  // server serves both clients, while two unicast UDP sockets bound to the same
  // port with SO_REUSEADDR share the datagrams at the kernel's discretion. What
  // score owes is that AT LEAST ONE delivers, that neither wedges, and that
  // destroying one leaves the other able to keep going.
  const auto m = loadMaster();
  const int block = blockSize();

  for(const auto& peer : peers())
  {
    if(peer.label != "udp/mpegts" && peer.label != "hls/http")
      continue;
    const bool serverFansOut = peer.label == "hls/http";
    INFO("peer " << peer.label);

    Process server{peer.argv};
    std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);

    auto run = std::make_unique<Run<std::vector<InputPtr>>>();
    run->state.push_back(std::make_unique<Video::LibavStreamInput>());
    run->state.push_back(std::make_unique<Video::LibavStreamInput>());

    int matchedA = 0, matchedB = 0;
    const bool opened = finishesWithin(50s, run, [&](std::vector<InputPtr>& ins) {
      matchedA = receiveFrom(*ins[0], peer, m, block, 2, 18s).matched;
      matchedB = receiveFrom(*ins[1], peer, m, block, 2, 18s).matched;
      return true;
    });
    REQUIRE(opened);
    REQUIRE(run != nullptr);
    INFO("first input matched " << matchedA << ", second " << matchedB);
    if(serverFansOut)
    {
      CHECK(matchedA >= 2);
      CHECK(matchedB >= 2);
    }
    else
    {
      CHECK(matchedA + matchedB >= 2);
    }

    // Destroy the first while the second still holds the URL.
    const bool firstGone = finishesWithin(15s, run, [](std::vector<InputPtr>& ins) {
      ins[0].reset();
      return true;
    });
    REQUIRE(firstGone);
    REQUIRE(run != nullptr);

    // The survivor must still be able to run a session of its own.
    int matchedAfter = 0;
    const bool secondStillWorks
        = finishesWithin(25s, run, [&](std::vector<InputPtr>& ins) {
            matchedAfter = receiveFrom(*ins[1], peer, m, block, 2, 18s).matched;
            return true;
          });
    REQUIRE(secondStillWorks);
    REQUIRE(run != nullptr);
    INFO("survivor matched " << matchedAfter << " after the other was destroyed");
    CHECK(matchedAfter >= 2);

    const bool secondGone = finishesWithin(15s, run, [](std::vector<InputPtr>& ins) {
      ins[1].reset();
      return true;
    });
    CHECK(secondGone);
  }
}

TEST_CASE("a rapid create/destroy loop over a live peer does not hang",
          "[video][stream][media][lifecycle]")
{
  // Twenty full lifecycles against a peer that keeps sending: every one of them
  // opens a socket, spawns a demux thread and tears both down. This is where a
  // missing interrupt abort, or a join that races the thread's own exit, shows
  // up as a stall.
  const auto& all = peers();
  const Peer* peer = nullptr;
  for(const auto& p : all)
    if(p.label == "udp/mpegts")
      peer = &p;
  REQUIRE(peer != nullptr);

  Process server{peer->argv};
  std::this_thread::sleep_for(400ms);

  auto run = std::make_unique<Run<int>>();
  const bool finished = finishesWithin(90s, run, [&](int& rounds) {
    for(int i = 0; i < 20; i++)
    {
      Video::LibavStreamInput in;
      if(!in.load(peer->url, peer->options))
        return false;
      // Deliberately NOT symmetric: half the rounds probe and start, half only
      // load, and none of them stop before the destructor runs.
      if(i % 2 == 0)
      {
        if(in.probe())
          in.start();
      }
      rounds++;
    }
    return true;
  });
  REQUIRE(finished);
  REQUIRE(run != nullptr);
  CHECK(run->ok);
  CHECK(run->state == 20);
}

TEST_CASE("the degenerate lifecycle orders are all refused or absorbed",
          "[video][stream][media][lifecycle]")
{
  // The orders a device layer can produce that nothing else covers. All of them
  // under one deadline: any of them hanging is the same defect.
  auto run = std::make_unique<Run<int>>();
  const bool finished = finishesWithin(30s, run, [&](int& step) {
    const auto file = matrixPath("codec-nut-rawvideo-exact.nut");

    {
      // stop() without start()
      Video::LibavStreamInput in;
      in.stop();
      step = 1;
    }
    {
      // stop() twice
      Video::LibavStreamInput in;
      if(!in.load(file))
        return false;
      if(!in.start())
        return false;
      in.stop();
      in.stop();
      step = 2;
    }
    {
      // start() twice: the second must be refused rather than spawn a second
      // demux thread onto the same queue.
      Video::LibavStreamInput in;
      if(!in.load(file))
        return false;
      if(!in.start())
        return false;
      if(in.start())
        return false;
      in.stop();
      step = 3;
    }
    {
      // load() while running: it closes the previous session first.
      Video::LibavStreamInput in;
      if(!in.load(file))
        return false;
      if(!in.start())
        return false;
      if(!in.load(matrixPath("codec-mkv-h264-lossy.mkv")))
        return false;
      if(!in.start())
        return false;
      in.stop();
      step = 4;
    }
    {
      // probe() twice must reuse the open context, not leak a second one.
      Video::LibavStreamInput in;
      if(!in.load(file))
        return false;
      if(!in.probe())
        return false;
      if(!in.probe())
        return false;
      step = 5;
    }
    {
      // Destroyed straight after load(), never started.
      Video::LibavStreamInput in;
      if(!in.load(file))
        return false;
      step = 6;
    }
    {
      // Destroyed straight after start(), before any frame was taken.
      Video::LibavStreamInput in;
      if(!in.load(file))
        return false;
      if(!in.start())
        return false;
      step = 7;
    }
    return true;
  });
  REQUIRE(finished);
  REQUIRE(run != nullptr);
  INFO("reached step " << run->state);
  CHECK(run->ok);
  CHECK(run->state == 7);
}

TEST_CASE("a peer that comes back is picked up again",
          "[video][stream][media][lifecycle]")
{
  // Reconnect: the same input object, after the peer died, must be able to open
  // a new session rather than stay wedged on the old one.
  const auto m = loadMaster();
  const int block = blockSize();

  // One connection-oriented and one datagram protocol: they take different
  // paths through avformat's open.
  for(const auto& peer : peers())
  {
    if(peer.label != "srt/mpegts" && peer.label != "hls/http")
      continue;
    INFO("peer " << peer.label);

    Video::LibavStreamInput in;
    {
      Process server{peer.argv};
      std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);
      const auto r = receiveFrom(in, peer, m, block, 2, 25s);
      INFO("first session matched " << r.matched);
      CHECK(r.matched >= 2);
    }

    // The peer is gone here; the input must let go of it.
    auto run = std::make_unique<Run<int>>();
    const bool stopped = finishesWithin(15s, run, [&](int&) {
      in.stop();
      return true;
    });
    REQUIRE(stopped);

    {
      Process server{peer.argv};
      std::this_thread::sleep_for(peer.tcpPort ? 700ms : 400ms);
      const auto r = receiveFrom(in, peer, m, block, 2, 25s);
      INFO("second session opened " << r.opened << " matched " << r.matched);
      CHECK(r.opened);
      CHECK(r.matched >= 2);
    }
    in.stop();
  }
}

TEST_CASE("a peer that accepts and then says nothing is given up on",
          "[video][stream][media][lifecycle]")
{
  // The connection succeeds, so nothing short of the probe deadline gets the
  // caller back. LibavStreamInput arms a 10 s LibavTimeout around
  // avformat_open_input / avformat_find_stream_info for exactly this.
  SilentTcpServer silent;
  const auto url = "tcp://127.0.0.1:" + std::to_string(silent.port());

  auto run = std::make_unique<Run<InputPtr>>();
  run->state = std::make_unique<Video::LibavStreamInput>();
  const bool returned = finishesWithin(20s, run, [&](InputPtr& in) {
    if(!in->load(url))
      return false;
    return in->probe();
  });
  REQUIRE(returned);
  REQUIRE(run != nullptr);
  CHECK_FALSE(run->ok);
}

TEST_CASE("a peer that is not there at all fails fast",
          "[video][stream][media][lifecycle]")
{
  // Nothing is listening: the connect is refused rather than hanging, so this
  // must come back in well under the probe deadline.
  const int p = freeTcpPort();
  const auto url = "tcp://127.0.0.1:" + std::to_string(p);

  auto run = std::make_unique<Run<InputPtr>>();
  run->state = std::make_unique<Video::LibavStreamInput>();
  const auto start = std::chrono::steady_clock::now();
  const bool returned = finishesWithin(20s, run, [&](InputPtr& in) {
    if(!in->load(url))
      return false;
    return in->probe();
  });
  const auto elapsed = std::chrono::steady_clock::now() - start;
  REQUIRE(returned);
  REQUIRE(run != nullptr);
  CHECK_FALSE(run->ok);
  CHECK(elapsed < 12s);

  // start() must refuse too, rather than spawn a demux thread onto a context
  // that was never opened.
  CHECK_FALSE(run->state->start());
}
#endif

#if !defined(_WIN32)
TEST_CASE("a RIST input can be opened repeatedly without crashing",
          "[video][stream][media][lifecycle][!mayfail]")
{
  // Third-party: opening a rist:// URL a second time in one process segfaults
  // inside librist. Score reaches it through nothing but avformat_open_input, so
  // the stack below LibavStreamInput::probe() belongs to libavformat and librist:
  //
  //   av_vlog                        libavutil.so.58   <- SIGSEGV
  //   av_log                         libavutil.so.58
  //   (rist protocol log callback)   libavformat.so.60
  //   rist_receiver_create           librist.so.4
  //   (rist_open)                    libavformat.so.60
  //   avformat_open_input            libavformat.so.60
  //   Video::LibavStreamInput::probe()
  //
  // Reconnecting a RIST device is one click and can take the application down, but
  // it cannot be fixed in this tree. Isolated in a forked child so the crash is
  // attributed here, and marked [!mayfail] because it is a race: roughly one run
  // in three survives.
  const auto m = loadMaster();
  const int block = blockSize();
  const auto peer = ristPeer();

  Process server{peer.argv};
  std::this_thread::sleep_for(400ms);

  const auto outcome = runInChild([&] {
    Video::LibavStreamInput in;
    if(receiveFrom(in, peer, m, block, 2, 20s).matched < 2)
      return false;
    in.stop();
    // The reopen is what librist does not survive.
    return receiveFrom(in, peer, m, block, 2, 20s).matched >= 2;
  });

  INFO("reopening a rist:// input: " << toString(outcome));
  CHECK(outcome != ChildOutcome::Crashed);
  CHECK(outcome == ChildOutcome::Ok);
}

TEST_CASE("a RIST input delivers the master picture",
          "[video][stream][media][!mayfail]")
{
  // Same isolation, for the same reason: the retry loop inside receiveFrom can
  // itself reopen the URL while the peer is still coming up.
  const auto m = loadMaster();
  const int block = blockSize();
  const auto peer = ristPeer();

  Process server{peer.argv};
  std::this_thread::sleep_for(400ms);

  const auto outcome = runInChild([&] {
    Video::LibavStreamInput in;
    const auto r = receiveFrom(in, peer, m, block, 3, 20s);
    in.stop();
    return r.opened && r.width == m.width && r.height == m.height
           && r.matched >= 3;
  });

  INFO("rist row: " << toString(outcome));
  CHECK(outcome != ChildOutcome::Crashed);
  CHECK(outcome == ChildOutcome::Ok);
}
#endif

TEST_CASE("RTSP", "[video][stream][media][rtsp]")
{
  // Attributed skip, not a silent gap: score's RTSP support goes through the
  // same LibavStreamInput code path as every row above, but serving RTSP needs a
  // server this host does not have. ffmpeg's rtsp muxer has no listen mode in
  // 6.x (it refuses with "Connection refused" against its own port), and
  // gst-rtsp-server is not installed (no libgstrtspserver-1.0, no
  // GstRtspServer GI namespace, no test-launch binary).
  if(!haveProtocol("tcp"))
    SKIP("this libav has no tcp protocol at all");

  const AVInputFormat* rtsp = av_find_input_format("rtsp");
  INFO("libav rtsp demuxer present: " << (rtsp != nullptr));
  CHECK(rtsp != nullptr);

  SKIP("no RTSP server available on this host: ffmpeg's rtsp muxer has no "
       "listen mode and gst-rtsp-server is not installed. Install "
       "gstreamer1.0-rtsp or mediamtx to enable this row.");
}

TEST_CASE("WHIP / WHEP", "[video][stream][media][webrtc]")
{
  // Attributed skip. score reaches WebRTC only through the GStreamer device's
  // whipsink / whepsrc presets (Gfx/GStreamer/GStreamerDevice.cpp), and this
  // host's gst-plugins-rs is not installed: webrtcbin exists, whipsink,
  // whipclientsink and whepsrc do not.
  SKIP("no WHIP/WHEP elements on this host: gst-plugins-rs (whipsink / "
       "whepsrc) is not installed. webrtcbin alone cannot serve the presets.");
}
