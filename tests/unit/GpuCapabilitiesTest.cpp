// score::gfx::interop::GpuCapabilities — the rung-selection logic every vendor
// video strategy (AJA, DeckLink, Magewell, Rivermax) consults before choosing
// how frames reach the GPU.
//
// The probe itself needs a driver; the decisions taken on its result do not.
// Those decisions are constexpr predicates over a plain struct, so the whole
// ladder is checkable on any host. The distinctions below are the ones that
// silently degrade rather than fail: a machine that takes a rung it cannot
// sustain does not crash, it copies through the wrong path.

#include <Gfx/Graph/interop/GpuCapabilities.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <set>
#include <string>

using namespace score::gfx::interop;

TEST_CASE("AMD extension presence", "[gfx][interop][gpucaps]")
{
  CHECK_FALSE(AmdGlExtensions{}.any());

  SECTION("any one extension is enough")
  {
    AmdGlExtensions e;
    e.busAddressable = true;
    CHECK(e.any());

    e = {};
    e.externalVirtualMemory = true;
    CHECK(e.any());

    e = {};
    e.externalPhysicalMemory = true;
    CHECK(e.any());

    e = {};
    e.pinnedMemory = true;
    CHECK(e.any());
  }
}

TEST_CASE("Tier 0: direct card-to-VRAM", "[gfx][interop][gpucaps]")
{
  CHECK_FALSE(GpuCapabilities{}.hasTier0());

  SECTION("CUDA VMM alone is not enough")
  {
    GpuCapabilities c;
    c.cudaVmmSupported = true;
    CHECK_FALSE(c.hasTier0());
  }

  SECTION("nvidia_peermem alone is not enough")
  {
    GpuCapabilities c;
    c.nvidiaPeermem = true;
    CHECK_FALSE(c.hasTier0());
  }

  SECTION("both together open the NVIDIA path")
  {
    GpuCapabilities c;
    c.cudaVmmSupported = true;
    c.nvidiaPeermem = true;
    CHECK(c.hasTier0());
  }

  SECTION("AMD bus-addressable memory opens it on its own")
  {
    GpuCapabilities c;
    c.amd.busAddressable = true;
    CHECK(c.hasTier0());
  }

  SECTION("the AMD sysmem extensions do not")
  {
    // They are tier 2: host memory pinned for the GPU, not P2P into VRAM.
    for(bool AmdGlExtensions::*flag :
        {&AmdGlExtensions::externalVirtualMemory,
         &AmdGlExtensions::externalPhysicalMemory, &AmdGlExtensions::pinnedMemory})
    {
      GpuCapabilities c;
      c.amd.*flag = true;
      CHECK_FALSE(c.hasTier0());
    }
  }
}

TEST_CASE("Tier 1: NVIDIA DVP", "[gfx][interop][gpucaps]")
{
  CHECK_FALSE(GpuCapabilities{}.hasTier1Dvp());

  SECTION("the library being present is not enough without entry points")
  {
    GpuCapabilities c;
    c.dvpLoaded = true;
    CHECK_FALSE(c.hasTier1Dvp());
  }

  SECTION("entry points without the library are not enough either")
  {
    for(bool GpuCapabilities::*flag :
        {&GpuCapabilities::dvpHaveGl, &GpuCapabilities::dvpHaveD3D11,
         &GpuCapabilities::dvpHaveCuda})
    {
      GpuCapabilities c;
      c.*flag = true;
      CHECK_FALSE(c.hasTier1Dvp());
    }
  }

  SECTION("any single backend, with the library loaded, is enough")
  {
    for(bool GpuCapabilities::*flag :
        {&GpuCapabilities::dvpHaveGl, &GpuCapabilities::dvpHaveD3D11,
         &GpuCapabilities::dvpHaveCuda})
    {
      GpuCapabilities c;
      c.dvpLoaded = true;
      c.*flag = true;
      CHECK(c.hasTier1Dvp());
    }
  }
}

TEST_CASE("Tier 2: AMD pinned host memory", "[gfx][interop][gpucaps]")
{
  CHECK_FALSE(GpuCapabilities{}.hasTier2AmdPinned());

  SECTION("each sysmem pinning extension qualifies")
  {
    for(bool AmdGlExtensions::*flag :
        {&AmdGlExtensions::externalVirtualMemory,
         &AmdGlExtensions::externalPhysicalMemory, &AmdGlExtensions::pinnedMemory})
    {
      GpuCapabilities c;
      c.amd.*flag = true;
      CHECK(c.hasTier2AmdPinned());
    }
  }

  SECTION("bus-addressable memory does not — it is tier 0")
  {
    // amd.any() is true here, so the two predicates are genuinely different.
    GpuCapabilities c;
    c.amd.busAddressable = true;
    CHECK(c.amd.any());
    CHECK_FALSE(c.hasTier2AmdPinned());
    CHECK(c.hasTier0());
  }
}

