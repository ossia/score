#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace Media::Metro
{
//! The sounds the Sound control picks, in the order of its choices. The
//! numbers are saved in documents.
enum sound_kind : int
{
  //! The two ticks of the user library
  Classic = 0,
  Click = 1,
  Woodblock = 2,
  Beep = 3,
  Cowbell = 4,
  //! The files of the Accent file and Beat file controls
  Files = 5,
  sound_count
};

//! What a beat is, in the order of the choices of the Beat control. The
//! numbers are saved in documents.
enum beat_unit : int
{
  //! The unit of the time signature: an eighth note in 7/8
  SignatureBeat = 0,
  //! A quarter note, whatever the signature
  QuarterBeat = 1
};

//! One click, at the rate of the engine. A mono sound has no right channel.
struct click_sound
{
  std::vector<float> left;
  std::vector<float> right;

  int64_t frames() const noexcept { return int64_t(left.size()); }
  bool empty() const noexcept { return left.empty(); }
};

//! What plays on the first beat of a bar, and on the others.
struct sound_set
{
  click_sound accent;
  click_sound beat;
};

using sound_set_ptr = std::shared_ptr<const sound_set>;

//! Ends the sound on a zero, so that it does not click when it stops.
inline void fade_tail(click_sound& s, int rate) noexcept
{
  const int64_t n = std::min<int64_t>(s.frames(), std::max(1, rate / 500));
  for(auto* ch : {&s.left, &s.right})
  {
    if(ch->empty())
      continue;
    const int64_t end = ch->size();
    for(int64_t i = 0; i < n; i++)
      (*ch)[end - 1 - i] *= float(i) / float(n);
  }
}

//! The synthesized sounds. They are cheap enough to be computed when the
//! execution starts: nothing has to ship with the software, and they come out
//! at the rate of the engine.
inline click_sound make_builtin_sound(sound_kind kind, bool accent, int rate)
{
  constexpr double two_pi = 2. * std::numbers::pi;
  click_sound s;
  if(rate <= 0)
    return s;

  //! A sum of partials {frequency, level}, under one exponential decay.
  struct partial
  {
    double freq;
    double level;
  };
  const auto render = [&](double seconds, double decay, double attack, double level,
                          std::initializer_list<partial> partials) {
    const int64_t n = std::max<int64_t>(1, std::llround(seconds * rate));
    s.left.resize(n);
    for(int64_t i = 0; i < n; i++)
    {
      const double t = double(i) / rate;
      double v = 0.;
      for(const auto& p : partials)
        // A partial above the Nyquist frequency would fold back.
        if(p.freq < 0.45 * rate)
          v += p.level * std::sin(two_pi * p.freq * t);
      const double env
          = std::exp(-t / decay) * (attack > 0. ? std::min(1., t / attack) : 1.);
      s.left[i] = float(level * env * v);
    }
  };

  switch(kind)
  {
    case Woodblock:
      if(accent)
        render(0.12, 0.016, 0.0003, 0.6, {{1180., 1.}, {3210., 0.35}, {5290., 0.12}});
      else
        render(0.12, 0.016, 0.0003, 0.5, {{820., 1.}, {2230., 0.35}, {3680., 0.12}});
      break;
    case Beep:
      if(accent)
        render(0.07, 0.03, 0.002, 0.7, {{1760., 1.}});
      else
        render(0.07, 0.03, 0.002, 0.6, {{880., 1.}});
      break;
    case Cowbell:
      // The two detuned tones of the analog drum machines' cowbell, with the
      // first harmonics a square wave would give them.
      if(accent)
        render(
            0.3, 0.07, 0.0005, 0.3,
            {{800., 1.}, {1200., 1.}, {2400., 0.33}, {3600., 0.33}, {4000., 0.2}});
      else
        render(
            0.25, 0.05, 0.0005, 0.26,
            {{540., 1.}, {800., 1.}, {1620., 0.33}, {2400., 0.33}, {2700., 0.2}});
      break;
    case Click:
    default:
      if(accent)
        render(0.03, 0.003, 0., 0.9, {{4000., 1.}});
      else
        render(0.03, 0.003, 0., 0.8, {{2500., 1.}});
      break;
  }
  fade_tail(s, rate);
  return s;
}

inline sound_set_ptr make_builtin_sounds(sound_kind kind, int rate)
{
  auto set = std::make_shared<sound_set>();
  set->accent = make_builtin_sound(kind, true, rate);
  set->beat = make_builtin_sound(kind, false, rate);
  return set;
}

//! What a point of the grid is.
enum class click_kind : uint8_t
{
  Accent,     //!< The first beat of a bar
  Beat,       //!< Another beat of the bar
  Subdivision //!< Between two beats
};

/**
 * @brief Calls f(musical position, kind) for every click a tick covers.
 *
 * A beat is the unit of the time signature (an eighth note in 7/8, a
 * sixteenth in 19/16) or a quarter note. The grid starts again on every bar
 * line, which the signature and the bars the token reports define, so a bar
 * of any length and a change of signature both put the accent where the bar
 * starts.
 *
 * Quarter notes are counted from the bar line, and a bar that is not a whole
 * number of them ends on a shorter beat: 7/8 is an accent, three beats a
 * quarter note apart, and the next accent an eighth note after the place a
 * fourth beat would have had; 19/16 is an accent, four beats, and the next
 * accent three sixteenths after the last. This is the grid of
 * ossia::token_request::metronome().
 *
 * Nothing is kept between two ticks: the clicks only depend on the musical
 * span of the tick. A change of tempo, a loop or a jump of the transport
 * cannot shift the grid, play a click twice or skip one. A position on the
 * boundary of two ticks belongs to the one that starts on it.
 */
template <typename F>
void for_each_click(
    const ossia::token_request& tk, int unit, int subdivision, F&& f) noexcept
{
  const double ms = tk.musical_start_position;
  const double me = tk.musical_end_position;
  const bool rewinding = tk.backward();
  if(tk.paused() || ms == me || (me < ms) != rewinding)
    return;

  const auto& sig = tk.signature;
  const double beat
      = (unit == SignatureBeat && sig.upper > 0 && sig.lower > 0) ? 4. / sig.lower : 1.;
  subdivision = std::clamp(subdivision, 1, 16);
  const double step = beat / subdivision;

  constexpr double eps = 1e-9;
  const double lo = rewinding ? me : ms;
  const double hi = rewinding ? ms : me;

  tk.for_each_bar_segment(lo, hi, [&](double bar_line, double next_bar) {
    // The first point of this bar the tick can hold.
    int64_t k = std::max<int64_t>(0, int64_t(std::floor((lo - bar_line) / step)) - 1);
    // A tick never holds more clicks than this, whatever it is handed.
    for(int guard = 0; guard < 4096; guard++, k++)
    {
      const double p = bar_line + k * step;
      if(p >= next_bar - eps || p > hi)
        break;
      if(rewinding ? (p <= lo) : (p < lo || p >= hi))
        continue;
      f(p,
        k == 0 ? click_kind::Accent
               : (k % subdivision == 0 ? click_kind::Beat : click_kind::Subdivision));
    }
  });
}

/**
 * @brief Plays a click on every beat of the bar.
 *
 * Each click starts on the sample its beat falls on, inside the buffer, and
 * plays at its own speed whatever the tempo. A click still sounding when the
 * next one starts is faded out, so that fast tempos do not pile them up.
 *
 * The sounds are handed over by the UI thread, already decoded and at the
 * rate of the engine: the audio thread only reads them.
 */
class metronome_node final : public ossia::nonowning_graph_node
{
public:
  ossia::value_inlet sound_in;
  //! Paths: a new one is handed to on_file, which has it decoded outside of
  //! the audio thread; the sounds then come back through swap_sounds().
  ossia::value_inlet accent_file_in;
  ossia::value_inlet beat_file_in;
  ossia::value_inlet volume_in;
  ossia::value_inlet accent_in;
  ossia::value_inlet subdivision_in;
  ossia::value_inlet beat_in;
  ossia::audio_outlet audio_out;
  ossia::value_outlet bang_out;

  //! Level of the clicks between two beats, relative to the beats.
  static constexpr float subdivision_level = 0.5f;

  /**
   * @brief Called on the audio thread when a file inlet receives a new path.
   *
   * 0 is the accent file, 1 the beat file. Only called when the path differs
   * from the last one of that inlet, so a source repeating its value on every
   * tick asks for nothing.
   */
  std::function<void(int, const std::string&)> on_file;

  int sound{Classic};
  float volume{1.f};
  bool accent{true};
  int subdivision{1};
  int beat{SignatureBeat};

  explicit metronome_node(int rate)
      : m_rate{rate > 0 ? rate : 48000}
      , m_fade{std::max(1, m_rate / 200)}
  {
    m_inlets.push_back(&sound_in);
    m_inlets.push_back(&accent_file_in);
    m_inlets.push_back(&beat_file_in);
    m_inlets.push_back(&volume_in);
    m_inlets.push_back(&accent_in);
    m_inlets.push_back(&subdivision_in);
    m_inlets.push_back(&beat_in);
    m_outlets.push_back(&audio_out);
    m_outlets.push_back(&bang_out);

    for(int k : {Click, Woodblock, Beep, Cowbell})
      m_sets[k] = make_builtin_sounds(sound_kind(k), m_rate);
    for(auto& path : m_paths)
      path.reserve(1024);
  }

  std::string label() const noexcept override { return "Metronome"; }

  //! Replaces the sounds of a kind, and leaves the previous ones in `set` for
  //! the caller to free outside of the audio thread. The clicks that were
  //! playing them stop: their samples are about to go away.
  void swap_sounds(int kind, sound_set_ptr& set) noexcept
  {
    if(kind < 0 || kind >= sound_count)
      return;
    if(const auto* old = m_sets[kind].get())
      for(auto& v : m_voices)
        if(v.snd == &old->accent || v.snd == &old->beat)
          v.snd = nullptr;
    std::swap(m_sets[kind], set);
  }

  const sound_set* sounds(int kind) const noexcept
  {
    if(kind >= 0 && kind < sound_count && m_sets[kind])
      return m_sets[kind].get();
    return nullptr;
  }

  //! The path a file inlet is on, e.g. the one of the model: the same path
  //! coming in again is then not a new one.
  void set_file_path(int file, const std::string& path)
  {
    if(file >= 0 && file < 2)
      m_paths[file].assign(path);
  }

  //! Nothing is left sounding, e.g. when the process starts again.
  void reset() noexcept
  {
    for(auto& v : m_voices)
      v.snd = nullptr;
    m_last_volume = -1.f;
  }

  void read_controls() noexcept
  {
    if(auto& d = sound_in->get_data(); !d.empty())
      sound = ossia::convert<int>(d.back().value);
    read_file(0, *accent_file_in);
    read_file(1, *beat_file_in);
    if(auto& d = volume_in->get_data(); !d.empty())
      volume = ossia::convert<float>(d.back().value);
    if(auto& d = accent_in->get_data(); !d.empty())
      accent = ossia::convert<bool>(d.back().value);
    if(auto& d = subdivision_in->get_data(); !d.empty())
      subdivision = ossia::convert<int>(d.back().value);
    if(auto& d = beat_in->get_data(); !d.empty())
      beat = ossia::convert<int>(d.back().value);
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // Before any early return: the inlets are cleared once the node has run.
    read_controls();

    const auto [tick_start, length] = st.timings(tk);
    if(length <= 0)
      return;

    // The clicks of this tick, in the order of their samples.
    struct event
    {
      int64_t offset;
      click_kind kind;
    };
    std::array<event, max_events> events;
    int n_events = 0;
    const double ratio = st.modelToSamples();
    for_each_click(tk, beat, subdivision, [&](double position, click_kind kind) {
      if(n_events >= max_events)
        return;
      const int64_t offset
          = std::clamp<int64_t>(tk.physical_position(position, ratio), 0, length - 1);
      // Rewinding, the bars are still walked in musical order, which is the
      // reverse of the samples'.
      int at = n_events++;
      while(at > 0 && events[at - 1].offset > offset)
      {
        events[at] = events[at - 1];
        at--;
      }
      events[at] = {offset, kind};
    });

    auto& port = *audio_out.target<ossia::audio_port>();
    port.set_channels(2);
    auto& ap = port.get();
    const std::size_t needed = std::size_t(tick_start + length);
    for(auto& channel : ap)
      if(channel.size() < needed)
        channel.resize(std::max<std::size_t>(needed, st.bufferSize()));
    double* const out[2] = {ap[0].data() + tick_start, ap[1].data() + tick_start};

    // The volume moves over the whole tick rather than stepping at its start.
    const float target = std::isfinite(volume) ? std::clamp(volume, 0.f, 4.f) : 0.f;
    const float from = m_last_volume < 0.f ? target : m_last_volume;
    const float volume_step = (target - from) / float(length);
    m_last_volume = target;

    const sound_set* set = current_sounds();
    auto& bangs = *bang_out.target<ossia::value_port>();

    int64_t cursor = 0;
    for(int i = 0; i < n_events; i++)
    {
      const auto& ev = events[i];
      render(out, cursor, ev.offset, from, volume_step);
      cursor = ev.offset;

      bangs.write_value(ossia::impulse{}, tick_start + ev.offset);
      if(set)
        start_voice(*set, ev.kind);
    }
    render(out, cursor, length, from, volume_step);
  }

private:
  static constexpr int max_voices = 16;
  static constexpr int max_events = 64;

  struct voice
  {
    const click_sound* snd{};
    int64_t pos{};
    float gain{1.f};
    //! Samples of fade-out left, -1 when the click plays to its end.
    int32_t fade_left{-1};
    int32_t fade_total{1};
  };

  //! Only the last value of a tick counts: the ones before it were replaced
  //! before anything could be done with them.
  void read_file(int file, ossia::value_port& port) noexcept
  {
    auto& d = port.get_data();
    if(d.empty())
      return;
    const auto* path = d.back().value.target<std::string>();
    if(!path || *path == m_paths[file])
      return;
    // Within the reserved capacity for any usual path: no allocation.
    m_paths[file].assign(*path);
    if(on_file)
      on_file(file, m_paths[file]);
  }

  //! The sounds asked for, or a built-in one while they are not there: files
  //! still decoding, a library without the classic ticks.
  const sound_set* current_sounds() const noexcept
  {
    if(const auto* s = sounds(sound))
      return s;
    return m_sets[Click].get();
  }

  void start_voice(const sound_set& set, click_kind kind) noexcept
  {
    const click_sound* snd = (kind == click_kind::Accent && accent) ? &set.accent : &set.beat;
    // One file is enough for both.
    if(snd->empty())
      snd = snd == &set.accent ? &set.beat : &set.accent;

    voice* free_voice{};
    voice* oldest{&m_voices[0]};
    for(auto& v : m_voices)
    {
      if(!v.snd)
      {
        if(!free_voice)
          free_voice = &v;
        continue;
      }
      if(v.fade_left < 0)
      {
        // Shorter when the click ends before: the fade still starts at 1.
        v.fade_total
            = int32_t(std::max<int64_t>(1, std::min<int64_t>(m_fade, v.snd->frames() - v.pos)));
        v.fade_left = v.fade_total;
      }
      if(v.pos > oldest->pos)
        oldest = &v;
    }
    if(snd->empty())
      return;

    // All of them fading: the one closest to silence makes room.
    auto& v = free_voice ? *free_voice : *oldest;
    v.snd = snd;
    v.pos = 0;
    v.gain = kind == click_kind::Subdivision ? subdivision_level : 1.f;
    v.fade_left = -1;
  }

  //! Adds what the voices play in [begin; end[ of the tick.
  void render(
      double* const out[2], int64_t begin, int64_t end, float volume_from,
      float volume_step) noexcept
  {
    if(end <= begin)
      return;
    for(auto& v : m_voices)
    {
      if(!v.snd)
        continue;
      const float* const left = v.snd->left.data();
      const float* const right
          = v.snd->right.size() == v.snd->left.size() ? v.snd->right.data() : left;
      int64_t count = std::min(end - begin, v.snd->frames() - v.pos);
      bool finished = count >= v.snd->frames() - v.pos;
      if(v.fade_left >= 0 && v.fade_left <= count)
      {
        count = v.fade_left;
        finished = true;
      }

      const float fade_step = 1.f / float(v.fade_total);
      float fade = v.fade_left >= 0 ? float(v.fade_left) * fade_step : 1.f;
      for(int64_t i = 0; i < count; i++)
      {
        const float g = v.gain * fade * (volume_from + volume_step * float(begin + i));
        out[0][begin + i] += double(left[v.pos + i] * g);
        out[1][begin + i] += double(right[v.pos + i] * g);
        if(v.fade_left >= 0)
          fade -= fade_step;
      }

      v.pos += count;
      if(v.fade_left >= 0)
        v.fade_left -= int32_t(count);
      if(finished)
        v.snd = nullptr;
    }
  }

  int m_rate{};
  //! Length of the fade-out of a click cut by the next one: 5 ms.
  int m_fade{};
  float m_last_volume{-1.f};
  std::string m_paths[2];
  std::array<sound_set_ptr, sound_count> m_sets;
  std::array<voice, max_voices> m_voices;
};
}
