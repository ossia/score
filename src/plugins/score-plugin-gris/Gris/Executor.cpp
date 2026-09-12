#include <Gris/Algo/SpeakerSetupIO.hpp>
#include <Gris/Algo/Spatializer.hpp>
#include <Gris/Executor.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QByteArray>

#include <vector>

namespace Gris
{
namespace
{
/** The most recent value a control port received this tick, or null when it
 *  received none. */
[[nodiscard]] ossia::value const* lastValue(ossia::value_inlet const& inlet) noexcept
{
  auto const& data = inlet.data.get_data();
  if(data.empty())
    return nullptr;
  return &data.back().value;
}
} // namespace

/** The execution node.
 *
 * Inlet order mirrors SpatModel's: audio, speaker setup, interpolation, source
 * count, then four value inlets per source (position, azimuth span, zenith
 * span, mode). One audio outlet, as wide as the setup's highest output patch.
 */
class SpatNode final : public ossia::nonowning_graph_node
{
public:
  explicit SpatNode(int sourceCount)
      : m_sourceCount{sourceCount}
  {
    m_inlets.push_back(&audio_in);
    m_inlets.push_back(&setup_in);
    m_inlets.push_back(&interp_in);
    m_inlets.push_back(&count_in);

    m_sourcePorts.resize(std::size_t(sourceCount) * SpatModel::SourceInletCount);
    for(auto& port : m_sourcePorts)
      m_inlets.push_back(&port);

    m_outlets.push_back(&audio_out);

    m_spat.setSourceCount(std::size_t(sourceCount));
  }

  [[nodiscard]] std::string label() const noexcept override { return "gris-spat"; }

  void run(const ossia::token_request& t, ossia::exec_state_facade f) noexcept override
  {
    readControls();

    auto const frames = int(f.bufferSize());
    if(frames <= 0)
      return;

    auto const channels = std::size_t(std::max(0, m_spat.numOutputChannels()));
    ossia::audio_port& out = *audio_out;
    out.set_channels(channels);
    for(std::size_t c = 0; c < channels; ++c)
    {
      auto& chan = out.channel(c);
      chan.assign(std::size_t(frames), 0.);
    }
    if(channels == 0)
      return;

    // ossia carries doubles, the GRIS algorithms are float: convert in, mix,
    // convert back. The scratch buffers are sized outside the audio path.
    ossia::audio_port const& in = *audio_in;
    auto const numSources = std::min(in.channels(), m_spat.sourceCount());
    ensureScratch(numSources, channels, frames);

    for(std::size_t s = 0; s < numSources; ++s)
    {
      auto const& src = in.channel(s);
      auto* dst = m_inScratch[s].data();
      auto const n = std::min(std::size_t(frames), src.size());
      for(std::size_t i = 0; i < n; ++i)
        dst[i] = float(src[i]);
      for(std::size_t i = n; i < std::size_t(frames); ++i)
        dst[i] = 0.f;
      m_inPtrs[s] = dst;
    }

    for(std::size_t c = 0; c < channels; ++c)
    {
      std::fill_n(m_outScratch[c].data(), frames, 0.f);
      m_outPtrs[c] = m_outScratch[c].data();
    }

    m_spat.process(
        m_inPtrs.data(), numSources, m_outPtrs.data(), channels, frames,
        GainInterpolation{m_interpolation});

    for(std::size_t c = 0; c < channels; ++c)
    {
      auto& chan = out.channel(c);
      auto const* src = m_outScratch[c].data();
      for(int i = 0; i < frames; ++i)
        chan[std::size_t(i)] = double(src[i]);
    }
  }

private:
  void ensureScratch(std::size_t numSources, std::size_t channels, int frames)
  {
    // Only grows, and only when the graph hands us something bigger than
    // before; steady state allocates nothing.
    if(m_inScratch.size() < numSources)
      m_inScratch.resize(numSources);
    if(m_inPtrs.size() < numSources)
      m_inPtrs.resize(numSources, nullptr);
    for(auto& buf : m_inScratch)
      if(buf.size() < std::size_t(frames))
        buf.resize(std::size_t(frames));

    if(m_outScratch.size() < channels)
      m_outScratch.resize(channels);
    if(m_outPtrs.size() < channels)
      m_outPtrs.resize(channels, nullptr);
    for(auto& buf : m_outScratch)
      if(buf.size() < std::size_t(frames))
        buf.resize(std::size_t(frames));
  }

  void readControls()
  {
    // Speaker setup: only reparsed when the serialised form actually changed,
    // since rebuilding both algorithms allocates.
    if(auto const* v = lastValue(setup_in))
    {
      auto str = ossia::convert<std::string>(*v);
      if(str != m_setupXml)
      {
        m_setupXml = std::move(str);
        if(auto res = readSpeakerSetup(QByteArray::fromStdString(m_setupXml)))
          m_spat.setSpeakerSetup(std::move(*res.setup));
      }
    }

    if(auto const* v = lastValue(interp_in))
      m_interpolation = std::clamp(ossia::convert<float>(*v), 0.f, 1.f);

    for(int source = 0; source < m_sourceCount; ++source)
    {
      auto const base = std::size_t(source) * SpatModel::SourceInletCount;

      if(auto const* v = lastValue(m_sourcePorts[base + SpatModel::Position]))
      {
        auto const vec = ossia::convert<ossia::vec3f>(*v);
        m_spat.setSourcePosition(
            std::size_t(source), Position{CartesianVector{vec[0], vec[1], vec[2]}});
      }

      float azimuthSpan{m_spans[std::size_t(source)].first};
      float zenithSpan{m_spans[std::size_t(source)].second};
      bool spansChanged{};
      if(auto const* v = lastValue(m_sourcePorts[base + SpatModel::AzimuthSpan]))
      {
        azimuthSpan = ossia::convert<float>(*v);
        spansChanged = true;
      }
      if(auto const* v = lastValue(m_sourcePorts[base + SpatModel::ZenithSpan]))
      {
        zenithSpan = ossia::convert<float>(*v);
        spansChanged = true;
      }
      if(spansChanged)
      {
        m_spans[std::size_t(source)] = {azimuthSpan, zenithSpan};
        m_spat.setSourceSpans(std::size_t(source), azimuthSpan, zenithSpan);
      }

      if(auto const* v = lastValue(m_sourcePorts[base + SpatModel::Mode]))
      {
        auto const mode = ossia::convert<std::string>(*v);
        m_spat.setSourceMode(
            std::size_t(source),
            mode.find("Cube") != std::string::npos ? SpatMode::mbap : SpatMode::vbap);
      }
    }
  }

public:
  ossia::audio_inlet audio_in;
  ossia::value_inlet setup_in;
  ossia::value_inlet interp_in;
  ossia::value_inlet count_in;
  ossia::audio_outlet audio_out;

private:
  int m_sourceCount{};
  std::vector<ossia::value_inlet> m_sourcePorts;
  std::vector<std::pair<float, float>> m_spans{
      std::size_t(SpatModel::maxSourceCount), {0.f, 0.f}};

  Spatializer m_spat;
  std::string m_setupXml;
  float m_interpolation{};

  std::vector<std::vector<float>> m_inScratch;
  std::vector<std::vector<float>> m_outScratch;
  std::vector<float const*> m_inPtrs;
  std::vector<float*> m_outPtrs;
};

//==============================================================================
Executor::Executor(SpatModel& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "GrisSpatComponent", parent}
{
  auto node = ossia::make_node<SpatNode>(*ctx.execState, proc.sourceCount());
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(this->node);
}

Executor::~Executor() = default;
} // namespace Gris