TEST_CASE("NVIDIA path exclusion", "[gfx][interop][gpucaps]")
{
  auto ruled = [](GpuVendor v) {
    GpuCapabilities c;
    c.vendor = v;
    return c.rulesOutNvidiaPaths();
  };

  CHECK(ruled(GpuVendor::Amd));
  CHECK(ruled(GpuVendor::Intel));
  CHECK(ruled(GpuVendor::Apple));

  CHECK_FALSE(ruled(GpuVendor::NvidiaConsumer));
  CHECK_FALSE(ruled(GpuVendor::NvidiaProQuadro));
  CHECK_FALSE(ruled(GpuVendor::Other));

  // Documented and load-bearing: an unidentified GPU must still attempt the
  // NVIDIA paths and degrade on init failure, exactly as before the predicate
  // existed. Flipping this to `true` would silently disable DVP wherever the
  // vendor probe comes back empty -- Mesa's GL driver among them.
  CHECK_FALSE(ruled(GpuVendor::Unknown));

  SECTION("exclusion is orthogonal to the loader flags")
  {
    // dvpLoaded only says libdvp is installed; it can be installed next to a
    // Radeon.
    GpuCapabilities c;
    c.vendor = GpuVendor::Amd;
    c.dvpLoaded = true;
    c.dvpHaveGl = true;
    CHECK(c.hasTier1Dvp());
    CHECK(c.rulesOutNvidiaPaths());
  }
}

TEST_CASE("Capability enum names", "[gfx][interop][gpucaps]")
{
  SECTION("every vendor has its own non-empty name")
  {
    const GpuVendor vendors[] = {
        GpuVendor::Unknown,        GpuVendor::NvidiaConsumer,
        GpuVendor::NvidiaProQuadro, GpuVendor::Amd,
        GpuVendor::Apple,           GpuVendor::Intel,
        GpuVendor::Other};
    std::set<std::string> names;
    for(auto v : vendors)
    {
      const char* n = gpuVendorName(v);
      REQUIRE(n != nullptr);
      CHECK(std::strlen(n) > 0);
      names.insert(n);
    }
    CHECK(names.size() == std::size(vendors));
  }

  SECTION("every backend has its own non-empty name")
  {
    const QRhiBackendKind backends[] = {
        QRhiBackendKind::Unknown, QRhiBackendKind::OpenGL, QRhiBackendKind::Vulkan,
        QRhiBackendKind::D3D11,   QRhiBackendKind::D3D12,  QRhiBackendKind::Metal,
        QRhiBackendKind::Null};
    std::set<std::string> names;
    for(auto b : backends)
    {
      const char* n = qrhiBackendName(b);
      REQUIRE(n != nullptr);
      CHECK(std::strlen(n) > 0);
      names.insert(n);
    }
    CHECK(names.size() == std::size(backends));
  }

  SECTION("an out-of-range value still returns a string")
  {
    REQUIRE(gpuVendorName(GpuVendor(200)) != nullptr);
    REQUIRE(qrhiBackendName(QRhiBackendKind(200)) != nullptr);
  }
}

TEST_CASE("Context-free probe", "[gfx][interop][gpucaps]")
{
  // Whatever this host is, the probe must be safe to run with no GL context,
  // must be idempotent, and must not leave the fixed-size name buffers
  // unterminated.
  const auto a = probeContextFree();
  const auto b = probeContextFree();

  CHECK(a.os == b.os);
  CHECK(a.dvpLoaded == b.dvpLoaded);
  CHECK(a.dvpHaveGl == b.dvpHaveGl);
  CHECK(a.dvpHaveCuda == b.dvpHaveCuda);
  CHECK(a.cudaLoaded == b.cudaLoaded);
  CHECK(a.cudaVmmSupported == b.cudaVmmSupported);
  CHECK(a.nvidiaPeermem == b.nvidiaPeermem);

  CHECK(std::strlen(a.rendererName) < sizeof(a.rendererName));
  CHECK(std::strlen(a.driverVersion) < sizeof(a.driverVersion));

  SECTION("the OS label matches the platform it was built for")
  {
#if defined(_WIN32)
    CHECK(a.os == HostOs::Windows);
#elif defined(__APPLE__)
    CHECK(a.os == HostOs::MacOS);
#elif defined(__linux__)
    CHECK(a.os == HostOs::Linux);
#endif
  }

  SECTION("nvidia_peermem is a Linux-only concept")
  {
#if !defined(__linux__)
    CHECK_FALSE(a.nvidiaPeermem);
#endif
  }

  SECTION("the context-free probe leaves the GL-only fields alone")
  {
    // probeGlExtensions() is the only thing allowed to populate these; a
    // context-free probe that claimed them would send a strategy down a rung
    // it has not verified.
    CHECK_FALSE(a.amd.any());
    CHECK(a.backend == QRhiBackendKind::Unknown);
  }

  SECTION("CUDA VMM implies the CUDA driver was loaded")
  {
    if(a.cudaVmmSupported)
      CHECK(a.cudaLoaded);
  }

  SECTION("DVP entry points imply the DVP library was loaded")
  {
    if(a.dvpHaveGl || a.dvpHaveD3D11 || a.dvpHaveCuda)
      CHECK(a.dvpLoaded);
  }
}
