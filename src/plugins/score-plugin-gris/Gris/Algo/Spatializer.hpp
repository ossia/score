#pragma once

/* VBAP + MBAP in one engine, switchable per source at realtime.
 *
 * This is the shape SpatGRIS's own hybrid mode uses (sg_HybridSpatAlgorithm):
 * both algorithms stay resident and each source is dispatched to one of them.
 * Memory is the cost -- a VbapData (triplets and inverse matrices) and an
 * MbapField (a 64-cell amplitude matrix per speaker) -- and there is no
 * per-buffer cost to having both.
 */

#include <Gris/Algo/GainMatrix.hpp>
#include <Gris/Algo/Mbap.hpp>
#include <Gris/Algo/SpeakerSetup.hpp>
#include <Gris/Algo/Types.hpp>
#include <Gris/Algo/Vbap.hpp>

#include <memory>
#include <vector>

namespace Gris
{
//==============================================================================
/** One source's spatialisation state. */
struct SourceState
{
  SourceData data{};
  /** vbap or mbap. `hybrid` is meaningless per source and is treated as vbap. */
  SpatMode mode{SpatMode::vbap};
  /** Set when position/spans/mode changed and the gains need recomputing. */
  bool dirty{true};
};

//==============================================================================
class Spatializer
{
public:
  Spatializer();
  ~Spatializer();

  /** Rebuilds both algorithms for a new layout. Allocates: never call from the
   *  audio thread. */
  void setSpeakerSetup(SpeakerSetup setup);

  [[nodiscard]] SpeakerSetup const& speakerSetup() const noexcept { return m_setup; }

  /** Number of audio channels the process must produce, i.e. the highest
   *  output patch in the setup. */
  [[nodiscard]] int numOutputChannels() const noexcept { return m_numOutputChannels; }

  /** Allocates: never call from the audio thread. */
  void setSourceCount(std::size_t count);
  [[nodiscard]] std::size_t sourceCount() const noexcept { return m_sources.size(); }

  /** Marks one source for recomputation. Cheap, realtime-safe. */
  void setSourcePosition(std::size_t source, Position const& position) noexcept;
  void setSourceSpans(std::size_t source, float azimuthSpan, float zenithSpan) noexcept;
  void setSourceMode(std::size_t source, SpatMode mode) noexcept;
  /** Drops a source out of the mix until it gets a position again. */
  void clearSourcePosition(std::size_t source) noexcept;

  /** Recomputes the gain rows of every dirty source. Realtime-safe as long as
   *  the setup and source count have not changed. */
  void updateGains() noexcept;

  /** Accumulates every source into the speaker buffers, interpolating from the
   *  previous buffer's gains. `outputs` holds one pointer per output channel;
   *  a null entry is skipped. Realtime-safe. */
  void process(
      float const* const* inputs, std::size_t numInputs, float* const* outputs,
      std::size_t numOutputs, int numSamples, GainInterpolation interp) noexcept;

  /** VBAP triangulation, for display. Empty unless the setup is a 3D dome. */
  [[nodiscard]] std::vector<Triplet> triplets() const;

  /** false when the setup cannot drive the algorithm at all (fewer than three
   *  spatialized speakers for VBAP, fewer than two for MBAP). */
  [[nodiscard]] bool vbapUsable() const noexcept { return m_vbap != nullptr; }
  [[nodiscard]] bool mbapUsable() const noexcept { return m_mbapUsable; }

private:
  void rebuild();

  SpeakerSetup m_setup{};
  SpeakersData m_flat{};
  /** Maps a row of the gain matrix to the output channel it feeds. */
  std::vector<int> m_channelOfSpeaker{};
  int m_numOutputChannels{};

  std::unique_ptr<VbapData> m_vbap{};
  MbapField m_mbap{};
  bool m_mbapUsable{};

  std::vector<SourceState> m_sources{};
  GainMatrix m_target{};
  GainMatrix m_last{};
  /** Scratch for one source's gains, indexed by output patch. */
  SpeakersSpatGains m_scratch{};
  /** Scratch for the per-speaker output pointers of one source. */
  std::vector<float*> m_outputPtrs{};
};

} // namespace Gris
