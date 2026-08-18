// Unit tests for Gfx/Graph/interop/HostFramePool.{hpp,cpp}: the app-owned
// pool of pinned host frames behind a FrameMemoryProvider.
//
// Everything the readback target relies on is a promise this file makes and
// nothing checks: that each frame is one whole allocation with no interior
// offset, that it is aligned to the granule the GPU wrap needs, that an
// exhausted pool answers with an empty frame rather than allocating one, that
// recycle() of a pointer from some other path is inert, and that a pool which
// fails halfway through allocation unpins exactly what it pinned.
//
// A stub registrar stands in for the card's page-lock calls; there is no
// device, no GPU and no application here.

#include <Gfx/Graph/interop/HostFramePool.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

using score::gfx::interop::HostFramePool;
using score::gfx::interop::VendorDmaRegistrar;

namespace
{
// The platform wrap granule HostFramePool.cpp rounds to: 64 KiB where
// VirtualAlloc is the allocator D3D12 accepts, a page elsewhere.
#if defined(_WIN32)
constexpr std::size_t kGranule = 65536;
#else
constexpr std::size_t kGranule = 4096;
#endif

struct StubRegistrar
{
  std::vector<std::pair<void*, std::uint32_t>> pinned;
  std::vector<std::pair<void*, std::uint32_t>> unpinned;
  int failOnPin{-1}; ///< 1-based index of the pin that fails; -1 = never

  VendorDmaRegistrar make()
  {
    VendorDmaRegistrar r;
    r.registerSlot = [this](void* p, std::uint32_t n) {
      if(failOnPin > 0 && int(pinned.size()) + 1 == failOnPin)
        return false;
      pinned.push_back({p, n});
      return true;
    };
    r.releaseSlot
        = [this](void* p, std::uint32_t n) { unpinned.push_back({p, n}); };
    return r;
  }
};
} // namespace

TEST_CASE("a pool hands out distinct, granule-aligned whole allocations")
{
  StubRegistrar reg;
  HostFramePool pool;
  REQUIRE(pool.allocate(1920 * 1080 * 4, 4, reg.make()));
  REQUIRE(pool.valid());
  REQUIRE(reg.pinned.size() == 4);

  auto provider = pool.provider();
  REQUIRE(bool(provider));

  std::vector<void*> bases;
  for(int i = 0; i < 4; ++i)
  {
    const auto f = provider.acquire();
    INFO("frame " << i);
    REQUIRE(bool(f));
    // The readback target wraps the whole region and writes at offset 0; a
    // frame that was an interior slice of a bigger allocation would put the
    // picture somewhere else in it.
    CHECK(f.bytes == f.regionBase);
    CHECK(f.regionBytes >= std::size_t(1920 * 1080 * 4));
    CHECK(f.regionBytes % kGranule == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(f.bytes) % kGranule == 0);
    CHECK(pool.owns(f.bytes));
    bases.push_back(f.bytes);
  }

  std::sort(bases.begin(), bases.end());
  CHECK(std::unique(bases.begin(), bases.end()) == bases.end());
}

TEST_CASE("an exhausted pool drops the frame instead of growing")
{
  StubRegistrar reg;
  HostFramePool pool;
  REQUIRE(pool.allocate(4096, 2, reg.make()));

  auto provider = pool.provider();
  const auto a = provider.acquire();
  const auto b = provider.acquire();
  REQUIRE(bool(a));
  REQUIRE(bool(b));

  // That render tick's back-pressure: an empty frame, not an allocation.
  const auto c = provider.acquire();
  CHECK_FALSE(bool(c));
  CHECK(c.bytes == nullptr);
  CHECK(c.regionBytes == 0u);

  provider.cancel(a.bytes);
  const auto d = provider.acquire();
  REQUIRE(bool(d));
  CHECK(d.bytes == a.bytes);
}

TEST_CASE("recycling a pointer the pool never owned is inert")
{
  StubRegistrar reg;
  HostFramePool pool;
  REQUIRE(pool.allocate(4096, 1, reg.make()));

  int onTheStack = 0;
  CHECK_FALSE(pool.owns(&onTheStack));

  // Staging-ring and GPU-direct frame pointers reach the same cancel hook;
  // seeing one must not free a pool slot that is still out on loan.
  auto provider = pool.provider();
  const auto only = provider.acquire();
  REQUIRE(bool(only));
  provider.cancel(&onTheStack);
  provider.cancel(nullptr);
  CHECK_FALSE(bool(provider.acquire()));

  provider.cancel(only.bytes);
  CHECK(bool(provider.acquire()));
}

// The pin is a real syscall against the card's driver and can fail on the third
// frame as easily as the first. What must not survive that is a half-built pool
// -- or a pinned page nobody remembers to unpin.
TEST_CASE("a failed pin rolls the whole pool back")
{
  StubRegistrar reg;
  reg.failOnPin = 3;

  HostFramePool pool;
  CHECK_FALSE(pool.allocate(4096, 4, reg.make()));
  CHECK_FALSE(pool.valid());
  CHECK_FALSE(bool(pool.provider().acquire()));

  CHECK(reg.pinned.size() == 2);
  REQUIRE(reg.unpinned.size() == reg.pinned.size());
  for(std::size_t i = 0; i < reg.pinned.size(); ++i)
  {
    INFO("pinned frame " << i);
    CHECK(reg.pinned[i].first == reg.unpinned[i].first);
    CHECK(reg.pinned[i].second == reg.unpinned[i].second);
  }
}

TEST_CASE("release unpins every frame, and a second allocate starts clean")
{
  StubRegistrar reg;
  StubRegistrar second;
  HostFramePool pool;
  REQUIRE(pool.allocate(4096, 3, reg.make()));
  REQUIRE(reg.pinned.size() == 3);

  pool.release();
  CHECK_FALSE(pool.valid());
  CHECK(reg.unpinned.size() == 3);

  REQUIRE(pool.allocate(8192, 2, second.make()));
  CHECK(second.pinned.size() == 2);
  // The first registrar's callbacks belong to a device handle that may already
  // be closed; re-allocating must not reach for them again.
  CHECK(reg.unpinned.size() == 3);
}

TEST_CASE("degenerate allocation requests are refused")
{
  StubRegistrar reg;
  HostFramePool pool;
  CHECK_FALSE(pool.allocate(0, 4, reg.make()));
  CHECK_FALSE(pool.allocate(4096, 0, reg.make()));
  CHECK_FALSE(pool.allocate(4096, -1, reg.make()));
  CHECK_FALSE(pool.valid());
  CHECK(reg.pinned.empty());
}
