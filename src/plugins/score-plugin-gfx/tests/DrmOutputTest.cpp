// Direct DRM/KMS output harness.
//
//   DrmOutputTest                       enumerate every card (read-only, safe)
//   DrmOutputTest --card /dev/dri/card0 one card
//   DrmOutputTest --flip --frames 120   modeset + page-flip loop (needs master)
//   DrmOutputTest --flip --async        tearing (immediate) flips
//   DrmOutputTest --flip --atomic       atomic modeset + flips
//   DrmOutputTest --flip --atomic --writeback   verify composited pixels
//   DrmOutputTest --flip --atomic --overlay     overlay-plane composition
//
// Enumeration never needs DRM master, so it is safe to run under a live
// desktop session. Anything that changes state does need master and will
// report so rather than fighting the compositor for the display.

#include <Gfx/Graph/interop/DrmKmsDevice.hpp>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace score::gfx::drm;

namespace
{
struct Args
{
  std::string card;
  std::string connector;
  bool flip{};
  bool async{};
  int frames{60};
  bool verbose{};
  bool atomic{};
  bool writeback{};
  bool overlay{};
};

const char* planeTypeName(PlaneInfo::Type t)
{
  switch(t)
  {
    case PlaneInfo::Primary:
      return "primary";
    case PlaneInfo::Cursor:
      return "cursor";
    default:
      return "overlay";
  }
}

void printDevice(const DeviceInfo& di, bool verbose)
{
  std::printf(
      "%-18s driver=%-10s atomic=%-3s planes=%-3s master=%s\n", di.path.c_str(),
      di.driver.c_str(), di.atomic ? "yes" : "no",
      di.universalPlanes ? "yes" : "no", di.hasMaster ? "yes" : "no");

  for(const auto& c : di.connectors)
  {
    const ModeInfo* best = nullptr;
    for(const auto& m : c.modes)
      if(!best || m.preferred
         || (std::uint64_t(m.width) * m.height
             > std::uint64_t(best->width) * best->height))
        if(!best || m.preferred || !best->preferred)
          best = &m;

    std::printf(
        "    %-14s %-13s %2zu modes", c.name.c_str(),
        c.connected ? "connected" : "disconnected", c.modes.size());
    if(c.writeback)
      std::printf("  [writeback sink]");
    else if(best)
      std::printf(
          "  best: %ux%u@%.3f%s", best->width, best->height,
          best->refreshMilliHz / 1000.0, best->preferred ? " (preferred)" : "");
    std::printf("\n");

    if(verbose)
      for(const auto& m : c.modes)
        std::printf(
            "        %-16s %ux%u@%.3f %s%s\n", m.name.c_str(), m.width, m.height,
            m.refreshMilliHz / 1000.0, m.interlaced ? "interlaced " : "",
            m.preferred ? "preferred" : "");
  }

  for(const auto& p : di.planes)
  {
    // The modifier list is the part that matters: a buffer whose layout is
    // not in it cannot be scanned out directly, which is where a hidden
    // detiling copy would otherwise appear.
    std::size_t nmod = 0;
    for(const auto& f : p.formats)
      nmod += f.modifiers.size();
    std::printf(
        "    plane %-4u %-8s %2zu formats, %zu format/modifier pairs\n", p.id,
        planeTypeName(p.type), p.formats.size(), nmod);
    if(verbose)
      for(const auto& f : p.formats)
      {
        std::printf("        %s :", fourccName(f.fourcc).c_str());
        if(f.modifiers.empty())
          std::printf(" (implicit)");
        for(auto m : f.modifiers)
          std::printf(" %s", modifierName(m).c_str());
        std::printf("\n");
      }
  }
}

/// Fills a BO with a moving pattern so a flip is visually distinguishable and
/// the writeback connector (vkms) has something non-uniform to verify.
void paint(void* map, std::uint32_t stride, std::uint32_t w, std::uint32_t h, int frame)
{
  auto* px = static_cast<std::uint8_t*>(map);
  for(std::uint32_t y = 0; y < h; ++y)
  {
    auto* row = px + std::size_t(y) * stride;
    for(std::uint32_t x = 0; x < w; ++x)
    {
      row[x * 4 + 0] = std::uint8_t((x + frame * 4) & 0xff); // B
      row[x * 4 + 1] = std::uint8_t((y + frame * 2) & 0xff); // G
      row[x * 4 + 2] = std::uint8_t(frame & 0xff);           // R
      row[x * 4 + 3] = 0xff;
    }
  }
}

int runFlip(const Args& a, const DeviceInfo& di)
{
  const ConnectorInfo* conn = nullptr;
  for(const auto& c : di.connectors)
  {
    if(!c.connected || c.modes.empty() || c.writeback)
      continue;
    if(!a.connector.empty() && c.name != a.connector)
      continue;
    conn = &c;
    break;
  }
  if(!conn)
  {
    std::printf("SKIP(no-connected-connector)\n");
    return 77;
  }

  const ModeInfo* mode = &conn->modes[0];
  for(const auto& m : conn->modes)
    if(m.preferred)
    {
      mode = &m;
      break;
    }

  KmsDevice dev;
  if(!dev.open(di.path))
  {
    std::printf("FAIL(open): %s\n", dev.lastError().c_str());
    return 1;
  }
  if(!dev.acquireMaster())
  {
    std::printf("SKIP(no-master): %s\n", dev.lastError().c_str());
    return 77;
  }

  // Dumb buffers rather than GBM: they need no GPU stack at all, so the
  // output can be brought up and paced against a virtual KMS driver.
  constexpr int kBuffers = 2;
  KmsDevice::DumbBuffer bufs[kBuffers];
  for(auto& b : bufs)
  {
    if(!dev.createDumbBuffer(mode->width, mode->height, 32, b))
    {
      std::printf("FAIL(dumb-buffer): %s\n", dev.lastError().c_str());
      return 1;
    }
  }

  std::printf(
      "modeset %s %ux%u@%.3f on %s (%s)\n", conn->name.c_str(), mode->width,
      mode->height, mode->refreshMilliHz / 1000.0, di.path.c_str(),
      di.driver.c_str());
  paint(bufs[0].map, bufs[0].stride, mode->width, mode->height, 0);
  const bool useAtomic = a.atomic || a.writeback || a.overlay;
  if(useAtomic)
  {
    if(!dev.atomicModeset(conn->id, *mode, bufs[0].fbId))
    {
      std::printf("FAIL(atomicModeset): %s\n", dev.lastError().c_str());
      return 1;
    }
    std::printf("  path: atomic\n");
  }
  else if(!dev.setMode(conn->id, *mode, bufs[0].fbId))
  {
    std::printf("FAIL(setMode): %s\n", dev.lastError().c_str());
    return 1;
  }

  // Overlay composition: a second buffer placed by the display engine, not
  // by a shader. Verified through writeback when both are requested.
  KmsDevice::DumbBuffer ovl{};
  std::uint32_t overlayPlane = 0;
  if(a.overlay)
  {
    for(const auto& p : di.planes)
      if(p.type == PlaneInfo::Overlay)
      {
        overlayPlane = p.id;
        break;
      }
    if(!overlayPlane)
      std::printf("  overlay: SKIP(no-overlay-plane)\n");
    else if(!dev.createDumbBuffer(mode->width / 4, mode->height / 4, 32, ovl))
      std::printf("  overlay: FAIL(buffer): %s\n", dev.lastError().c_str());
    else
    {
      auto* px = static_cast<std::uint8_t*>(ovl.map);
      for(std::uint32_t y = 0; y < mode->height / 4; ++y)
        for(std::uint32_t x = 0; x < mode->width / 4; ++x)
        {
          auto* p4 = px + std::size_t(y) * ovl.stride + x * 4;
          p4[0] = 0x00; p4[1] = 0xff; p4[2] = 0x00; p4[3] = 0xff;
        }
      if(!dev.setOverlay(
             overlayPlane, ovl.fbId, 0, 0, mode->width / 4, mode->height / 4))
        std::printf("  overlay: FAIL(setOverlay): %s\n", dev.lastError().c_str());
      else
        std::printf("  overlay: plane %u attached\n", overlayPlane);
    }
  }

  // Flip-to-flip intervals are the measurement that matters: the kernel
  // timestamps when each flip actually landed, which is the clock a pacing
  // policy would schedule against.
  std::vector<double> intervals;
  intervals.reserve(a.frames);
  std::uint64_t prev = 0;

  for(int f = 1; f <= a.frames; ++f)
  {
    auto& b = bufs[f % kBuffers];
    paint(b.map, b.stride, mode->width, mode->height, f);
    const bool flipOk = useAtomic ? dev.atomicFlip(b.fbId, a.async)
                                  : dev.pageFlip(b.fbId, a.async);
    if(!flipOk)
    {
      // Immediate (tearing) flips are optional; a driver without them
      // rejects the flag outright, which is a missing capability rather
      // than a defect in the output path.
      if(a.async && f == 1
         && dev.lastError().find("Invalid argument") != std::string::npos)
      {
        std::printf("SKIP(async-unsupported): %s\n", dev.lastError().c_str());
        for(auto& bb : bufs)
          dev.destroyDumbBuffer(bb);
        return 77;
      }
      std::printf("FAIL(pageFlip @%d): %s\n", f, dev.lastError().c_str());
      return 1;
    }
    const auto ev = dev.waitFlip(2000);
    if(!ev.valid)
    {
      std::printf("FAIL(flip-timeout @%d): %s\n", f, dev.lastError().c_str());
      return 1;
    }
    if(prev)
      intervals.push_back(double(ev.timestampNs - prev) / 1e6);
    prev = ev.timestampNs;
  }

  // Writeback is the only assertion here that proves the display engine
  // composited what we asked, rather than that a flip merely returned.
  if(a.writeback)
  {
    const ConnectorInfo* wb = nullptr;
    for(const auto& c : di.connectors)
      if(c.name.rfind("Writeback", 0) == 0)
      {
        wb = &c;
        break;
      }
    if(!wb)
      std::printf("writeback: SKIP(no-writeback-connector)\n");
    else
    {
      KmsDevice::DumbBuffer cap{};
      if(!dev.createDumbBuffer(mode->width, mode->height, 32, cap))
        std::printf("writeback: FAIL(buffer): %s\n", dev.lastError().c_str());
      else if(!dev.writebackCapture(wb->id, cap.fbId))
      {
        // EINVAL from a writeback commit has several distinct causes; try
        // the variants so the failure names itself instead of needing a
        // kernel debug build to interpret.
        std::printf("writeback: FAIL(capture): %s\n", dev.lastError().c_str());
        struct V { const char* name; bool crtc, modeset, test; };
        static const V variants[] = {
            {"crtc-state + modeset + TEST", true, true, true},
            {"crtc-state + modeset", true, true, false},
            {"no-crtc-state + modeset", false, true, false},
            {"crtc-state + no-modeset", true, false, false},
            {"no-crtc-state + no-modeset", false, false, false},
        };
        for(const auto& v : variants)
          std::printf(
              "  variant %-28s : %s\n", v.name,
              dev.writebackCapture(wb->id, cap.fbId, v.crtc, v.modeset, v.test)
                  ? "OK"
                  : dev.lastError().c_str());
      }
      else
      {
        const auto* got = static_cast<const std::uint8_t*>(cap.map);
        std::size_t nonZero = 0, differing = 0;
        const std::uint8_t first = got[0];
        for(std::uint32_t y = 0; y < mode->height; y += 8)
          for(std::uint32_t x = 0; x < mode->width; x += 8)
          {
            const auto* p4 = got + std::size_t(y) * cap.stride + x * 4;
            if(p4[0] || p4[1] || p4[2])
              nonZero++;
            if(p4[0] != first)
              differing++;
          }
        std::printf(
            "writeback: captured %ux%u  nonzero=%zu differing=%zu  %s\n",
            mode->width, mode->height, nonZero, differing,
            (nonZero > 0 && differing > 0) ? "PASS" : "FAIL(uniform-or-black)");
      }
      dev.destroyDumbBuffer(cap);
    }
  }

  if(overlayPlane)
    dev.setOverlay(overlayPlane, 0, 0, 0, 0, 0);
  if(ovl.fbId)
    dev.destroyDumbBuffer(ovl);
  for(auto& b : bufs)
    dev.destroyDumbBuffer(b);

  if(intervals.empty())
  {
    std::printf("FAIL(no-intervals)\n");
    return 1;
  }
  auto sorted = intervals;
  std::sort(sorted.begin(), sorted.end());
  double sum = 0;
  for(double v : intervals)
    sum += v;
  const double mean = sum / intervals.size();
  const double expected = 1000.0 / (mode->refreshMilliHz / 1000.0);
  int late = 0;
  for(double v : intervals)
    if(v > expected * 1.5)
      late++;

  std::printf(
      "flips=%zu  mean=%.3f ms  min=%.3f  median=%.3f  max=%.3f  expected=%.3f"
      "  late=%d%s\n",
      intervals.size(), mean, sorted.front(), sorted[sorted.size() / 2],
      sorted.back(), expected, late, a.async ? "  (async/tearing)" : "");

  // Async flips are not vblank-bound by definition, so pacing is only judged
  // for the synchronous case.
  const bool ok = a.async ? (late == 0 || mean < expected)
                          : (late == 0 && mean < expected * 1.5);
  std::printf("%s\n", ok ? "PASS" : "FAIL(pacing)");
  return ok ? 0 : 1;
}
} // namespace

int main(int argc, char** argv)
{
  Args a;
  for(int i = 1; i < argc; ++i)
  {
    const std::string s = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if(s == "--card")
      a.card = next();
    else if(s == "--connector")
      a.connector = next();
    else if(s == "--flip")
      a.flip = true;
    else if(s == "--async")
      a.async = true;
    else if(s == "--atomic")
      a.atomic = true;
    else if(s == "--writeback")
      a.writeback = true;
    else if(s == "--overlay")
      a.overlay = true;
    else if(s == "--verbose" || s == "-v")
      a.verbose = true;
    else if(s == "--frames")
      a.frames = std::atoi(next().c_str());
  }

  auto devices = KmsDevice::enumerateDevices();
  if(!a.card.empty())
  {
    std::vector<DeviceInfo> f;
    for(auto& d : devices)
      if(d.path == a.card)
        f.push_back(d);
    devices = f;
  }
  if(devices.empty())
  {
    std::printf("no KMS-capable DRM devices\n");
    return 2;
  }

  for(const auto& d : devices)
    printDevice(d, a.verbose);

  if(!a.flip)
    return 0;

  std::printf("\n");
  return runFlip(a, devices.front());
}
