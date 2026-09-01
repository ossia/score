// =============================================================================
// Playback throughput of score's video paths, measured through the real
// engine, pumped exactly like GfxContext::update_inputs() does it
// (process(Message) -> node.update() -> output->render()), with the transport
// date advancing one frame per render. No vsync, so the numbers are each
// path's saturation throughput.
//
// Two pipelines, selected with VIDEO_BENCH_MODE:
//   direct (default): VideoNode(PlaybackMode::Direct) -> DirectVideoNodeRenderer.
//     Decode happens synchronously in the render loop (the intra-codec /
//     frame-exact-scrubbing path). fps = rendered frames / wall time.
//   queue: VideoNode(PlaybackMode::FrameQueue) -> VideoNodeRenderer, with the
//     threaded VideoDecoder producing. fps counts DISTINCT frames consumed
//     (reader.m_currentFrameIdx), since the renderer repeats the current
//     frame when the producer is behind.
//
// VIDEO_BENCH_HWDEC selects the hardware decoding method by its settings name
// ("Auto", "CUDA", "VA-API", "Vulkan Video", ...; default "None"). Run with
// XDG_CONFIG_HOME pointing at a scratch directory so the persisted setting
// cannot leak into a real score configuration.
//
// The final readback (RGBA8 1280x720) is hashed and printed so before/after
// runs of a change can be diffed for pixel regressions, not just speed.
//
// Hidden from the default test run: it needs media files and prints numbers
// instead of asserting them. Run as:
//
//   VIDEO_BENCH_FILES=/path/a.mov:/path/b.mov SCORE_TEST_API=Vulkan \
//   VIDEO_BENCH_MODE=direct ./test_gfx_video_direct_bench "[.videobench]"
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>

#include <Video/VideoDecoder.hpp>

#include <ossia/detail/flicks.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
std::vector<std::string> bench_files()
{
  std::vector<std::string> r;
  if(const char* env = getenv("VIDEO_BENCH_FILES"))
  {
    // ';' preferred (needed on Windows, where ':' appears in drive letters);
    // ':' accepted for unixy invocations.
    std::string s{env};
    const char sep = s.find(';') != std::string::npos ? ';' : ':';
    size_t pos = 0;
    while(pos != std::string::npos)
    {
      auto next = s.find(sep, pos);
      auto item = s.substr(pos, next == std::string::npos ? next : next - pos);
      if(item.size() == 1 && isalpha(uint8_t(item[0])) && sep == ':'
         && next != std::string::npos)
      {
        // A bare drive letter split from its path: rejoin.
        auto next2 = s.find(sep, next + 1);
        item += ":"
                + s.substr(
                    next + 1, next2 == std::string::npos ? next2 : next2 - next - 1);
        next = next2;
      }
      if(!item.empty())
        r.push_back(item);
      pos = next == std::string::npos ? next : next + 1;
    }
  }
  return r;
}

struct BenchResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  double fps_achieved = 0.;
  double ms_per_frame = 0.;
  int frames = 0;
  int64_t distinct = 0;
  int width = 0, height = 0;
  double file_fps = 0.;
  uint32_t readback_hash = 0;
  size_t readback_bytes = 0;
};

uint32_t hash_bytes(const char* p, size_t n)
{
  // FNV-1a, enough to compare two runs.
  uint32_t h = 2166136261u;
  for(size_t i = 0; i < n; i++)
  {
    h ^= uint8_t(p[i]);
    h *= 16777619u;
  }
  return h;
}

