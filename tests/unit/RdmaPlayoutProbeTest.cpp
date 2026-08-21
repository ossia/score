// The gate that stands between an RDMA output rung and a blank transmission.
//
// The failure this pins: the AJA RDMA-GL / RDMA-Vulkan output shims probed the
// playout P2P path with `card->DMAWriteFrame(...)` and believed its return
// code. A dropped peer-to-peer read returns success, so the rung engaged, the
// card played out whatever its framestore already held, and the pump reported
// a full frame count with zero drops. Counters, "a device appeared" and
// non-blackness all pass in that state; only comparing the delivered bytes
// against the bytes that were handed over does not.
//
// The vendor DMA calls are the injected halves here, so the decision table runs
// on any host: a peer that returns exactly what it was given, one that returns
// a constant (the observed symptom), one that returns nothing, and one that
// offers no probe at all.

#include <Gfx/Graph/interop/RdmaPlayoutProbe.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

using namespace score::gfx::interop;

namespace
{
constexpr std::uint32_t kFrameBytes = 1920u * 1080u * 2u;

// Stands in for the pinned CUDA bounce buffer.
struct FakeGpuBuffer
{
  std::vector<unsigned char> mem = std::vector<unsigned char>(kFrameBytes, 0);
  void* ptr() { return mem.data(); }
};

// Stands in for the peer device's scratch framestore.
struct FakePeer
{
  std::vector<unsigned char> framestore = std::vector<unsigned char>(kFrameBytes, 0);
  bool acceptsTransfer{true};
  bool acceptsReadback{true};
  bool deliversPayload{true};

  VendorDmaRegistrar registrar(bool withReadback)
  {
    VendorDmaRegistrar r;
    r.registerSlot = [](void*, std::uint32_t) { return true; };
    r.releaseSlot = [](void*, std::uint32_t) { };
    r.verifyTransfer = [this](void* gpu, std::uint32_t n) {
      if(!acceptsTransfer)
        return false;
      if(deliversPayload)
        std::memcpy(
            framestore.data(), gpu, n < framestore.size() ? n : framestore.size());
      // else: the write is silently dropped and the framestore keeps its
      // previous contents — exactly what a blocked P2P read looks like.
      return true;
    };
    if(withReadback)
      r.readbackTransfer = [this](void* host, std::uint32_t n) {
        if(!acceptsReadback)
          return false;
        std::memcpy(host, framestore.data(), n);
        return true;
      };
    return r;
  }
};

RdmaPlayoutProbeIo hostSeedIo()
{
  RdmaPlayoutProbeIo io;
  io.seedGpu = [](void* dst, const void* src, std::uint32_t n) {
    std::memcpy(dst, src, n);
    return true;
  };
  return io;
}
}

TEST_CASE("the probe pattern cannot alias a dropped transfer", "[gfx][interop][rdma]")
{
  std::set<unsigned char> values;
  for(std::uint32_t i = 0; i < rdmaPlayoutProbeMaxBytes; ++i)
    values.insert(rdmaPlayoutProbeByte(i));

  // A constant framestore, a zero-filled scratch and a 0xFF-filled one are the
  // three shapes a dropped transfer leaves behind. None of them may compare
  // equal to the pattern, so the pattern must not be constant.
  CHECK(values.size() > 1);
}

TEST_CASE("a peer that returns the bytes it was given engages the rung",
          "[gfx][interop][rdma]")
{
  FakeGpuBuffer gpu;
  FakePeer peer;

  const auto r = rdmaProbePlayoutPath(
      gpu.ptr(), kFrameBytes, peer.registrar(/*withReadback=*/true), hostSeedIo());

  CHECK(r == RdmaPlayoutProbeResult::Delivered);
  CHECK(rdmaPlayoutProbeEngages(r));
}

