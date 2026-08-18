// Unit tests for the four decisions DmaBufImportCapture::init() makes about a
// producer's buffers before importing anything: whose plane layout to believe,
// what pitch to import a plane with, what pitch is large enough to accept, and
// whether the Vulkan rung may be used at all.

#if defined(__linux__)
#include <Gfx/Graph/interop/DmaBufImportCapture.hpp>
#endif

#include <catch2/catch_test_macros.hpp>

#if defined(__linux__)
using score::gfx::interop::DmaBufOrigin;
using score::gfx::interop::DmaBufSlotDesc;
namespace ic = score::gfx::interop;

namespace
{
// The NvBufSurface numbers from the Orin: a 3552x3556 NV12 frame whose chroma
// plane the allocator put at a 64 KB-aligned offset, well past where tight
// packing would have placed it.
constexpr std::uint32_t kWidth = 3552;
constexpr std::uint32_t kHeight = 3556;
constexpr std::uint32_t kStatedPitch = 3584;
constexpr std::uint32_t kStatedChromaOffset = 1966080;
constexpr std::uint32_t kDerivedChromaOffset = kWidth * kHeight;

DmaBufSlotDesc statedNv12Slot()
{
  DmaBufSlotDesc s;
  s.fd = 3;
  s.offset = 0;
  s.pitch = kStatedPitch;
  s.planeCount = 2;
  s.planes[0] = {0, kStatedPitch};
  s.planes[1] = {kStatedChromaOffset, kStatedPitch};
  return s;
}

DmaBufSlotDesc derivedNv12Slot()
{
  DmaBufSlotDesc s;
  s.fd = 3;
  s.offset = 0;
  s.pitch = kWidth;
  s.planeCount = 0;
  return s;
}
} // namespace

TEST_CASE("a producer that states its layout is believed")
{
  const auto stated = statedNv12Slot();
  REQUIRE(ic::dmaBufExplicitLayout(stated, 2));

  const auto luma
      = ic::dmaBufPlaneLayout(true, stated, 0, 0, kWidth);
  CHECK(luma.offset == 0u);
  CHECK(luma.pitch == kStatedPitch);

  // The allocator's 64 KB-aligned offset, not the accumulated one: no
  // derivation can express it, and chroma read from the derived address is a
  // different picture.
  const auto chroma = ic::dmaBufPlaneLayout(
      true, stated, 1, kDerivedChromaOffset, kWidth);
  CHECK(chroma.offset == kStatedChromaOffset);
  CHECK(chroma.offset != kDerivedChromaOffset);
  CHECK(chroma.pitch == kStatedPitch);
}

TEST_CASE("a producer that states nothing still gets the derivation")
{
  const auto derived = derivedNv12Slot();
  CHECK_FALSE(ic::dmaBufExplicitLayout(derived, 2));
  // One plane is all a single-texture decoder asks for, and a slot that says
  // nothing cannot cover it either.
  CHECK_FALSE(ic::dmaBufExplicitLayout(derived, 1));

  const auto luma = ic::dmaBufPlaneLayout(false, derived, 0, 0, kWidth);
  CHECK(luma.offset == 0u);
  CHECK(luma.pitch == kWidth);

  const auto chroma
      = ic::dmaBufPlaneLayout(false, derived, 1, kDerivedChromaOffset, kWidth);
  CHECK(chroma.offset == kDerivedChromaOffset);
  CHECK(chroma.pitch == kWidth);
}

TEST_CASE("a partial stated layout is not a stated layout")
{
  auto s = derivedNv12Slot();
  s.planeCount = 1;
  // One plane described out of the two the decoder wants: believing that would
  // read chroma from planes[1], which the producer never filled in.
  CHECK_FALSE(ic::dmaBufExplicitLayout(s, 2));
  CHECK(ic::dmaBufExplicitLayout(s, 1));
}

