// =============================================================================
// L3 -- what the GPU Bayer demosaic (9a6c1bb035) actually puts on screen.
//
// tests/unit/WireDecoderFactoryTest.cpp proves each CFA format resolves to A
// decoder; nothing asserted what that decoder reconstructs. The demosaic's two
// parameters are exactly the failure modes the commit warns about:
//
//   - the CFA ORDER: a wrong phase does not look broken, it looks like a
//     colour cast -- easy to chase as white balance forever;
//   - `sampleScale`: ten bits right-aligned in a 16-bit lane normalise to
//     1/64 of full scale, so a missing 65535/1023 is a picture that is merely
//     dark, which a "not blank" check would bless.
//
// So the frames are synthesised: a mosaic of ONE constant colour as a real
// sensor with that CFA order would sample it (R=200 at red sites, G=100 at
// green sites, B=50 at blue sites). Bilinear reconstruction of a constant
// scene is exact at every interior pixel whatever the interpolation weights,
// so the expectation is the colour itself, computed from the format spec --
// not from score. A wrong phase globally permutes the channel assignment
// (RGGB bytes decoded as GRBG turn (200,100,50) into (100,200,100)), so the
// per-format mosaic + fixed expectation pins each factory phase. Uniformity
// also makes the assertion orientation-proof: a flip only moves the edges.
//
// The path is the real capture path: a fake DMACaptureBackend whose decoder is
// makeWireDecoder(<bayer format>) and whose only strategy is the real
// CpuStagedCapture -- the same rung a V4L2 sensor uses when zero-copy
// declines. The producer side is this test: memcpy into the strategy's slot,
// ingestFrame, bump the ring, exactly the contract the capture threads follow.
// Nothing about the render path is faked.
//
// Edges are excluded (interior only, 2 px in): the shader clamps its
// neighbourhood at the border, which mixes site classes by design.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/DMACaptureInputNode.hpp>
#include <Gfx/Graph/decoders/WireDecoderFactory.hpp>
#include <Gfx/Graph/interop/CpuStagedCapture.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;
namespace ic = score::gfx::interop;
using V = ic::VideoPixelFormat;

namespace
{
constexpr int kW = 16;
constexpr int kH = 16;

// The constant colour, in 8-bit codes. Chosen distinct in every channel so a
// phase permutation moves all three.
constexpr int kR = 200, kG = 100, kB = 50;

// Which of R/G/B a 2x2 CFA cell holds at (x&1, y&1), per order.
struct CfaOrder
{
  const char* name;
  V format;
  // site[y&1][x&1] in {'R','G','B'}
  char site[2][2];
};

// One constant-colour mosaic, 8-bit: each pixel carries its site's channel.
std::vector<std::uint8_t> mosaic8(const CfaOrder& o)
{
  std::vector<std::uint8_t> m(std::size_t(kW) * kH);
  for(int y = 0; y < kH; y++)
    for(int x = 0; x < kW; x++)
    {
      const char c = o.site[y & 1][x & 1];
      m[std::size_t(y) * kW + x]
          = std::uint8_t(c == 'R' ? kR : c == 'G' ? kG : kB);
    }
  return m;
}

// 16-bit little-endian mosaic; `scale` maps the 8-bit code into the lane
// (257 = full-range 16-bit, 4 = 10-bit right-aligned: code*4 <= 1020 < 1023).
std::vector<std::uint8_t> mosaic16(const CfaOrder& o, int scale)
{
  std::vector<std::uint8_t> m(std::size_t(kW) * kH * 2);
  for(int y = 0; y < kH; y++)
    for(int x = 0; x < kW; x++)
    {
      const char c = o.site[y & 1][x & 1];
      const std::uint16_t v
          = std::uint16_t((c == 'R' ? kR : c == 'G' ? kG : kB) * scale);
      std::memcpy(&m[(std::size_t(y) * kW + x) * 2], &v, 2);
    }
  return m;
}

struct BayerBackend final : score::gfx::DMACaptureBackend
{
  BayerBackend(ic::VideoCaptureSlotRing& ring, V fmt, std::uint32_t bpp)
      : m_ring{ring}
      , m_fmt{fmt}
      , m_bpp{bpp}
  {
  }

  bool open() override { return true; }
  int width() const noexcept override { return kW; }
  int height() const noexcept override { return kH; }
  std::uint32_t frameByteSize() const noexcept override
  {
    return std::uint32_t(kW) * kH * m_bpp;
  }

  Video::ImageFormat imageFormat() const override
  {
    Video::ImageFormat f;
    f.width = kW;
    f.height = kH;
    return f;
  }

  std::unique_ptr<score::gfx::GPUVideoDecoder>
  makeDecoder(Video::VideoMetadata& meta) override
  {
    return score::gfx::makeWireDecoder(m_fmt, meta);
  }

  // No GPU-direct rung: the ladder must land on the CPU rung, which is the
  // one a V4L2 Bayer sensor without dma-buf uses.
  std::unique_ptr<ic::VideoCaptureStrategy>
  pickStrategy(QRhi::Implementation, const ic::GpuCapabilities&) override
  {
    return nullptr;
  }

  std::unique_ptr<ic::VideoCaptureStrategy> makeCpuStrategy() override
  {
    return std::make_unique<
        ic::CpuStagedCapture<ic::CpuStagedNoLockPolicy>>();
  }

