// Direct KMS scanout harness: drives a real score graph into KmsOutputNode.
//
//   KmsOutputTest                     enumerate, then try to run on the first
//                                     connected connector
//   KmsOutputTest --card /dev/dri/card0 --frames 300
//   KmsOutputTest --tearing           async (immediate) flips
//   KmsOutputTest --buffers 2         shallower scanout ring
//
// Needs DRM master, which a running compositor holds. Under a desktop session
// this reports that and exits rather than fighting for the display -- run it
// from a bare VT, on an appliance, or against vkms.
//
// What it is actually checking, in order: that the scanout buffers can be
// allocated with a modifier the plane advertises, wrapped as QRhi render
// targets, and registered as KMS framebuffers; that the graph renders into them;
// and that flips land at the display's own rate. The last one is the point --
// steady flips at the mode's refresh mean the graph is rendering straight into
// scanout with no present blit anywhere.

#include <QGuiApplication>
#include <QTimer>

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/KmsOutputNode.hpp>
#include <Gfx/Graph/TexgenNode.hpp>
#include <Gfx/Graph/interop/DrmKmsDevice.hpp>

#include <QDebug>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
double nowMs()
{
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// A moving pattern: a static image would flip identically whether or not the
// graph is actually rendering, so the source has to change every frame for the
// run to mean anything.
void paint(unsigned char* rgb, int width, int height, int t)
{
  const int band = height / 8 > 0 ? height / 8 : 1;
  const int yBand = (t * 7) % (height > band ? height - band : 1);
  for(int y = 0; y < height; y++)
  {
    unsigned char* row = rgb + std::size_t(y) * width * 4;
    const bool inBand = (y >= yBand && y < yBand + band);
    for(int x = 0; x < width; x++)
    {
      unsigned char* p = row + std::size_t(x) * 4;
      p[0] = inBand ? 255 : (unsigned char)((x * 255) / (width > 1 ? width - 1 : 1));
      p[1] = inBand ? 255 : (unsigned char)((y * 255) / (height > 1 ? height - 1 : 1));
      p[2] = inBand ? 0 : (unsigned char)((t * 3) & 0xFF);
      p[3] = 255;
    }
  }
}

struct Args
{
  std::string card;
  int frames{300};
  bool tearing{false};
  std::size_t buffers{3};
  bool listOnly{false};
};

Args parseArgs(int argc, char** argv)
{
  Args a;
  for(int i = 1; i < argc; ++i)
  {
    const std::string s = argv[i];
    auto next = [&]() -> std::string {
      return (i + 1 < argc) ? std::string{argv[++i]} : std::string{};
    };
    if(s == "--card")
      a.card = next();
    else if(s == "--frames")
      a.frames = std::atoi(next().c_str());
    else if(s == "--buffers")
      a.buffers = std::size_t(std::atoi(next().c_str()));
    else if(s == "--tearing")
      a.tearing = true;
    else if(s == "--list")
      a.listOnly = true;
  }
  return a;
}

void enumerate()
{
  std::printf("=== DRM devices ===\n");
  for(const auto& d : score::gfx::drm::KmsDevice::enumerateDevices())
  {
    std::printf(
        "%s  driver=%s atomic=%d universal-planes=%d writeback=%d master=%d\n",
        d.path.c_str(), d.driver.c_str(), int(d.atomic), int(d.universalPlanes),
        int(d.writebackConnectors), int(d.hasMaster));
    for(const auto& c : d.connectors)
    {
      if(!c.connected)
        continue;
      std::printf(
          "  connector %u %s  %zu modes%s\n", c.id, c.name.c_str(),
          c.modes.size(), c.writeback ? " (writeback)" : "");
      if(!c.modes.empty())
      {
        const auto& m = c.modes.front();
        std::printf(
            "    first mode: %s %ux%u @ %.3f Hz%s\n", m.name.c_str(), m.width,
            m.height, m.refreshMilliHz / 1000.0, m.preferred ? " (preferred)" : "");
      }
    }
  }
}

int run(int argc, char** argv)
{
  const auto args = parseArgs(argc, argv);
  enumerate();
  if(args.listOnly)
    return 0;

  score::gfx::KmsOutputSettings set;
  set.device = args.card;
  set.tearing = args.tearing;
  set.bufferCount = args.buffers;
  set.verbose = true;

  auto* src = new score::gfx::TexgenNode;
  src->function = &paint;
  auto* out = new score::gfx::KmsOutputNode{set};

  auto graph = std::make_unique<score::gfx::Graph>();
  graph->addNode(src);
  graph->addNode(out);
  graph->addEdge(
      src->output[0], out->input[0], Process::CableType::ImmediateGlutton);
  graph->createAllRenderLists(score::gfx::GraphicsApi::OpenGL);

  if(!out->canRender())
  {
    std::printf(
        "\nSKIP: the KMS output did not come up.\n"
        "  Most likely DRM master is held by a compositor -- see the warning\n"
        "  above. Run from a bare VT, on an appliance, or against vkms\n"
        "  (modprobe vkms, then --card /dev/dri/cardN for the virtual one).\n");
    graph.reset();
    return 2;
  }

  std::printf("\nengaged: %s\n", out->engagedDescription().c_str());

  out->startRendering();
  std::vector<double> intervals;
  intervals.reserve(std::size_t(args.frames));
  double prev = 0;
  const double t0 = nowMs();
  for(int i = 0; i < args.frames; ++i)
  {
    out->render();
    const double t = nowMs();
    if(prev > 0)
      intervals.push_back(t - prev);
    prev = t;
    QCoreApplication::processEvents();
  }
  const double elapsed = nowMs() - t0;
  out->stopRendering();

  // The first interval is the modeset settling rather than a flip, and one
  // settling frame in a mean is how a good result gets reported as a bad one.
  if(intervals.size() > 2)
    intervals.erase(intervals.begin());

  if(intervals.empty())
  {
    std::printf("FAIL: no frames rendered\n");
    graph.reset();
    return 1;
  }
  std::sort(intervals.begin(), intervals.end());
  double sum = 0;
  for(double v : intervals)
    sum += v;
  const double mean = sum / double(intervals.size());

  std::printf(
      "\nframes=%d in %.1f ms\n"
      "interval  min=%.3f  p50=%.3f  mean=%.3f  max=%.3f ms  -> %.2f fps\n",
      args.frames, elapsed, intervals.front(),
      intervals[intervals.size() / 2], mean, intervals.back(),
      mean > 0 ? 1000.0 / mean : 0.0);

  graph.reset();
  return 0;
}
}

int main(int argc, char** argv)
{
  QGuiApplication app{argc, argv};
  return run(argc, argv);
}