BenchResult run_bench(
    score::gfx::GraphicsApi api, const std::string& file, bool queue_mode,
    bool window_mode)
{
  BenchResult r;
  r.backend = score::test::gfx::backend_name(api);

  {
    std::string probed;
    if(!score::test::gfx::probe_api(api, probed))
    {
      r.skipped = true;
      r.skip_reason = std::string("backend unavailable: ") + r.backend;
      return r;
    }
  }

  if(const char* hw = getenv("VIDEO_BENCH_HWDEC"))
  {
    auto& set = score::AppContext().settings<Gfx::Settings::Model>();
    set.setHardwareDecode(QString::fromUtf8(hw));
  }
  // Uncapped presentation for throughput measurements (the setting defaults
  // to on); VIDEO_BENCH_VSYNC=1 restores it.
  {
    const char* vs = getenv("VIDEO_BENCH_VSYNC");
    score::AppContext().settings<Gfx::Settings::Model>().setVSync(
        vs && std::string(vs) == "1");
  }

  auto dec = std::make_shared<Video::VideoDecoder>(Video::DecoderConfiguration{});
  const bool opened = queue_mode ? dec->load(file) : dec->open(file);
  if(!opened)
  {
    AVFormatContext* probe{};
    const int rc = avformat_open_input(&probe, file.c_str(), nullptr, nullptr);
    char buf[128]{};
    av_strerror(rc, buf, sizeof buf);
    if(probe)
      avformat_close_input(&probe);
    r.error = "cannot open " + file + " (raw avformat: " + std::to_string(rc) + " "
              + buf + ")";
    return r;
  }
  r.width = dec->width;
  r.height = dec->height;
  r.file_fps = dec->fps > 0 ? dec->fps : 25.;

  auto node = std::make_unique<score::gfx::VideoNode>(dec, std::nullopt);
  node->setPlaybackMode(
      queue_mode ? score::gfx::PlaybackMode::FrameQueue
                 : score::gfx::PlaybackMode::Direct);

  // Offscreen readback target, or a real presented swapchain window (the
  // app's non-vsync manual-rendering path; the Gfx VSync setting defaults to
  // off, so the swapchain is created with QRhiSwapChain::NoVSync).
  std::unique_ptr<score::gfx::BackgroundNode> bg;
  std::unique_ptr<score::gfx::ScreenNode> screen;
  score::gfx::OutputNode* sink{};
  if(window_mode)
  {
    screen = std::make_unique<score::gfx::ScreenNode>(
        score::gfx::OutputNode::Configuration{
            .manualRenderingRate = 1000. / 60., .supportsVSync = false});
    screen->setSize(QSize{1280, 720});
    screen->setTitle(QStringLiteral("score video bench"));
    sink = screen.get();
  }
  else
  {
    bg = std::make_unique<score::gfx::BackgroundNode>();
    bg->shared_readback = std::make_shared<QRhiReadbackResult>();
    bg->setSize(QSize{1280, 720});
    sink = bg.get();
  }

  {
    score::gfx::Graph graph;
    node->nodeId = 1;
    sink->nodeId = 2;
    graph.addNode(node.get());
    graph.addNode(sink);
    graph.addEdge(
        node->output[0], sink->input[0], Process::CableType::ImmediateGlutton);
    graph.createAllRenderLists(api);

    if(window_mode)
    {
      QElapsedTimer t;
      t.start();
      while(!sink->canRender() && t.elapsed() < 5000)
      {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(2);
      }
      if(!sink->canRender())
      {
        r.skipped = true;
        r.skip_reason = "the ScreenNode's swap chain never became ready";
        return r;
      }
    }
    else if(!bg->renderState())
    {
      r.skipped = true;
      r.skip_reason = "offscreen QRhi could not be created";
      return r;
    }
    if(auto rs = sink->renderState(); rs && rs->rhi)
      r.backend = std::string(rs->rhi->backendName()) + "/"
                  + rs->rhi->driverInfo().deviceName.toStdString();

    const double flicks_per_frame
        = ossia::flicks_per_second<double> / r.file_fps;
    const int64_t total_frames
        = std::max<int64_t>(1, int64_t(dec->duration() / flicks_per_frame));

    auto pump = [&](int f) {
      // What GfxContext::update_inputs does per tick, one frame of transport
      // time per render.
      score::gfx::Timings tk{};
      tk.date = ossia::time_value{int64_t(f * flicks_per_frame)};
      tk.parent_duration
          = ossia::time_value{int64_t(total_frames * flicks_per_frame)};
      score::gfx::Message m;
      m.node_id = node->nodeId;
      m.token = tk;
      node->process(std::move(m));
      node->update();
      sink->render();
      if(window_mode)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    };

    constexpr int warmup = 5;
    const int measured
        = std::max<int>(1, std::min<int64_t>(total_frames - warmup, 250));

    for(int f = 0; f < warmup; f++)
      pump(f);

    const int64_t distinct0 = node->reader.m_currentFrameIdx;
    QElapsedTimer timer;
    timer.start();
    for(int f = 0; f < measured; f++)
      pump(warmup + f);
    const double secs = timer.nsecsElapsed() / 1e9;

    r.frames = measured;
    r.distinct = queue_mode ? (node->reader.m_currentFrameIdx - distinct0)
                            : int64_t(measured);
    r.fps_achieved = (queue_mode ? double(r.distinct) : double(measured)) / secs;
    r.ms_per_frame = 1000.0 * secs / std::max<int64_t>(1, r.distinct);

    if(bg)
    {
      if(auto& rb = *bg->shared_readback; rb.data.size() > 0)
      {
        r.readback_bytes = size_t(rb.data.size());
        r.readback_hash = hash_bytes(rb.data.constData(), r.readback_bytes);
      }
    }
  }
  if(screen)
    screen->destroyOutput();
  return r;
}
}

TEST_CASE("direct video playback throughput", "[.videobench]")
{
  const auto files = bench_files();
  if(files.empty())
    SKIP("set VIDEO_BENCH_FILES=/path/a.mov:/path/b.mov");

  const char* modeEnv = getenv("VIDEO_BENCH_MODE");
  const bool queue_mode = modeEnv && std::string(modeEnv) == "queue";
  const char* outEnv = getenv("VIDEO_BENCH_OUTPUT");
  const bool window_mode = outEnv && std::string(outEnv) == "window";

  const auto backend
      = GENERATE(from_range(score::test::gfx::platform_backends()));

  for(const auto& file : files)
  {
    BenchResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = run_bench(backend, file, queue_mode, window_mode);
    });
    if(r.skipped)
    {
      WARN(r.backend << ": " << r.skip_reason);
      continue;
    }
    INFO(r.error);
    REQUIRE(r.error.empty());
    std::fprintf(
        stderr,
        "BENCH %s%-7s %-10s %-44s %dx%d @ %.4g: %8.2f fps (%7.2f ms/frame, "
        "%d pumped, %lld distinct) rb=%08x/%zu\n",
        window_mode ? "win-" : "", queue_mode ? "queue" : "direct",
        r.backend.c_str(), file.c_str(), r.width,
        r.height, r.file_fps, r.fps_achieved, r.ms_per_frame, r.frames,
        (long long)r.distinct, r.readback_hash, r.readback_bytes);
    std::fflush(stderr);
    CHECK(r.fps_achieved > 0.);
  }
}
