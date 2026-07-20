// =============================================================================
// UNIT — GPUBufferScatter dispatch-dims clamp + shader index linearization
// (finding R2-#5 / F5, commit 0efeab422).
//
// FINDING: GPUBufferScatter::dispatch() issued cb.dispatch(workgroups, 1, 1)
// with the ENTIRE workgroup count on the X axis and no clamp against the
// backend's per-dimension limit (65535). element_count is the per-attribute
// vertex/instance count of geometry being format-converted (e.g. float3->float4
// widening for a point cloud), so it is content-controlled and can exceed
// 65535*256 = 16,776,960. An X dispatch of > 65535 groups is invalid:
// GL_INVALID_VALUE (dispatch silently dropped) / VUID-vkCmdDispatch-groupCountX
// (validation error / lost device).
//
// THE FIX: computeDispatchDims() spreads the workgroup count across Y (and Z)
// so no axis exceeds the backend max, and the scatter shader reconstructs the
// linear element index from all three gl_GlobalInvocationID components using
// num_workgroups_x / num_workgroups_y fed through the params UBO:
//     width_x = num_workgroups_x * localX          (localX = 256)
//     i = (gid.z * num_workgroups_y + gid.y) * width_x + gid.x
//
// WHAT THIS TEST DOES / HONEST SCOPE:
//   computeDispatchDims() is a PRIVATE const member and the linearization lives
//   inside a GLSL string; neither is externally callable, and the RED-proof
//   reverts GPUBufferScatter.hpp/.cpp (pre-fix the helper did not even exist).
//   So this is a UNIT test of the ALGORITHM: it REPLICATES computeDispatchDims'
//   exact formula (with the default backend max of 65535, as set in the header)
//   and the shader's index reconstruction, then asserts the three contract
//   properties across element counts that span the 65535 boundary:
//     (a) COVERAGE      — the grid dispatches at least element_count threads
//                         (no element left unprocessed);
//     (b) AXIS BOUNDS   — every axis (x,y,z) is <= the backend max (the actual
//                         bug: pre-fix x could be 65536 > 65535);
//     (c) BIJECTION     — the shader linearization maps the dispatched thread
//                         box one-to-one onto [0, total_threads), so every
//                         element index in [0, element_count) is written by
//                         exactly one thread (no gaps, no overlaps), and
//                         over-dispatched threads (i >= element_count) are the
//                         only slack.
//   Because it replicates rather than links the private helper, this test does
//   NOT flip RED on the reverted engine — its value is encoding the fix's exact
//   contract as an executable proof. The C++/GLSL formulas below are copied
//   verbatim from GPUBufferScatter.cpp; if the production formula changes, this
//   reference must be updated in lockstep.
// =============================================================================
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
// ---- Verbatim replica of GPUBufferScatter internals -------------------------
constexpr int kLocalSize = 256;                 // GPUBufferScatter::LocalSize
constexpr int64_t kMaxWorkgroupsPerDim = 65535; // header default m_maxWorkgroupsPerDim

struct DispatchDims
{
  int x{};
  int y{};
  int z{};
};

// Verbatim copy of GPUBufferScatter::computeDispatchDims() (GPUBufferScatter.cpp).
DispatchDims computeDispatchDims(uint32_t element_count)
{
  const int64_t maxWorkgroups = kMaxWorkgroupsPerDim;
  const int64_t totalWorkgroups
      = (static_cast<int64_t>(element_count) + kLocalSize - 1) / kLocalSize;

  DispatchDims d{0, 0, 0};
  if(totalWorkgroups <= 0)
    return d;

  if(totalWorkgroups > maxWorkgroups * maxWorkgroups)
  {
    d.x = static_cast<int>(maxWorkgroups);
    const int64_t remaining
        = (totalWorkgroups + maxWorkgroups - 1) / maxWorkgroups;
    d.y = static_cast<int>(std::min<int64_t>(remaining, maxWorkgroups));
    d.z = static_cast<int>((remaining + maxWorkgroups - 1) / maxWorkgroups);
  }
  else if(totalWorkgroups > maxWorkgroups)
  {
    d.x = static_cast<int>(std::min<int64_t>(totalWorkgroups, maxWorkgroups));
    d.y = static_cast<int>((totalWorkgroups + maxWorkgroups - 1) / maxWorkgroups);
    d.z = 1;
  }
  else
  {
    d.x = static_cast<int>(totalWorkgroups);
    d.y = 1;
    d.z = 1;
  }
  return d;
}

// Verbatim replica of the scatter shader's linear-index reconstruction:
//   width_x = num_workgroups_x * gl_WorkGroupSize.x
//   i = (gid.z * num_workgroups_y + gid.y) * width_x + gid.x
// (num_workgroups_x/y == dims.x/y written to the UBO by updateParams()).
int64_t shaderLinearIndex(
    const DispatchDims& dims, int64_t gid_x, int64_t gid_y, int64_t gid_z)
{
  const int64_t width_x = static_cast<int64_t>(dims.x) * kLocalSize;
  const int64_t num_workgroups_y = dims.y;
  return (gid_z * num_workgroups_y + gid_y) * width_x + gid_x;
}

int64_t totalThreads(const DispatchDims& d)
{
  return static_cast<int64_t>(d.x) * kLocalSize * d.y * d.z;
}

