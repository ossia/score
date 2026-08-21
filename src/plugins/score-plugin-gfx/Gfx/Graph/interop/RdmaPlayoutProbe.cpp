#include <Gfx/Graph/interop/RdmaPlayoutProbe.hpp>

#include <algorithm>
#include <vector>

namespace score::gfx::interop
{

unsigned char rdmaPlayoutProbeByte(std::uint32_t index) noexcept
{
  return static_cast<unsigned char>(
      (0x5Au ^ (index * 31u + (index >> 7) * 17u + 11u)) & 0xFFu);
}

RdmaPlayoutProbeResult rdmaProbePlayoutPath(
    void* pinnedGpuPtr, std::uint32_t frameByteSize,
    const VendorDmaRegistrar& registrar, const RdmaPlayoutProbeIo& io) noexcept
{
  if(!registrar.verifyTransfer)
    return RdmaPlayoutProbeResult::NoProbe;
  if(!pinnedGpuPtr || frameByteSize == 0 || !io.seedGpu)
    return RdmaPlayoutProbeResult::SeedFailed;

  const std::uint32_t probeBytes
      = std::min(frameByteSize, rdmaPlayoutProbeMaxBytes);

  std::vector<unsigned char> pattern(probeBytes);
  for(std::uint32_t i = 0; i < probeBytes; ++i)
    pattern[i] = rdmaPlayoutProbeByte(i);

  if(!io.seedGpu(pinnedGpuPtr, pattern.data(), probeBytes))
    return RdmaPlayoutProbeResult::SeedFailed;

  if(!registrar.verifyTransfer(pinnedGpuPtr, frameByteSize))
    return RdmaPlayoutProbeResult::TransferFailed;

  if(!registrar.readbackTransfer)
    return RdmaPlayoutProbeResult::Unverified;

  std::vector<unsigned char> back(probeBytes, 0);
  if(!registrar.readbackTransfer(back.data(), probeBytes))
    return RdmaPlayoutProbeResult::ReadbackFailed;

  if(back != pattern)
    return RdmaPlayoutProbeResult::ContentMismatch;

  return RdmaPlayoutProbeResult::Delivered;
}

bool rdmaPlayoutProbeEngages(RdmaPlayoutProbeResult r) noexcept
{
  switch(r)
  {
    case RdmaPlayoutProbeResult::Delivered:
    case RdmaPlayoutProbeResult::Unverified:
      return true;
    case RdmaPlayoutProbeResult::NoProbe:
    case RdmaPlayoutProbeResult::SeedFailed:
    case RdmaPlayoutProbeResult::TransferFailed:
    case RdmaPlayoutProbeResult::ReadbackFailed:
    case RdmaPlayoutProbeResult::ContentMismatch:
      return false;
  }
  return false;
}

const char* rdmaPlayoutProbeMessage(RdmaPlayoutProbeResult r) noexcept
{
  switch(r)
  {
    case RdmaPlayoutProbeResult::Delivered:
      return "playout P2P verified byte-for-byte";
    case RdmaPlayoutProbeResult::Unverified:
      return "playout P2P rests on a return code only — the vendor adapter "
             "supplies no VendorDmaRegistrar::readbackTransfer, so a silently "
             "dropped transfer would play out a constant frame with zero drops";
    case RdmaPlayoutProbeResult::NoProbe:
      return "the vendor adapter supplies no VendorDmaRegistrar::verifyTransfer; "
             "refusing to engage a P2P playout path that was never exercised";
    case RdmaPlayoutProbeResult::SeedFailed:
      return "the probe pattern could not be written into the pinned GPU buffer";
    case RdmaPlayoutProbeResult::TransferFailed:
      return "the peer device refused the transfer out of the pinned GPU buffer";
    case RdmaPlayoutProbeResult::ReadbackFailed:
      return "the peer device could not return what it was given";
    case RdmaPlayoutProbeResult::ContentMismatch:
      return "the peer device returned different bytes than it was given: the "
             "pin and every return code succeeded but the card↔GPU PCIe path "
             "does not actually deliver playout data";
  }
  return "unknown playout probe result";
}

} // namespace score::gfx::interop