// The Tegra VI reports bytesperline 7168 for a 3552-wide 16-bit raster whose
// packed width is 7104. Importing that as packed shifts every row by 64 bytes
// more than the last, so the picture shears rather than failing.
TEST_CASE("a single-plane frame is imported with the producer's own pitch")
{
  DmaBufSlotDesc padded;
  padded.fd = 3;
  padded.pitch = 7168;
  padded.planeCount = 0;
  CHECK(ic::dmaBufUsesProducerPitch(1, padded));

  // Multi-plane is unaffected: that path refuses a padded slot outright,
  // because it has no way to derive where the next plane starts.
  CHECK_FALSE(ic::dmaBufUsesProducerPitch(2, padded));
  CHECK_FALSE(ic::dmaBufUsesProducerPitch(3, padded));

  // A producer that stated its layout has already said what the pitch is, per
  // plane; its slot-level pitch is not the answer for any of them.
  CHECK_FALSE(ic::dmaBufUsesProducerPitch(1, statedNv12Slot()));

  DmaBufSlotDesc silent;
  silent.fd = 3;
  silent.pitch = 0;
  CHECK_FALSE(ic::dmaBufUsesProducerPitch(1, silent));
}

// The external form's texture format is nominal RGBA8 over what are really NV12
// luma bytes, so validating the producer's pitch against it compares a 4-byte
// row against a 1-byte one and rejects a buffer that was always fine.
TEST_CASE("the external form validates the pitch against the real format")
{
  CHECK(ic::dmaBufMinPitch(true, kWidth, 4) == kWidth);
  CHECK(ic::dmaBufMinPitch(false, kWidth, 4) == kWidth * 4);
  CHECK(kStatedPitch >= ic::dmaBufMinPitch(true, kWidth, 4));
  CHECK(kStatedPitch < ic::dmaBufMinPitch(false, kWidth, 4));

  // The per-plane form is untouched: an RGBA8 texture really is four bytes a
  // pixel when it is not standing in for something else.
  CHECK(ic::dmaBufMinPitch(false, 1920, 4) == 7680u);
  CHECK(ic::dmaBufMinPitch(false, 1920, 1) == 1920u);
  CHECK(ic::dmaBufMinPitch(true, 1920, 4) == 1920u);
}

// importExternal's first guard is planeCount == 0, so the "derive it" sentinel
// has to be expanded here: V4L2-style producers never fill planes[].
TEST_CASE("an external import with no stated layout describes one plane")
{
  DmaBufSlotDesc s;
  s.fd = 3;
  s.offset = 4096;
  s.pitch = 3584;
  s.planeCount = 0;

  const auto ext = ic::dmaBufExternalPlanes(s);
  CHECK(ext.count == 1u);
  CHECK(ext.offsets[0] == 4096u);
  CHECK(ext.pitches[0] == 3584u);
}

TEST_CASE("a stated external layout is offset by the slot's own base")
{
  const auto ext = ic::dmaBufExternalPlanes(statedNv12Slot());
  CHECK(ext.count == 2u);
  CHECK(ext.offsets[0] == 0u);
  CHECK(ext.offsets[1] == kStatedChromaOffset);
  CHECK(ext.pitches[0] == kStatedPitch);
  CHECK(ext.pitches[1] == kStatedPitch);

  auto shifted = statedNv12Slot();
  shifted.offset = 8192;
  const auto ext2 = ic::dmaBufExternalPlanes(shifted);
  CHECK(ext2.offsets[0] == 8192u);
  CHECK(ext2.offsets[1] == kStatedChromaOffset + 8192u);
}

TEST_CASE("more stated planes than the descriptor holds are clamped")
{
  auto s = statedNv12Slot();
  s.planeCount = 7;
  const auto ext = ic::dmaBufExternalPlanes(s);
  CHECK(ext.count == 3u);
}

// Refusing on the driver id alone also refused GBM and NvBufSurface, which
// measured 10/10 byte-exact on the same driver. What decides it is who
// allocated the buffer.
TEST_CASE("the Vulkan rung is gated on the exporter, not the driver")
{
  constexpr auto nvidia = ic::kNvidiaProprietaryDriverId;
  constexpr std::uint32_t mesa = 3;

  CHECK(ic::dmaBufVulkanRungRefused(DmaBufOrigin::ForeignDevice, nvidia));
  CHECK_FALSE(ic::dmaBufVulkanRungRefused(DmaBufOrigin::GpuAllocated, nvidia));
  CHECK_FALSE(ic::dmaBufVulkanRungRefused(DmaBufOrigin::ForeignDevice, mesa));
  CHECK_FALSE(ic::dmaBufVulkanRungRefused(DmaBufOrigin::GpuAllocated, mesa));
  CHECK_FALSE(ic::dmaBufVulkanRungRefused(DmaBufOrigin::ForeignDevice, 0));
}

#else
TEST_CASE("dma-buf import is a Linux path")
{
  SUCCEED();
}
#endif
