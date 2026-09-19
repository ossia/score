#include <Gris/Algo/Spatializer.hpp>

#include <algorithm>

namespace Gris
{
Spatializer::Spatializer() = default;
Spatializer::~Spatializer() = default;

void Spatializer::setSpeakerSetup(SpeakerSetup setup)
{
  m_setup = std::move(setup);
  rebuild();
}

void Spatializer::setSourceCount(std::size_t count)
{
  m_sources.assign(count, SourceState{});
  m_target.resize(count, m_flat.size());
  m_last.resize(count, m_flat.size());
}

void Spatializer::rebuild()
{
  m_flat = m_setup.flattened();
  m_numOutputChannels = m_setup.maxOutputPatch();

  // The gain matrix is indexed by position in the flattened setup; this maps
  // each of those rows to the audio channel it drives. Direct-out-only
  // speakers get no channel: the algorithms do not spatialize into them.
  m_channelOfSpeaker.assign(m_flat.size(), -1);
  for(std::size_t i = 0; i < m_flat.size(); ++i)
  {
    if(m_flat[i].data.isDirectOutOnly || m_flat[i].data.state == SliceState::muted)
      continue;
    m_channelOfSpeaker[i] = m_flat[i].patch.get() - 1;
  }

  // --- VBAP ---------------------------------------------------------------
  m_vbap.reset();
  {
    std::array<Position, MAX_NUM_SPEAKERS> positions{};
    std::array<output_patch_t, MAX_NUM_SPEAKERS> patches{};
    int count{};
    bool is3d{};

    for(auto const& speaker : m_flat)
    {
      if(speaker.data.isDirectOutOnly || count >= MAX_NUM_SPEAKERS)
        continue;
      positions[static_cast<std::size_t>(count)] = speaker.data.position;
      patches[static_cast<std::size_t>(count)] = speaker.patch;
      if(std::abs(speaker.data.position.getPolar().elevation.get()) > 0.001f)
        is3d = true;
      ++count;
    }

    // VBAP needs a triplet (3D) or a pair (2D); below three speakers there is
    // nothing to pan between.
    if(count >= 3)
      m_vbap = vbapInit(positions, count, is3d ? 3 : 2, patches);
  }

  // --- MBAP ---------------------------------------------------------------
  m_mbapUsable = false;
  m_mbap.reset();
  {
    SpeakersData spatialized;
    for(auto const& speaker : m_flat)
      if(!speaker.data.isDirectOutOnly)
        spatialized.push_back(speaker);

    if(spatialized.size() >= 2)
    {
      m_mbap = mbapInit(spatialized);

      // mbapInit() leaves the field exponent unset; SpatGRIS fills it in from
      // the setup's diffusion, inverted, in sg_MbapSpatAlgorithm.cpp. Without
      // it the exponent is zero, every gain becomes pow(g, 0) == 1 and the
      // source spreads evenly over every speaker.
      constexpr float DIFFUSION_IN_MIN{1.f};
      constexpr float DIFFUSION_IN_MAX{0.f};
      constexpr float DIFFUSION_OUT_MIN{1.f};
      constexpr float DIFFUSION_OUT_MAX{8.f};
      m_mbap.fieldExponent
          = ((m_setup.diffusion - DIFFUSION_IN_MIN) * (DIFFUSION_OUT_MAX - DIFFUSION_OUT_MIN)
             / (DIFFUSION_IN_MAX - DIFFUSION_IN_MIN))
            + DIFFUSION_OUT_MIN;

      m_mbapUsable = true;
    }
  }

  m_outputPtrs.assign(m_flat.size(), nullptr);
  m_target.resize(m_sources.size(), m_flat.size());
  m_last.resize(m_sources.size(), m_flat.size());
}

//==============================================================================
void Spatializer::setSourcePosition(std::size_t source, Position const& position) noexcept
{
  if(source >= m_sources.size())
    return;
  auto& s = m_sources[source];
  if(s.data.position && *s.data.position == position)
    return;
  s.data.position = position;
  s.dirty = true;
}

void Spatializer::setSourceSpans(
    std::size_t source, float azimuthSpan, float zenithSpan) noexcept
{
  if(source >= m_sources.size())
    return;
  auto& s = m_sources[source];
  if(s.data.azimuthSpan == azimuthSpan && s.data.zenithSpan == zenithSpan)
    return;
  s.data.azimuthSpan = azimuthSpan;
  s.data.zenithSpan = zenithSpan;
  s.dirty = true;
}

void Spatializer::setSourceMode(std::size_t source, SpatMode mode) noexcept
{
  if(source >= m_sources.size())
    return;
  auto& s = m_sources[source];
  // `hybrid` is a project-level mode; per source it resolves to one algorithm.
  auto const resolved = (mode == SpatMode::mbap) ? SpatMode::mbap : SpatMode::vbap;
  if(s.mode == resolved)
    return;
  s.mode = resolved;
  s.dirty = true;
}

void Spatializer::clearSourcePosition(std::size_t source) noexcept
{
  if(source >= m_sources.size())
    return;
  auto& s = m_sources[source];
  if(!s.data.position)
    return;
  s.data.position.reset();
  s.dirty = true;
}

//==============================================================================
void Spatializer::updateGains() noexcept
{
  if(m_flat.empty())
    return;

  for(std::size_t src = 0; src < m_sources.size(); ++src)
  {
    auto& state = m_sources[src];
    if(!state.dirty)
      continue;

    auto* row = m_target.row(src);
    std::fill_n(row, m_flat.size(), 0.f);

    if(state.data.position)
    {
      m_scratch.fill(0.f);

      if(state.mode == SpatMode::mbap && m_mbapUsable)
        mbap(state.data, m_scratch, m_mbap);
      else if(m_vbap)
        vbapCompute(state.data, m_scratch, *m_vbap);

      // SpeakersSpatGains is keyed by output patch; the matrix row is keyed by
      // position in the flattened setup.
      for(std::size_t i = 0; i < m_flat.size(); ++i)
        row[i] = m_scratch[m_flat[i].patch];
    }

    state.dirty = false;
  }
}

//==============================================================================
void Spatializer::process(
    float const* const* inputs, std::size_t numInputs, float* const* outputs,
    std::size_t numOutputs, int numSamples, GainInterpolation interp) noexcept
{
  if(m_flat.empty() || numSamples <= 0)
    return;

  updateGains();

  auto const numSources = std::min(numInputs, m_sources.size());
  for(std::size_t src = 0; src < numSources; ++src)
  {
    auto const* input = inputs[src];
    if(input == nullptr)
      continue;

    // Resolve this buffer's write pointers once per source: a speaker with no
    // channel (direct-out only, muted) is skipped by passing a null pointer.
    for(std::size_t i = 0; i < m_flat.size(); ++i)
    {
      auto const channel = m_channelOfSpeaker[i];
      m_outputPtrs[i]
          = (channel >= 0 && static_cast<std::size_t>(channel) < numOutputs)
                ? outputs[static_cast<std::size_t>(channel)]
                : nullptr;
    }

    applySourceGains(
        m_target.row(src), m_last.row(src), input, m_outputPtrs.data(), m_flat.size(),
        numSamples, interp);
  }
}

//==============================================================================
std::vector<Triplet> Spatializer::triplets() const
{
  if(!m_vbap || m_vbap->dimension != 3)
    return {};
  return vbapExtractTriplets(*m_vbap);
}

} // namespace Gris
