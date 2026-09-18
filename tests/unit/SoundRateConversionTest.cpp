// A sound whose file rate differs from the graph's must keep its own pitch and
// its own speed. Drives the real nodes over a tone and measures the result; a
// missing rate term transposes by graphRate/fileRate.

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/dataflow/nodes/sound_mmap.hpp>
#include <ossia/dataflow/nodes/sound_ref.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/thread.hpp>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace
{
constexpr double tone_hz = 440.0;
constexpr int buffer_size = 512;

ossia::audio_array make_tone(int rate, double seconds, int channels = 1)
{
  const int64_t n = int64_t(rate * seconds);
  ossia::audio_array arr;
  arr.resize(channels);
  for(auto& ch : arr)
  {
    ch.resize(n);
    for(int64_t i = 0; i < n; i++)
      ch[i] = 0.5 * std::sin(2. * std::numbers::pi * tone_hz * double(i) / rate);
  }
  return arr;
}

void setup_state(ossia::execution_state& e, int graph_rate)
{
  e.bufferSize = buffer_size;
  e.sampleRate = graph_rate;
  e.modelToSamplesRatio = graph_rate / ossia::flicks_per_second<double>;
  e.samplesToModelRatio = ossia::flicks_per_second<double> / graph_rate;
}

//! Runs the node for `ticks` buffers of straight forward transport and returns
//! channel 0.
template <typename Node>
std::vector<double> render(Node& snd, ossia::execution_state& e, int ticks)
{
  const int64_t per_buffer
      = int64_t(ossia::flicks_per_second<double> * buffer_size / e.sampleRate);

  std::vector<double> out;
  out.reserve(std::size_t(ticks) * buffer_size);
  for(int i = 0; i < ticks; i++)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{int64_t(i) * per_buffer};
    tk.date = ossia::time_value{int64_t(i + 1) * per_buffer};
    tk.offset = ossia::time_value{0};
    tk.speed = 1.;
    tk.tempo = 120.;
    tk.signature = {4, 4};

    snd.run(tk, ossia::exec_state_facade{&e});

    auto& ap = *snd.root_outputs()[0]->template target<ossia::audio_port>();
    if(ap.channels() == 0)
      continue;
    auto& ch = ap.channel(0);
    for(int k = 0; k < buffer_size; k++)
      out.push_back(k < int(ch.size()) ? ch[k] : 0.);
  }
  return out;
}

//! Rising zero crossings; exact enough on a pure tone to catch a few percent.
double measure_hz(const std::vector<double>& x, int rate)
{
  if(x.size() < 2)
    return 0.;
  int crossings = 0;
  for(std::size_t i = 1; i < x.size(); i++)
    if(x[i - 1] <= 0. && x[i] > 0.)
      crossings++;
  return crossings / (double(x.size()) / rate);
}
} // namespace

TEST_CASE("sound_ref plays at its own pitch on a faster graph", "[sound][rate]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  constexpr int file_rate = 44100;
  constexpr int graph_rate = 48000;

  auto handle = std::make_shared<ossia::audio_data>();
  handle->data = make_tone(file_rate, 6.);

  ossia::nodes::sound_ref snd;
  snd.set_sound(handle, 1, file_rate);
  snd.m_resampler.reset(
      0, ossia::audio_stretch_mode::None, 1, file_rate, graph_rate);
  snd.set_native_tempo(120.);

  ossia::execution_state e;
  setup_state(e, graph_rate);
  const auto out = render(snd, e, 300);

  REQUIRE(out.size() == 300u * buffer_size);
  // 1:1 would read 440 * 48000/44100 = 478.9 Hz.
  CHECK(measure_hz(out, graph_rate) == Catch::Approx(tone_hz).margin(3.0));
}

TEST_CASE("sound_ref plays at its own pitch on a slower graph", "[sound][rate]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  constexpr int file_rate = 48000;
  constexpr int graph_rate = 44100;

  auto handle = std::make_shared<ossia::audio_data>();
  handle->data = make_tone(file_rate, 6.);

  ossia::nodes::sound_ref snd;
  snd.set_sound(handle, 1, file_rate);
  snd.m_resampler.reset(
      0, ossia::audio_stretch_mode::None, 1, file_rate, graph_rate);
  snd.set_native_tempo(120.);

  ossia::execution_state e;
  setup_state(e, graph_rate);
  const auto out = render(snd, e, 300);

  // 1:1 would read 440 * 44100/48000 = 404.3 Hz.
  CHECK(measure_hz(out, graph_rate) == Catch::Approx(tone_hz).margin(3.0));
}