TEST_CASE("a silently dropped playout transfer must refuse the rung",
          "[gfx][interop][rdma]")
{
  // The bug, reproduced: every call succeeds, the peer keeps playing out the
  // frame it already held, and nothing but the bytes reveals it.
  FakeGpuBuffer gpu;
  FakePeer peer;
  peer.deliversPayload = false;

  SECTION("framestore holding a previous constant frame")
  {
    std::fill(peer.framestore.begin(), peer.framestore.end(), 0x10);

    const auto r = rdmaProbePlayoutPath(
        gpu.ptr(), kFrameBytes, peer.registrar(/*withReadback=*/true),
        hostSeedIo());

    CHECK(r == RdmaPlayoutProbeResult::ContentMismatch);
    CHECK_FALSE(rdmaPlayoutProbeEngages(r));
  }

  SECTION("zero-filled framestore")
  {
    const auto r = rdmaProbePlayoutPath(
        gpu.ptr(), kFrameBytes, peer.registrar(/*withReadback=*/true),
        hostSeedIo());

    CHECK(r == RdmaPlayoutProbeResult::ContentMismatch);
    CHECK_FALSE(rdmaPlayoutProbeEngages(r));
  }

  SECTION("a single wrong byte anywhere in the probe window is enough")
  {
    peer.deliversPayload = true;
    VendorDmaRegistrar reg = peer.registrar(/*withReadback=*/true);
    const auto push = reg.verifyTransfer;
    reg.verifyTransfer = [&](void* p, std::uint32_t n) {
      const bool ok = push(p, n);
      peer.framestore[rdmaPlayoutProbeMaxBytes / 2] ^= 0xFF;
      return ok;
    };

    const auto r
        = rdmaProbePlayoutPath(gpu.ptr(), kFrameBytes, reg, hostSeedIo());

    CHECK(r == RdmaPlayoutProbeResult::ContentMismatch);
    CHECK_FALSE(rdmaPlayoutProbeEngages(r));
  }
}

TEST_CASE("a vendor with no playout probe does not get an RDMA rung",
          "[gfx][interop][rdma]")
{
  FakeGpuBuffer gpu;
  FakePeer peer;
  VendorDmaRegistrar reg = peer.registrar(/*withReadback=*/false);
  reg.verifyTransfer = nullptr;

  const auto r = rdmaProbePlayoutPath(gpu.ptr(), kFrameBytes, reg, hostSeedIo());

  CHECK(r == RdmaPlayoutProbeResult::NoProbe);
  CHECK_FALSE(rdmaPlayoutProbeEngages(r));
}

TEST_CASE("a return-code-only vendor engages but is reported as unverified",
          "[gfx][interop][rdma]")
{
  FakeGpuBuffer gpu;
  FakePeer peer;
  peer.deliversPayload = false;

  const auto r = rdmaProbePlayoutPath(
      gpu.ptr(), kFrameBytes, peer.registrar(/*withReadback=*/false), hostSeedIo());

  CHECK(r == RdmaPlayoutProbeResult::Unverified);
  CHECK(rdmaPlayoutProbeEngages(r));
  CHECK(std::strstr(rdmaPlayoutProbeMessage(r), "readbackTransfer") != nullptr);
}

TEST_CASE("transfer and readback failures refuse the rung", "[gfx][interop][rdma]")
{
  FakeGpuBuffer gpu;

  SECTION("the peer refuses the transfer")
  {
    FakePeer peer;
    peer.acceptsTransfer = false;
    const auto r = rdmaProbePlayoutPath(
        gpu.ptr(), kFrameBytes, peer.registrar(true), hostSeedIo());
    CHECK(r == RdmaPlayoutProbeResult::TransferFailed);
    CHECK_FALSE(rdmaPlayoutProbeEngages(r));
  }

  SECTION("the peer cannot return what it was given")
  {
    FakePeer peer;
    peer.acceptsReadback = false;
    const auto r = rdmaProbePlayoutPath(
        gpu.ptr(), kFrameBytes, peer.registrar(true), hostSeedIo());
    CHECK(r == RdmaPlayoutProbeResult::ReadbackFailed);
    CHECK_FALSE(rdmaPlayoutProbeEngages(r));
  }

  SECTION("the pattern cannot be written into the pinned buffer")
  {
    FakePeer peer;
    RdmaPlayoutProbeIo io;
    io.seedGpu = [](void*, const void*, std::uint32_t) { return false; };
    const auto r
        = rdmaProbePlayoutPath(gpu.ptr(), kFrameBytes, peer.registrar(true), io);
    CHECK(r == RdmaPlayoutProbeResult::SeedFailed);
    CHECK_FALSE(rdmaPlayoutProbeEngages(r));
  }
}

TEST_CASE("the probe window is bounded but crosses several GPU pages",
          "[gfx][interop][rdma]")
{
  CHECK(rdmaPlayoutProbeMaxBytes >= 64u * 1024u);

  // A frame smaller than the window must still be probed over its whole length
  // rather than reading past its end.
  FakeGpuBuffer gpu;
  FakePeer peer;
  const std::uint32_t small = 4096;
  const auto r = rdmaProbePlayoutPath(
      gpu.ptr(), small, peer.registrar(true), hostSeedIo());
  CHECK(r == RdmaPlayoutProbeResult::Delivered);
}
