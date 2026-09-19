#pragma once

/* The N x M gain applier.
 *
 * Ported from SpatGRIS's sg_VbapSpatAlgorithm.cpp (GRIS / SAT, GPLv3), which
 * is where the gain interpolation lives. Kept as a free function over a gain
 * matrix rather than fused into the VBAP/MBAP path, so that a gain vector
 * computed elsewhere (DBAP, GBAP, an external inlet) could be applied through
 * exactly the same interpolation later on -- see the porting plan, 5d.4.
 *
 * Spatialisation here is a dense mix-accumulate: every source contributes to
 * every speaker, with a per-(source, speaker) gain. Nothing is rerouted at
 * runtime; the topology is fixed and the matrix is the spatialisation.
 */

#include <Gris/Algo/Types.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

namespace Gris
{
//==============================================================================
/** Per-source, per-speaker gains. Row-major, `numSpeakers` wide. */
class GainMatrix
{
public:
  void resize(std::size_t numSources, std::size_t numSpeakers)
  {
    m_numSources = numSources;
    m_numSpeakers = numSpeakers;
    m_gains.assign(numSources * numSpeakers, 0.f);
  }

  void clear() noexcept
  {
    for(auto& g : m_gains)
      g = 0.f;
  }

  [[nodiscard]] float* row(std::size_t source) noexcept
  {
    return m_gains.data() + source * m_numSpeakers;
  }
  [[nodiscard]] float const* row(std::size_t source) const noexcept
  {
    return m_gains.data() + source * m_numSpeakers;
  }

  [[nodiscard]] std::size_t numSources() const noexcept { return m_numSources; }
  [[nodiscard]] std::size_t numSpeakers() const noexcept { return m_numSpeakers; }
  [[nodiscard]] bool empty() const noexcept { return m_gains.empty(); }

private:
  std::vector<float> m_gains{};
  std::size_t m_numSources{};
  std::size_t m_numSpeakers{};
};

//==============================================================================
/** How the applier ramps from the previous buffer's gains to this one's. */
struct GainInterpolation
{
  /** SpatGRIS's `spatGainsInterpolation`, in [0, 1].
   *  0 means a linear ramp across the buffer; anything above uses a
   *  first-order filter whose coefficient is derived below. */
  float amount{};

  [[nodiscard]] float factor() const noexcept
  {
    return std::pow(amount, 0.1f) * 0.0099f + 0.99f;
  }
};

//==============================================================================
/** Applies one source's gain row to the speaker buffers, accumulating.
 *
 * @param target      the gains this buffer should end on
 * @param last        the gains the previous buffer ended on; updated in place
 * @param input       the source's samples
 * @param outputs     one write pointer per speaker; null entries are skipped
 * @param numSamples  frames in this buffer
 *
 * Realtime-safe: no allocation, no locking. `last` carries the smoothing state
 * and must live as long as the source does.
 */
inline void applySourceGains(
    float const* target, float* last, float const* input, float* const* outputs,
    std::size_t numSpeakers, int numSamples, GainInterpolation interp) noexcept
{
  auto const gainFactor = interp.factor();

  for(std::size_t spk = 0; spk < numSpeakers; ++spk)
  {
    auto* outputSamples = outputs[spk];
    if(outputSamples == nullptr)
      continue; // speaker muted, direct-out only, or not connected

    auto& currentGain = last[spk];
    auto const targetGain = target[spk];
    auto const gainDiff = targetGain - currentGain;
    auto const gainSlope = gainDiff / static_cast<float>(numSamples);

    if(gainSlope == 0.f || std::abs(gainDiff) < SMALL_GAIN)
    {
      // no interpolation needed
      currentGain = targetGain;
      if(currentGain >= SMALL_GAIN)
        for(int i = 0; i < numSamples; ++i)
          outputSamples[i] += input[i] * currentGain;
      continue;
    }

    if(interp.amount == 0.f)
    {
      // linear interpolation over the buffer
      for(int i = 0; i < numSamples; ++i)
      {
        currentGain += gainSlope;
        outputSamples[i] += input[i] * currentGain;
      }
    }
    else if(targetGain < SMALL_GAIN)
    {
      // log interpolation, targeting silence: stop once we are inaudible
      for(int i = 0; i < numSamples && currentGain >= SMALL_GAIN; ++i)
      {
        currentGain = targetGain + (currentGain - targetGain) * gainFactor;
        outputSamples[i] += input[i] * currentGain;
      }
    }
    else
    {
      // log interpolation with a 1st order filter
      for(int i = 0; i < numSamples; ++i)
      {
        currentGain = targetGain + (currentGain - targetGain) * gainFactor;
        outputSamples[i] += input[i] * currentGain;
      }
    }
  }
}

} // namespace Gris