// Assert the three contract properties for one element_count. `sweep` fully
// walks the dispatched thread box (only for counts small enough to be cheap)
// and verifies the linearization is the canonical row-major flatten -> a
// bijection onto [0, total_threads).
void checkDims(uint32_t element_count, bool sweep)
{
  CAPTURE(element_count);
  const DispatchDims d = computeDispatchDims(element_count);

  if(element_count == 0)
  {
    // Zero elements -> no dispatch. computeDispatchDims returns {0,0,0} and
    // dispatch() skips the (invalid) empty dispatch (guard dims.x<=0 ...).
    CHECK(d.x == 0);
    CHECK(d.y == 0);
    CHECK(d.z == 0);
    return;
  }

  // (b) AXIS BOUNDS — the actual bug guard: no axis may exceed the backend max.
  CHECK(int64_t(d.x) <= kMaxWorkgroupsPerDim);
  CHECK(int64_t(d.y) <= kMaxWorkgroupsPerDim);
  CHECK(int64_t(d.z) <= kMaxWorkgroupsPerDim);
  CHECK(d.x >= 1);
  CHECK(d.y >= 1);
  CHECK(d.z >= 1);

  const int64_t total = totalThreads(d);

  // (a) COVERAGE — every element gets a thread.
  CHECK(total >= int64_t(element_count));

  // Tightness: the grid must not massively over-provision. The X axis rounds up
  // to whole workgroups (kLocalSize), and Y/Z round up to whole rows/planes, so
  // the slack is bounded by one extra plane of threads.
  const int64_t planeThreads = int64_t(d.x) * kLocalSize * d.y;
  CHECK(total - int64_t(element_count) < planeThreads + int64_t(kLocalSize));

  if(sweep)
  {
    // (c) BIJECTION — walk the box in (z,y,x) order; the shader index must be
    // exactly the running counter, proving a one-to-one map onto [0, total).
    // Then every element index in [0, element_count) is hit exactly once.
    const int64_t width_x = int64_t(d.x) * kLocalSize;
    int64_t counter = 0;
    bool ok = true;
    for(int64_t z = 0; z < d.z && ok; ++z)
      for(int64_t y = 0; y < d.y && ok; ++y)
        for(int64_t gx = 0; gx < width_x && ok; ++gx)
        {
          if(shaderLinearIndex(d, gx, y, z) != counter)
            ok = false;
          ++counter;
        }
    CHECK(ok);
    CHECK(counter == total);
    // The last element index is produced, and it is < total (in range).
    if(element_count > 0)
      CHECK(int64_t(element_count) - 1 < total);
  }
}
} // namespace

TEST_CASE(
    "GPUBufferScatter dispatch: dims cover, stay in-bounds, linearize bijectively",
    "[gfx][unit][scatter][dispatch]")
{
  // Small / boundary-of-a-single-workgroup counts (full bijection sweep).
  for(uint32_t n : {0u, 1u, 255u, 256u, 257u, 512u, 65535u, 65536u})
    checkDims(n, /*sweep*/ true);
}

TEST_CASE(
    "GPUBufferScatter dispatch: counts spanning the 65535-workgroup boundary",
    "[gfx][unit][scatter][dispatch]")
{
  // Just BELOW the X-axis overflow: 65535 workgroups -> still a 1-D dispatch.
  {
    const uint32_t n = 65535u * 256u; // 16,776,960 -> exactly 65535 workgroups
    const DispatchDims d = computeDispatchDims(n);
    CHECK(d.x == 65535);
    CHECK(d.y == 1);
    CHECK(d.z == 1);
    checkDims(n, /*sweep*/ true);
  }

  // One element past that: 65536 workgroups would overflow the X axis pre-fix.
  // The fix must spread onto Y (x clamped to <= 65535, y >= 2).
  {
    const uint32_t n = 65535u * 256u + 1u;
    const DispatchDims d = computeDispatchDims(n);
    CHECK(int64_t(d.x) <= 65535);
    CHECK(d.y >= 2);
    checkDims(n, /*sweep*/ true);
  }

  // The canonical bug scenario from the finding: 65536*256 = 16,777,216
  // elements -> 65536 workgroups. Naive single-axis dispatch = 65536 > 65535
  // (INVALID). The fix keeps x <= 65535 and spreads the remainder onto Y.
  {
    const uint32_t n = 65536u * 256u; // 16,777,216
    const int64_t naiveWorkgroups = (int64_t(n) + kLocalSize - 1) / kLocalSize;
    REQUIRE(naiveWorkgroups == 65536);      // would overflow the X axis...
    REQUIRE(naiveWorkgroups > 65535);       // ...confirming the pre-fix bug case
    const DispatchDims d = computeDispatchDims(n);
    CHECK(int64_t(d.x) <= 65535);
    CHECK(d.y >= 2);
    checkDims(n, /*sweep*/ true);
  }

  // Large content-controlled count from the finding (20,000,000 vertices).
  checkDims(20'000'000u, /*sweep*/ true);
}

TEST_CASE(
    "GPUBufferScatter dispatch: near-uint32 max stays within per-axis limits",
    "[gfx][unit][scatter][dispatch]")
{
  // The largest possible element_count for a uint32 field. totalWorkgroups
  // ~= ceil(2^32 / 256) ~= 16.78M, still well below 65535^2 (~4.29e9), so only
  // the Y spread is exercised; Z stays 1. A full sweep of 16.8M+ threads is
  // still cheap. Axis bounds + coverage are the load-bearing assertions here.
  const uint32_t n = 0xFFFFFFFFu;
  const DispatchDims d = computeDispatchDims(n);
  CHECK(int64_t(d.x) <= kMaxWorkgroupsPerDim);
  CHECK(int64_t(d.y) <= kMaxWorkgroupsPerDim);
  CHECK(int64_t(d.z) <= kMaxWorkgroupsPerDim);
  CHECK(totalThreads(d) >= int64_t(n));
  checkDims(n, /*sweep*/ false); // property-only; box is large but algebra holds
}