TEST_CASE("matched rates are left exactly alone", "[sound][rate]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  constexpr int rate = 44100;

  auto handle = std::make_shared<ossia::audio_data>();
  handle->data = make_tone(rate, 4.);

  ossia::nodes::sound_ref snd;
  snd.set_sound(handle, 1, rate);
  snd.m_resampler.reset(0, ossia::audio_stretch_mode::None, 1, rate, rate);
  snd.set_native_tempo(120.);

  // Equal rates keep the cheap path: the output is the file, sample for sample.
  REQUIRE(snd.m_resampler.stretch() == false);

  ossia::execution_state e;
  setup_state(e, rate);
  const auto out = render(snd, e, 8);

  const auto& src = handle->data[0];
  for(std::size_t i = 0; i < out.size() && i < src.size(); i++)
    REQUIRE(out[i] == Catch::Approx(src[i]).margin(1e-9));
}

TEST_CASE("a rate-converting sound is not reported as time-stretched", "[sound][rate]")
{
  // stretch() drives file_sample_for_model_time's tempo scaling: a mode-None
  // rate conversion must not flip it, or every seek lands wrong.
  ossia::resampler r;
  r.reset(0, ossia::audio_stretch_mode::None, 1, 44100, 48000);
  CHECK(r.stretch() == false);

  ossia::resampler r2;
  r2.reset(0, ossia::audio_stretch_mode::None, 1, 44100, 44100);
  CHECK(r2.stretch() == false);

#if defined(OSSIA_ENABLE_RUBBERBAND)
  ossia::resampler r3;
  r3.reset(0, ossia::audio_stretch_mode::RubberBandStandard, 1, 44100, 44100);
  CHECK(r3.stretch() == true);
#endif
}

TEST_CASE("rate_ratio is the graph rate over the material rate", "[sound][rate]")
{
  CHECK(ossia::snd::rate_ratio(44100, 44100) == 1.);
  CHECK(ossia::snd::rate_ratio(44100, 48000) == Catch::Approx(48000. / 44100.));
  CHECK(ossia::snd::rate_ratio(48000, 44100) == Catch::Approx(44100. / 48000.));

  // Unknown rates must not scale anything.
  CHECK(ossia::snd::rate_ratio(0, 48000) == 1.);
  CHECK(ossia::snd::rate_ratio(44100, 0) == 1.);
  CHECK(ossia::snd::rate_ratio(-1, 48000) == 1.);
}

TEST_CASE("a rate-converted sound advances in real time", "[sound][rate]")
{
  // Pitch can be right while the file pointer drifts; a ramp checks the speed.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  constexpr int file_rate = 44100;
  constexpr int graph_rate = 48000;
  constexpr double seconds = 8.;
  const int64_t frames = int64_t(file_rate * seconds);

  auto handle = std::make_shared<ossia::audio_data>();
  handle->data.resize(1);
  handle->data[0].resize(frames);
  for(int64_t i = 0; i < frames; i++)
    handle->data[0][i] = double(i) / frames;

  ossia::nodes::sound_ref snd;
  snd.set_sound(handle, 1, file_rate);
  snd.m_resampler.reset(
      0, ossia::audio_stretch_mode::None, 1, file_rate, graph_rate);
  snd.set_native_tempo(120.);

  ossia::execution_state e;
  setup_state(e, graph_rate);
  constexpr int ticks = 300;
  const auto out = render(snd, e, ticks);

  const double transport_s = double(out.size()) / graph_rate;
  const double file_s = out.back() * seconds;

  // Margin is the resampler's latency; a missing rate term is 8.8% out.
  CHECK(file_s == Catch::Approx(transport_s).margin(0.05));
}

#if defined(OSSIA_ENABLE_RUBBERBAND)
TEST_CASE("rubberband keeps pitch across a rate change", "[sound][rate][rubberband]")
{
  // Stretching mode: the conversion rides on the time ratio, and the pitch
  // scale takes the resulting transposition back out.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  constexpr int file_rate = 44100;
  constexpr int graph_rate = 48000;

  auto handle = std::make_shared<ossia::audio_data>();
  handle->data = make_tone(file_rate, 8.);

  ossia::nodes::sound_ref snd;
  snd.set_sound(handle, 1, file_rate);
  snd.m_resampler.reset(
      0, ossia::audio_stretch_mode::RubberBandStandard, 1, file_rate, graph_rate);
  // file tempo == timeline tempo, so the only ratio left is the rate one.
  snd.set_native_tempo(120.);

  ossia::execution_state e;
  setup_state(e, graph_rate);
  auto out = render(snd, e, 300);

  // Skip the stretcher's warm-up before measuring.
  out.erase(out.begin(), out.begin() + 4 * buffer_size);
  CHECK(measure_hz(out, graph_rate) == Catch::Approx(tone_hz).margin(6.0));
}
#endif