  void setStrategy(ic::VideoCaptureStrategy* s) noexcept override
  {
    strategy = s;
  }
  void start() override { }
  void stop() override { }

  /// The producer side, verbatim from the VideoCaptureSlotRing contract.
  bool push(const std::vector<std::uint8_t>& bytes)
  {
    if(!strategy || bytes.size() != frameByteSize())
      return false;
    const std::size_t slot = m_next++ % strategy->slotCount();
    void* dst = strategy->slotBuffer(slot);
    if(!dst)
      return false;
    std::memcpy(dst, bytes.data(), bytes.size());
    strategy->ingestFrame(slot);
    m_ring.latestSlot.store(slot, std::memory_order_release);
    m_ring.latestFrameId.fetch_add(1, std::memory_order_release);
    return true;
  }

  ic::VideoCaptureSlotRing& m_ring;
  V m_fmt;
  std::uint32_t m_bpp;
  ic::VideoCaptureStrategy* strategy{};
  std::size_t m_next{0};
};

struct BayerNode final : score::gfx::DMACaptureInputNode
{
  BayerNode(V fmt, std::uint32_t bpp)
      : m_fmt{fmt}
      , m_bpp{bpp}
  {
  }

  std::unique_ptr<score::gfx::DMACaptureBackend>
  makeCaptureBackend(ic::VideoCaptureSlotRing& ring) const override
  {
    auto b = std::make_unique<BayerBackend>(ring, m_fmt, m_bpp);
    live = b.get();
    return b;
  }

  V m_fmt;
  std::uint32_t m_bpp;
  //! Owned by the renderer; valid while the graph is.
  mutable BayerBackend* live{};
};

struct Shot
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  ReadbackImage img;
};

Shot renderMosaic(
    score::gfx::GraphicsApi api, V fmt, std::uint32_t bpp,
    const std::vector<std::uint8_t>& mosaic)
{
  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<BayerNode>(fmt, bpp);
    auto* raw = node.get();
    const int cap = p.addNode(std::move(node));
    const int sink = p.addSink({kW, kH});
    p.wire(p.nodeImageOut(cap, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }

    // Frame 1 resolves the ladder (decoder + CPU rung) and gives the backend
    // its strategy; only then can the producer land a frame.
    p.render(1);
    if(!raw->live || !raw->live->strategy)
    {
      out.error = "the renderer never engaged a capture strategy";
      return;
    }
    if(!raw->live->push(mosaic))
    {
      out.error = "the producer could not land the mosaic in a slot";
      return;
    }
    p.render(2);
    out.img = p.readback(sink);
    if(!out.img.valid())
      out.error = "empty readback";
  });
  return out;
}

void requireInterior(const Shot& s, int r, int g, int b, int tol)
{
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  for(int y = 2; y < kH - 2; y++)
    for(int x = 2; x < kW - 2; x++)
    {
      const auto px = s.img.at(x, y);
      INFO("pixel (" << x << "," << y << ") = (" << int(px[0]) << ","
                     << int(px[1]) << "," << int(px[2]) << ")");
      REQUIRE(std::abs(int(px[0]) - r) <= tol);
      REQUIRE(std::abs(int(px[1]) - g) <= tol);
      REQUIRE(std::abs(int(px[2]) - b) <= tol);
    }
}
} // namespace

TEST_CASE(
    "each 8-bit CFA order demosaics its own mosaic to the scene colour",
    "[gfx][l3][bayer]")
{
  static const CfaOrder orders[] = {
      {"RGGB", V::BayerRGGB8, {{'R', 'G'}, {'G', 'B'}}},
      {"BGGR", V::BayerBGGR8, {{'B', 'G'}, {'G', 'R'}}},
      {"GRBG", V::BayerGRBG8, {{'G', 'R'}, {'B', 'G'}}},
      {"GBRG", V::BayerGBRG8, {{'G', 'B'}, {'R', 'G'}}},
  };

  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  for(const auto& o : orders)
  {
    INFO("CFA order " << o.name);
    const auto shot = renderMosaic(be, o.format, 1, mosaic8(o));
    // A wrong phase turns (200,100,50) into a permutation like (100,200,100):
    // every channel moves by >= 50 codes, so tol 3 is decisive.
    requireInterior(shot, kR, kG, kB, 3);
  }
}

TEST_CASE(
    "high-bit-depth mosaics land at the same colour as 8-bit",
    "[gfx][l3][bayer]")
{
  static const CfaOrder rggb{"RGGB", V::BayerRGGB8, {{'R', 'G'}, {'G', 'B'}}};

  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  {
    // Full-range 16-bit: code*257 spans the lane exactly.
    INFO("BayerRGGB16, full-range");
    const auto shot = renderMosaic(be, V::BayerRGGB16, 2, mosaic16(rggb, 257));
    requireInterior(shot, kR, kG, kB, 3);
  }
  {
    // 10-bit right-aligned in the 16-bit lane, as V4L2 specifies for
    // SRGGB10: without the 65535/1023 sampleScale this renders at ~1/64
    // brightness ((3,1,0) instead of (199,100,50)).
    INFO("BayerRGGB10, right-aligned");
    const auto shot = renderMosaic(be, V::BayerRGGB10, 2, mosaic16(rggb, 4));
    // code*4/1023*255: 199.4 / 99.7 / 49.9
    requireInterior(shot, 199, 100, 50, 3);
  }
}
