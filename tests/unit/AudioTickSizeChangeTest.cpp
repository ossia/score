// The engine's ticks do not always have the block size the nodes were set up
// for: PipeWire's quantum is global to the graph, so another client (a second
// score, a DAW) can impose a smaller one, and JACK can change it at runtime.
// The nodes must then neither read nor write past their buffers, and the
// audio they hand downstream must span exactly the tick.

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_utils.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/mono_audio_node.hpp>
#include <avnd/binding/ossia/node.hpp>
#include <avnd/binding/ossia/ossia_audio_node.hpp>
#include <avnd/binding/ossia/ossia_dynamic_audio_node.hpp>
#include <avnd/binding/ossia/poly_audio_node.hpp>

#include <Advanced/Audio/AudioSum.hpp>
#include <Advanced/Audio/Gain.hpp>
#include <Helpers/PeakBandFFTPort.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

namespace
{
constexpr int rate = 48000;

//! An avendish node driven like the graph drives it, one tick at a time.
template <typename T>
struct rig
{
  ossia::execution_state state;
  oscr::safe_node<T> node;
  int64_t date{};

  explicit rig(int prepared_frames)
      : node{prepared_frames, double(rate), 1}
  {
    state.sampleRate = rate;
    state.bufferSize = prepared_frames;
    node.finish_init();
    node.audio_configuration_changed({&state});
    node.prepare(state);
  }

  ossia::audio_port& audio_in(int k = 0)
  {
    int i = 0;
    for(auto* in : node.root_inputs())
      if(auto* p = in->template target<ossia::audio_port>())
        if(i++ == k)
          return *p;
    FAIL("no audio inlet " << k);
    throw;
  }

  ossia::audio_port& audio_out()
  {
    for(auto* out : node.root_outputs())
      if(auto* p = out->template target<ossia::audio_port>())
        return *p;
    FAIL("no audio outlet");
    throw;
  }

  //! Channels of exactly `size` frames, sample i of channel c = f(c, i).
  void feed(ossia::audio_port& p, int channels, int size, auto f)
  {
    p.set_channels(channels);
    for(int c = 0; c < channels; c++)
    {
      p.channel(c).assign(size, 0.);
      for(int i = 0; i < size; i++)
        p.channel(c)[i] = f(c, i);
    }
  }

  //! One tick of `frames`, through the graph's own init / run / teardown.
  void tick(int frames)
  {
    state.bufferSize = frames;
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{date};
    tk.date = ossia::time_value{date + frames};
    tk.start_sample = 0;
    tk.length_sample = frames;
    date += frames;
    node.requested_tokens.clear();
    node.request(tk);
    ossia::graph_util::exec_node(node, state);
  }
};

double ramp(int c, int i)
{
  return (c + 1) * 0.001 * (i + 1);
}
}

TEST_CASE(
    "avnd audio node: ticks smaller and larger than the prepared block size",
    "[avnd][audio][buffer_size]")
{
  // Prepared for 1024, then the graph's quantum drops to 512 (the reported
  // case), outgrows the preparation (2048), and comes back.
  rig<ao::AudioSum> r{1024};
  for(int frames : {1024, 512, 2048, 512, 64, 1024})
  {
    INFO("tick of " << frames << " frames");
    // What upstream hands over can be sized for another block size too.
    for(int input_size : {frames, frames * 2, frames / 2})
    {
      INFO("input of " << input_size << " frames");
      r.feed(r.audio_in(), 2, input_size, ramp);
      r.tick(frames);

      auto& out = r.audio_out();
      REQUIRE(out.channels() == 1);
      // Exactly the tick: not the 1024 the node was prepared with.
      REQUIRE(out.channel(0).size() == std::size_t(frames));
      for(int i = 0; i < frames; i++)
      {
        // Past the end of a short input is silence.
        const double expected = i < input_size ? ramp(0, i) + ramp(1, i) : 0.;
        REQUIRE(out.channel(0)[i] == expected);
      }
    }
  }
}

TEST_CASE(
    "avnd per-sample node (Gain): outputs follow the tick size",
    "[avnd][audio][buffer_size]")
{
  rig<ao::Gain> r{1024};
  for(int frames : {1024, 512, 2048, 512, 1024})
  {
    INFO("tick of " << frames << " frames");
    r.feed(r.audio_in(), 2, frames * 2, ramp);
    r.tick(frames);
    auto& out = r.audio_out();
    REQUIRE(out.channels() == 2);
    for(auto& chan : out)
    {
      REQUIRE(chan.size() == std::size_t(frames));
      for(double v : chan)
        REQUIRE(std::isfinite(v));
    }
  }
}

TEST_CASE(
    "avnd spectrum input: the FFT follows the size of the samples it gets",
    "[avnd][audio][buffer_size][fft]")
{
  using fx = examples::helpers::PeakBandFFTPort;
  // A cosine sitting exactly on bin 8 of an N-point transform.
  auto cosine = [](int n) {
    return [n](int, int i) {
      return std::cos(2. * std::numbers::pi * 8. * i / n);
    };
  };

  rig<fx> r{1024};

  SECTION("smaller ticks than prepared")
  {
    for(int frames : {256, 1024, 128})
    {
      INFO("tick of " << frames << " frames");
      r.feed(r.audio_in(0), 1, frames, cosine(frames));
      r.feed(r.audio_in(1), 1, frames, cosine(frames));
      r.tick(frames);
      // A transform of the prepared 1024 points would see the peak at
      // 8 * 1024 / frames, over the zeros (or stale frames) past the tick.
      CHECK(r.node.impl.effect.outputs.band.value == 8);
      CHECK(r.node.impl.effect.outputs.band_2.value == 8);
    }
  }

  SECTION("an input larger than the transform was planned for")
  {
    // Bypassing the graph, which would fit the input to the tick: a node
    // must not trust its inputs' size either. The transform covers the tick,
    // not the 4096 frames handed over, nor the 1024 it was planned for.
    const int frames = 512;
    r.feed(r.audio_in(0), 1, 8 * frames, cosine(frames));
    r.feed(r.audio_in(1), 1, 8 * frames, cosine(frames));
    r.state.bufferSize = frames;
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{frames};
    tk.start_sample = 0;
    tk.length_sample = frames;
    r.node.run(tk, {&r.state});
    CHECK(r.node.impl.effect.outputs.band.value == 8);
  }
}

namespace
{
//! Writes `size` samples per channel whatever the tick: a node set up for
//! another block size.
struct fixed_size_source final : ossia::nonowning_graph_node
{
  ossia::audio_outlet out;
  std::size_t size{};
  explicit fixed_size_source(std::size_t sz)
      : size{sz}
  {
    m_outlets.push_back(&out);
  }
  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    out.data.set_channels(2);
    for(auto& c : out.data)
      c.assign(size, 1.);
  }
  std::string label() const noexcept override { return "fixed_size_source"; }
};

struct recorder final : ossia::nonowning_graph_node
{
  ossia::audio_inlet in;
  std::vector<std::size_t> sizes;
  explicit recorder() { m_inlets.push_back(&in); }
  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    sizes.clear();
    for(auto& c : in.data)
      sizes.push_back(c.size());
  }
  std::string label() const noexcept override { return "recorder"; }
};
}

TEST_CASE(
    "graph: what reaches an audio inlet spans exactly the tick",
    "[graph][audio][buffer_size]")
{
  ossia::execution_state state;
  state.sampleRate = rate;

  for(std::size_t produced : {1024u, 256u, 0u})
  {
    INFO("upstream writes " << produced << " frames");
    auto src = std::make_shared<fixed_size_source>(produced);
    auto dst = std::make_shared<recorder>();
    ossia::graph_edge edge{
        ossia::immediate_glutton_connection{}, &src->out, &dst->in, src, dst};
    src->out.connect(&edge);
    dst->in.connect(&edge);

    state.bufferSize = 512;
    ossia::token_request tk;
    tk.start_sample = 0;
    tk.length_sample = 512;
    src->request(tk);
    dst->request(tk);
    ossia::graph_util::exec_node(*src, state);
    ossia::graph_util::exec_node(*dst, state);

    REQUIRE(dst->sizes.size() == 2);
    for(auto sz : dst->sizes)
      CHECK(sz == 512u);
  }
}
