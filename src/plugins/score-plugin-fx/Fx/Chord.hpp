#pragma once
#include <Advanced/MidiScaler/MidiCommon/ChannelAllocator.hpp>
#include <Advanced/MidiScaler/MidiCommon/EventQueue.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/NoteTracker.hpp>
#include <Advanced/MidiScaler/MidiCommon/Theory.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <string_view>

namespace Nodes
{
namespace Chord
{
/**
 * @brief A list of integers written as text: "4 7 -12" or "100, 50".
 *
 * Parsing never allocates. Invalid or out-of-range numbers make the list
 * empty and `valid` false.
 */
template <int N>
struct int_list
{
  std::array<int, N> values{};
  int count{};
  bool valid{true};

  void parse(std::string_view s, int min, int max) noexcept
  {
    count = 0;
    valid = true;
    std::size_t i = 0;
    while(i < s.size())
    {
      const char c = s[i];
      if(c == ' ' || c == ',' || c == '\t' || c == '\n' || c == '\r')
      {
        i++;
        continue;
      }
      int v{};
      const char* first = s.data() + i;
      if(*first == '+')
        first++;
      auto [ptr, ec] = std::from_chars(first, s.data() + s.size(), v);
      if(ec != std::errc{} || v < min || v > max || count >= N)
      {
        count = 0;
        valid = false;
        return;
      }
      values[count++] = v;
      i = std::size_t(ptr - s.data());
    }
  }

  //! Element i, the last one repeating past the end; `fallback` when empty.
  int at_or_last(int i, int fallback) const noexcept
  {
    if(count == 0)
      return fallback;
    return values[std::min(i, count - 1)];
  }
};

//! Keeps an int_list in sync with a text control, parsing only on change.
template <int N, std::size_t MaxText = 256>
struct cached_int_list
{
  int_list<N> list;

  const int_list<N>& update(std::string_view text, int min, int max) noexcept
  {
    if(text.size() > MaxText)
      text = text.substr(0, MaxText);
    if(m_parsed && text == std::string_view{m_text.data(), m_size})
      return list;
    std::copy(text.begin(), text.end(), m_text.begin());
    m_size = text.size();
    m_parsed = true;
    list.parse(text, min, max);
    return list;
  }

private:
  std::array<char, MaxText> m_text{};
  std::size_t m_size{};
  bool m_parsed{};
};

struct Node
{
  halp_meta(name, "Chord")
  halp_meta(c_name, "Chord")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#chord")
  halp_meta(description, "Generate a chord from a single note")
  halp_meta(uuid, "F0904279-EA26-48DB-B0DF-F68FE3091DA1");

  //! The first six are saved in documents by name and keep their notes; the
  //! others follow mtk::chord_types, in its order.
  enum Chord : uint8_t
  {
    Maj,
    Min,
    Sus2,
    Sus4,
    Dim,
    Aug,
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09,
    T10, T11, T12, T13, T14, T15, T16, T17, T18, T19,
    T20, T21, T22, T23, T24, T25, T26, T27, T28, T29,
    T30, T31, T32, T33, T34, T35, T36
  };
  static constexpr int legacy_chord_count = 6;
  static constexpr int chord_count = legacy_chord_count + mtk::chord_type_count;
  static_assert(chord_count == T36 + 1);

  enum PlayMode : uint8_t
  {
    Table,
    Harmonizer,
    Memory
  };

  enum StrumDirection : uint8_t
  {
    Up,
    Down,
    Alternate
  };

  //! Voices one input note can produce: 8 intervals and the root, or a learned
  //! chord.
  static constexpr int max_voices = 12;
  static constexpr int max_intervals = 8;
  static constexpr int max_memory_text = 4096;

  using midi_out = halp::midi_out_bus<"out", libremidi::message>;
  struct ins
  {
    halp::midi_bus<"in", libremidi::message> midi;
    halp::hslider_i32<"Num. Notes", halp::irange{1, 5, 3}> num;
    struct : halp::enum_t<Chord, "Chord">
    {
      // Names of the table that would only differ by case from another
      // entry are spelled out: "sus2" is "sus2 triad" next to "Sus2", "M7+5"
      // is "maj7+5" next to "m7+5".
      struct range
      {
        std::string_view values[chord_count]
            = {"Maj",        "Min",       "Sus2",      "Sus4",     "Dim",
               "Aug",        "maj triad", "min triad", "dim triad", "aug triad",
               "5",          "sus2 triad", "sus4 triad", "6",       "7",
               "7-5",        "7+5",       "7sus4",     "maj7",     "maj7+5",
               "add4",       "add9",      "sus4add9",  "add11",    "69",
               "9",          "maj9",      "9sus4",     "7-9",      "7+11",
               "m6",         "m7",        "m7-5",      "m7+5",     "dim7",
               "mM7",        "madd4",     "madd9",     "m69",      "m9",
               "m7b9b5",     "m7add11",   "mM7add11"};
        Chord init = Maj;
      };
    } chord;

    struct : halp::enum_t<PlayMode, "Mode">
    {
      halp_meta(
          description,
          "Chord plays the chosen chord type, Harmonizer the intervals, Memory "
          "the chord learned for each key")
      struct range
      {
        std::string_view values[3] = {"Chord", "Harmonizer", "Memory"};
        PlayMode init = Table;
      };
    } mode;

    struct : halp::lineedit<"Intervals", "4 7">
    {
      halp_meta(
          description,
          "Harmonizer: up to 8 intervals in semitones from the played note, e.g. "
          "4 7 or -12 7. A repeated interval plays one note")
    } intervals;

    struct : halp::toggle<"Play root", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Harmonizer: also play the note itself")
    } root;

    struct : halp::spinbox_i32<"Inversion", halp::irange{-8, 8, 0}>
    {
      halp_meta(
          description,
          "Each step moves the lowest note an octave up (negative: the highest "
          "an octave down). Past the number of notes, the whole chord moves by "
          "octaves")
    } inversion;

    struct : halp::lineedit<"Voice chance", "100">
    {
      halp_meta(
          description,
          "Percent chance of each voice, from the root up, e.g. 100 50 50. The "
          "last value applies to the remaining voices")
    } chance;

    struct : halp::lineedit<"Voice velocity", "100">
    {
      halp_meta(
          description,
          "Percent of the played velocity for each voice, from the root up, e.g. "
          "100 80 60. The last value applies to the remaining voices")
    } velocity;

    struct : halp::spinbox_i32<"Seed", halp::irange{0, 10000, 0}>
    {
      halp_meta(
          description,
          "0 draws freely; any other value gives a reproducible sequence of "
          "chances, restarted on stop")
    } seed;

    struct : halp::time_chooser<"Strum", halp::range{0., 0.5, 0.}>
    {
      halp_meta(description, "Delay between successive notes of a chord")
    } strum;

    struct : halp::enum_t<StrumDirection, "Strum direction">
    {
      halp_meta(
          description, "Order of the strum: low to high, high to low, or both in turn")
      struct range
      {
        std::string_view values[3] = {"Up", "Down", "Alternate"};
        StrumDirection init = Up;
      };
    } strum_direction;

    struct : halp::knob_f32<"Tension", halp::range{-1., 1., 0.}>
    {
      halp_meta(
          description,
          "Above 0 the strum slows down across the chord, below 0 it speeds up")
    } tension;

    struct : halp::knob_f32<"Crescendo", halp::range{-1., 1., 0.}>
    {
      halp_meta(
          description,
          "Velocity change across the strum order: 1 doubles the last note, -1 "
          "fades it out")
    } crescendo;

    struct : halp::toggle<"Learn">
    {
      halp_meta(
          description,
          "Memory learning: play a chord, release it, then press the key that "
          "will trigger it")
    } learn;

    struct : halp::lineedit<"Memory", "">
    {
      halp_meta(
          description,
          "Learned chords: key, then semitones from the key, e.g. 60: 0 4 7; "
          "C3: 12 15 19. Keys may be note names")
      std::function<void(std::string)> update_controller;
    } memory;

    struct : halp::toggle<"MPE">
    {
      halp_meta(
          description,
          "Each voice on its own channel, with the pitch bend, pressure and "
          "CC 74 of the note that started it")
    } mpe;

    struct : halp::lineedit<"MPE channels", "2-16">
    {
      halp_meta(description, "Channels the MPE voices use, e.g. 2-16")
    } mpe_channels;
  } inputs;

  struct
  {
    midi_out midi;
  } outputs;

  struct ui
  {
    halp_meta(name, "Chord")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Chord")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::mode> mode;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::mode, PlayMode::Table>{};
        }
        halp::item<&ins::chord> chord;
        halp::item<&ins::num> num;
      } table;
      halp::item<&ins::inversion> inversion;
    } chord;

    struct
    {
      halp_meta(name, "Strum")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::strum> strum;
      halp::item<&ins::strum_direction, halp::label_as("Direction")> strum_direction;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::tension> tension;
        halp::item<&ins::crescendo> crescendo;
      } shape;
    } strum;

    struct
    {
      halp_meta(name, "Settings")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_darker)

      struct
      {
        halp_meta(name, "Harmonizer")
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::mode, PlayMode::Harmonizer>{};
        }
        halp::item<&ins::intervals> intervals;
        halp::item<&ins::root> root;
      } harmonizer;

      struct
      {
        halp_meta(name, "Voices")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::chance> chance;
        halp::item<&ins::velocity> velocity;
        halp::item<&ins::seed> seed;
      } voices;

      struct
      {
        halp_meta(name, "Memory")
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::mode, PlayMode::Memory>{};
        }
        halp::item<&ins::learn> learn;
        halp::item<&ins::memory> memory;
      } memory;

      struct
      {
        halp_meta(name, "MPE")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::mpe> mpe;
        halp::item<&ins::mpe_channels, halp::label_as("Channels")> mpe_channels;
      } mpe;
    } settings;
  };

  // C C# D D# E F F# G G# A A# B
  // 1 .  . .  1 . .  1 .  1 .  .
  static const constexpr std::array<int, 5> major7{0, 4, 7, 11, 12};
  static const constexpr std::array<int, 5> minor7{0, 3, 7, 10, 12};
  static const constexpr std::array<int, 5> sus2{0, 2, 7, 9, 12};
  static const constexpr std::array<int, 5> sus4{0, 5, 7, 9, 12};
  static const constexpr std::array<int, 5> dim{0, 3, 6, 9, 12};
  static const constexpr std::array<int, 5> aug{0, 4, 8, 10, 12};
  static constexpr std::array<const std::array<int, 5>*, legacy_chord_count> legacy_chords{
      &major7, &minor7, &sus2, &sus4, &dim, &aug};

  struct learned_chord
  {
    int8_t count{};
    std::array<int8_t, max_voices> offsets{};
  };

  Node()
  {
    std::random_device rd;
    m_free_seed = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    reseed();
  }

  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0.)
      m_rate = info.rate;
  }

  void stop() noexcept { m_stop_requested = true; }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk) noexcept
  {
    const int frames = tk.frames > 0 ? tk.frames : 0;
    update_parameters();

    if(m_stop_requested)
    {
      m_stop_requested = false;
      panic(0);
    }

    for(const libremidi::message& m : inputs.midi)
    {
      const int64_t ts
          = frames > 0 ? std::clamp<int64_t>(m.timestamp, 0, frames - 1) : 0;
      release_queue(int(ts));
      handle(m, ts);
    }
    release_queue(frames);
    m_now += frames;
  }

  //! Voices of the chord a note-on plays, as semitones from the note, in
  //! voice order (root first). For tests and the inspector.
  int voices(int pitch, std::array<int, max_voices>& out) noexcept
  {
    update_parameters();
    std::array<voice, max_voices> v{};
    const int n = build_voices(pitch, v);
    for(int i = 0; i < n; i++)
      out[i] = v[i].offset;
    return n;
  }

private:
  struct voice
  {
    int offset{};
    int index{};
    int velocity{};
    int64_t delay{};
  };

  using tracker_type = mtk::note_tracker<128, max_voices>;

  static uint32_t tag_of(int channel, int pitch) noexcept
  {
    return uint32_t(((channel - 1) << 7) | pitch) + 1;
  }

  // ------------------------------------------------------------ parameters

  void update_parameters() noexcept
  {
    const int seed = inputs.seed;
    if(seed != m_seed)
    {
      m_seed = seed;
      reseed();
    }

    m_intervals.update(inputs.intervals.value, -127, 127);
    m_chance.update(inputs.chance.value, 0, 100);
    m_velocity.update(inputs.velocity.value, 0, 400);
    update_memory(inputs.memory.value);

    const auto& chans = m_mpe_channels.update(inputs.mpe_channels.value, 1, 16);
    uint16_t mask = 0;
    for(int ch = 1; ch <= 16; ch++)
      if(chans.contains(ch))
        mask |= uint16_t(1u << (ch - 1));
    m_alloc.set_channels(mask != 0 ? mask : uint16_t(0xFFFE));

    if(!inputs.learn.value)
      reset_learning();
  }

  void reseed() noexcept
  {
    m_rng = m_seed != 0 ? uint64_t(m_seed) * UINT64_C(0x9E3779B97F4A7C15) : m_free_seed;
  }

  //! splitmix64, so that a seed gives the same draws on every platform.
  double random01() noexcept
  {
    uint64_t z = (m_rng += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    z = z ^ (z >> 31);
    return double(z >> 11) * 0x1.0p-53;
  }

  // ---------------------------------------------------------------- memory

  //! Parses the memory text when it changed since the last tick. Learning
  //! edits the table directly, and the text it sends to the control comes
  //! back here as an ordinary change.
  void update_memory(std::string_view text) noexcept
  {
    if(text.size() > max_memory_text)
      text = text.substr(0, max_memory_text);
    if(m_memory_parsed && text == std::string_view{m_memory_text.data(), m_memory_size})
      return;
    std::copy(text.begin(), text.end(), m_memory_text.begin());
    m_memory_size = text.size();
    m_memory_parsed = true;
    parse_memory(text, m_memory);
  }

public:
  //! "60: 0 4 7; C3: 12 15 19". Entries are separated by ';' or new lines;
  //! an entry that does not parse is skipped.
  static void parse_memory(std::string_view s, std::array<learned_chord, 128>& table) noexcept
  {
    table = {};
    std::size_t start = 0;
    while(start <= s.size())
    {
      std::size_t end = start;
      while(end < s.size() && s[end] != ';' && s[end] != '\n')
        end++;
      parse_memory_entry(s.substr(start, end - start), table);
      start = end + 1;
    }
  }

  //! The table as text, in key order. Returns the length written.
  static std::size_t
  print_memory(const std::array<learned_chord, 128>& table, char* buf, std::size_t size) noexcept
  {
    std::size_t n = 0;
    for(int key = 0; key < 128; key++)
    {
      const auto& c = table[key];
      if(c.count <= 0)
        continue;
      char entry[128];
      std::size_t k = 0;
      if(n > 0)
      {
        entry[k++] = ';';
        entry[k++] = ' ';
      }
      k = std::size_t(std::to_chars(entry + k, entry + sizeof(entry), key).ptr - entry);
      entry[k++] = ':';
      for(int i = 0; i < c.count; i++)
      {
        entry[k++] = ' ';
        k = std::size_t(
            std::to_chars(entry + k, entry + sizeof(entry), int(c.offsets[i])).ptr - entry);
      }
      if(n + k > size)
        break;
      std::copy_n(entry, k, buf + n);
      n += k;
    }
    return n;
  }

private:
  static void parse_memory_entry(std::string_view e, std::array<learned_chord, 128>& table) noexcept
  {
    const auto colon = e.find(':');
    if(colon == std::string_view::npos)
      return;
    auto key_text = e.substr(0, colon);
    while(!key_text.empty() && (key_text.front() == ' ' || key_text.front() == '\t'))
      key_text.remove_prefix(1);
    while(!key_text.empty() && (key_text.back() == ' ' || key_text.back() == '\t' || key_text.back() == '\r'))
      key_text.remove_suffix(1);
    if(key_text.empty())
      return;

    int key = -1;
    if(key_text[0] >= '0' && key_text[0] <= '9')
    {
      auto [ptr, ec] = std::from_chars(key_text.data(), key_text.data() + key_text.size(), key);
      if(ec != std::errc{} || ptr != key_text.data() + key_text.size())
        return;
    }
    else if(auto n = mtk::parse_note_name(key_text); n && n->second == key_text.size())
    {
      key = n->first;
    }
    if(key < 0 || key > 127)
      return;

    int_list<max_voices> offsets;
    offsets.parse(e.substr(colon + 1), -127, 127);
    if(!offsets.valid || offsets.count == 0)
      return;
    auto& c = table[key];
    c.count = int8_t(offsets.count);
    for(int i = 0; i < offsets.count; i++)
      c.offsets[i] = int8_t(offsets.values[i]);
  }

  // -------------------------------------------------------------- learning

  void reset_learning() noexcept
  {
    m_learn_down = {};
    m_learn_down_count = 0;
    m_group_count = 0;
    m_captured_count = 0;
    m_trigger_down = false;
  }

  //! A chord is every key pressed between the first press and the moment
  //! no key is held anymore. The next key pressed alone is its trigger.
  void learn_note_on(int pitch) noexcept
  {
    if(m_learn_down[pitch])
      return;
    if(m_learn_down_count == 0)
    {
      m_group_count = 0;
      if(m_captured_count >= 2)
      {
        assign_learned(pitch);
        m_captured_count = 0;
        m_trigger_down = true;
      }
    }
    m_learn_down[pitch] = true;
    m_learn_down_count++;
    if(!m_trigger_down && m_group_count < max_voices)
      m_group[m_group_count++] = pitch;
  }

  void learn_note_off(int pitch) noexcept
  {
    if(!m_learn_down[pitch])
      return;
    m_learn_down[pitch] = false;
    if(--m_learn_down_count > 0)
      return;
    if(!m_trigger_down && m_group_count >= 2)
    {
      m_captured = m_group;
      m_captured_count = m_group_count;
      std::sort(m_captured.begin(), m_captured.begin() + m_captured_count);
    }
    m_trigger_down = false;
    m_group_count = 0;
  }

  void assign_learned(int key) noexcept
  {
    auto& c = m_memory[key];
    c.count = int8_t(m_captured_count);
    for(int i = 0; i < m_captured_count; i++)
      c.offsets[i] = int8_t(m_captured[i] - key);

    // Sending the text to the control is the one place that allocates: it is
    // what saves the learned chord with the document, and it only happens on
    // a learning gesture.
    if(inputs.memory.update_controller)
    {
      const auto n = print_memory(m_memory, m_print_buffer.data(), m_print_buffer.size());
      inputs.memory.update_controller(std::string(m_print_buffer.data(), n));
    }
  }

  // ---------------------------------------------------------------- voices

  int build_voices(int pitch, std::array<voice, max_voices>& v) noexcept
  {
    int n = 0;
    auto add = [&](int offset) {
      if(n < max_voices)
      {
        v[n].offset = offset;
        v[n].index = n;
        n++;
      }
    };

    if(inputs.learn.value)
    {
      add(0);
      return n;
    }

    switch(inputs.mode.value)
    {
      case Table:
      default: {
        const int chord = std::clamp(int(inputs.chord.value), 0, chord_count - 1);
        const int num = std::clamp(int(inputs.num.value), 1, max_voices);
        if(chord < legacy_chord_count)
        {
          const auto& c = *legacy_chords[chord];
          for(int i = 0; i < std::min(num, int(c.size())); i++)
            add(c[i]);
        }
        else
        {
          const auto notes = mtk::chord_types[chord - legacy_chord_count].notes();
          for(int i = 0; i < std::min(num, int(notes.size())); i++)
            add(notes[i]);
        }
        break;
      }
      case Harmonizer: {
        if(inputs.root.value)
          add(0);
        const auto& iv = m_intervals.list;
        for(int i = 0; i < std::min(iv.count, max_intervals); i++)
        {
          bool seen = false;
          for(int k = 0; k < n; k++)
            seen |= v[k].offset == iv.values[i];
          if(!seen)
            add(iv.values[i]);
        }
        break;
      }
      case Memory: {
        const auto& c = m_memory[pitch & 127];
        if(c.count > 0)
          for(int i = 0; i < c.count; i++)
            add(c.offsets[i]);
        else
          add(0);
        break;
      }
    }

    invert(v, n, inputs.inversion.value);
    return n;
  }

  //! Moves the lowest voice an octave up `steps` times (the highest down
  //! when negative), then keeps one voice per pitch.
  static void invert(std::array<voice, max_voices>& v, int& n, int steps) noexcept
  {
    if(n == 0)
      return;
    for(int s = 0; s < steps; s++)
    {
      int lo = 0;
      for(int i = 1; i < n; i++)
        if(v[i].offset < v[lo].offset)
          lo = i;
      v[lo].offset += 12;
    }
    for(int s = 0; s > steps; s--)
    {
      int hi = 0;
      for(int i = 1; i < n; i++)
        if(v[i].offset > v[hi].offset)
          hi = i;
      v[hi].offset -= 12;
    }
    int w = 0;
    for(int r = 0; r < n; r++)
    {
      bool seen = false;
      for(int k = 0; k < w; k++)
        seen |= v[k].offset == v[r].offset;
      if(!seen)
        v[w++] = v[r];
    }
    n = w;
  }

  // -------------------------------------------------------------- handling

  void handle(const libremidi::message& m, int64_t ts) noexcept
  {
    const int ch = mtk::channel_of(m);
    switch(mtk::classify(m))
    {
      case mtk::midi_kind::note_on:
        note_on(ch, m.bytes[1] & 127, m.bytes[2] & 127, ts);
        return;
      case mtk::midi_kind::note_off:
        if(m.size() >= 3)
        {
          const int vel = (m.bytes[0] & 0xF0) == 0x80 ? (m.bytes[2] & 127) : 0;
          note_off(ch, m.bytes[1] & 127, vel, ts);
          return;
        }
        break;
      case mtk::midi_kind::control_change:
        if(mtk::is_all_notes_off(m))
        {
          release_channel(ch, ts);
          break;
        }
        if(m.size() >= 3 && m.bytes[1] == 74 && forward_expression(m, ch, -1))
          return;
        break;
      case mtk::midi_kind::pitch_bend:
        if(m.size() >= 3)
          m_in_bend[ch - 1] = int16_t((m.bytes[2] << 7) | m.bytes[1]);
        if(forward_expression(m, ch, -1))
          return;
        break;
      case mtk::midi_kind::channel_pressure:
        if(forward_expression(m, ch, -1))
          return;
        break;
      case mtk::midi_kind::poly_pressure:
        if(m.size() >= 3 && forward_expression(m, ch, m.bytes[1] & 127))
          return;
        break;
      default:
        break;
    }
    outputs.midi.push_back(m);
  }

  void note_on(int ch, int pitch, int vel, int64_t ts) noexcept
  {
    if(inputs.learn.value)
      learn_note_on(pitch);

    // A key pressed again without a release ends its previous chord first.
    if(m_tracker.holds(ch, pitch))
      end_input(ch, pitch, 0, ts);

    if(m_tracker.size() == 128)
    {
      const auto& oldest = m_tracker[0];
      cancel_pending(oldest.channel, oldest.pitch);
    }
    m_tracker.begin(ch, pitch, ts, [this](libremidi::message&& m) { emit(std::move(m)); });

    std::array<voice, max_voices> v{};
    int n = build_voices(pitch, v);

    // Chance and velocity per voice index; the draw happens for every voice
    // so that the sequence of a seed does not depend on the values.
    int kept = 0;
    for(int i = 0; i < n; i++)
    {
      const double draw = random01() * 100.;
      const int chance = m_chance.list.at_or_last(v[i].index, 100);
      const int percent = m_velocity.list.at_or_last(v[i].index, 100);
      const int out = pitch + v[i].offset;
      if(draw >= chance || out < 0 || out > 127)
        continue;
      v[kept] = v[i];
      v[kept].offset = out;
      v[kept].velocity = int(std::lround(vel * percent / 100.));
      kept++;
    }
    n = kept;

    // Strum order
    bool down = inputs.strum_direction.value == Down;
    if(inputs.strum_direction.value == Alternate)
    {
      down = m_alternate_down;
      m_alternate_down = !m_alternate_down;
    }
    // Insertion sort: stable, and std::stable_sort may allocate.
    for(int i = 1; i < n; i++)
    {
      const voice x = v[i];
      int j = i;
      while(j > 0 && (down ? v[j - 1].offset < x.offset : v[j - 1].offset > x.offset))
      {
        v[j] = v[j - 1];
        j--;
      }
      v[j] = x;
    }

    const double strum = std::max(0., double(inputs.strum.value)) * m_rate;
    const double tension = std::clamp(double(inputs.tension.value), -1., 1.);
    const double crescendo = std::clamp(double(inputs.crescendo.value), -1., 1.);
    const double exponent = std::exp2(2. * tension);
    for(int k = 0; k < n; k++)
    {
      const double x = n > 1 ? double(k) / double(n - 1) : 0.;
      const double total = strum * double(n - 1);
      v[k].delay = int64_t(std::llround(total * std::pow(x, exponent)));
      v[k].velocity
          = std::clamp(int(std::lround(v[k].velocity * (1. + crescendo * x))), 1, 127);
    }

    auto* h = m_tracker.find(ch, pitch);
    if(!h)
      return;
    for(int k = 0; k < n; k++)
    {
      if(v[k].delay <= 0)
      {
        start_voice(*h, ch, v[k].offset, v[k].velocity, ts);
      }
      else
      {
        m_queue.schedule(
            m_now + ts + v[k].delay,
            libremidi::channel_events::note_on(ch, v[k].offset, v[k].velocity),
            tag_of(ch, pitch));
      }
    }
  }

  //! The note-off of a key the object does not hold is dropped: it has
  //! nothing of it to end, and the key's own note never went out.
  void note_off(int ch, int pitch, int vel, int64_t ts) noexcept
  {
    if(inputs.learn.value)
      learn_note_off(pitch);
    end_input(ch, pitch, vel, ts);
  }

  bool end_input(int ch, int pitch, int vel, int64_t ts) noexcept
  {
    cancel_pending(ch, pitch);
    return m_tracker.end(
        ch, pitch, ts, vel, [this](libremidi::message&& m) { emit(std::move(m)); });
  }

  void cancel_pending(int ch, int pitch) noexcept
  {
    const uint32_t tag = tag_of(ch, pitch);
    m_queue.remove_if([tag](const auto& e) { return e.tag == tag; });
  }

  void start_voice(tracker_type::held& h, int in_ch, int pitch, int vel, int64_t ts) noexcept
  {
    if(h.count >= max_voices || pitch < 0 || pitch > 127)
      return;
    auto send = [this](libremidi::message&& m) { emit(std::move(m)); };
    if(!inputs.mpe.value)
    {
      m_tracker.play(h, in_ch, pitch, vel, ts, send);
      return;
    }

    const auto ch = m_alloc.allocate(m_now + ts, [this] { return random01(); });
    if(!ch)
      return;
    const int16_t bend = m_in_bend[in_ch - 1];
    if(m_out_bend[*ch - 1] != bend)
    {
      auto b = libremidi::channel_events::pitch_bend(*ch, int(bend));
      b.timestamp = ts;
      outputs.midi.push_back(std::move(b));
      m_out_bend[*ch - 1] = bend;
    }
    m_mpe_voice[*ch - 1][pitch] = true;
    m_tracker.play(h, *ch, pitch, vel, ts, send);
  }

  void emit(libremidi::message&& m) noexcept
  {
    if(mtk::is_note_off(m))
    {
      const int ch = mtk::channel_of(m);
      auto& mpe = m_mpe_voice[ch - 1][m.bytes[1] & 127];
      if(mpe)
      {
        mpe = false;
        m_alloc.release(ch, m_now + m.timestamp);
      }
    }
    outputs.midi.push_back(std::move(m));
  }

  //! Sends a per-note expression message of input channel `ch` (and input
  //! note `pitch` for poly pressure) to the channels of that note's voices.
  //! Returns false when it does not apply: the message then passes through.
  bool forward_expression(const libremidi::message& m, int ch, int pitch) noexcept
  {
    if(!inputs.mpe.value)
      return false;
    uint16_t sent = 0;
    bool any = false;
    for(int i = 0; i < m_tracker.size(); i++)
    {
      const auto& h = m_tracker[i];
      if(h.channel != ch || (pitch >= 0 && h.pitch != pitch))
        continue;
      for(int k = 0; k < h.count; k++)
      {
        const auto& o = h.outputs[k];
        any = true;
        if(pitch >= 0)
        {
          auto c = m;
          mtk::set_channel(c, o.channel);
          c.bytes[1] = o.pitch;
          outputs.midi.push_back(std::move(c));
          continue;
        }
        if(sent & (1u << (o.channel - 1)))
          continue;
        sent |= uint16_t(1u << (o.channel - 1));
        auto c = m;
        mtk::set_channel(c, o.channel);
        if(mtk::classify(c) == mtk::midi_kind::pitch_bend)
          m_out_bend[o.channel - 1] = m_in_bend[ch - 1];
        outputs.midi.push_back(std::move(c));
      }
    }
    return any;
  }

  //! All notes off / all sound off: in MPE mode the input channels are those
  //! of the notes, so everything is released; otherwise only that channel.
  void release_channel(int ch, int64_t ts) noexcept
  {
    if(inputs.mpe.value)
    {
      m_queue.clear();
      m_tracker.end_all(ts, [this](libremidi::message&& m) { emit(std::move(m)); });
      return;
    }
    std::array<uint8_t, 128> pitches{};
    int n = 0;
    for(int i = 0; i < m_tracker.size() && n < 128; i++)
      if(m_tracker[i].channel == ch)
        pitches[n++] = m_tracker[i].pitch;
    for(int i = 0; i < n; i++)
      end_input(ch, pitches[i], 0, ts);
  }

  void release_queue(int upto) noexcept
  {
    m_queue.release(m_now, upto, [this](libremidi::message&& m, uint32_t tag) {
      if(tag == 0)
        return;
      const int ch = int((tag - 1) >> 7) + 1;
      const int pitch = int((tag - 1) & 127);
      if(auto* h = m_tracker.find(ch, pitch))
        start_voice(*h, ch, m.bytes[1], m.bytes[2], m.timestamp);
    });
  }

  void panic(int64_t ts) noexcept
  {
    m_queue.clear();
    m_tracker.end_all(ts, [this](libremidi::message&& m) { emit(std::move(m)); });
    m_alloc.reset();
    m_mpe_voice = {};
    m_alternate_down = false;
    reset_learning();
    reseed();
  }

  tracker_type m_tracker;
  mtk::event_queue<512> m_queue;
  mtk::channel_allocator m_alloc;
  mtk::cached_range_list m_mpe_channels;
  cached_int_list<max_intervals> m_intervals;
  cached_int_list<max_voices> m_chance;
  cached_int_list<max_voices> m_velocity;

  std::array<learned_chord, 128> m_memory{};
  std::array<char, max_memory_text> m_memory_text{};
  std::size_t m_memory_size{};
  bool m_memory_parsed{};
  std::array<char, max_memory_text> m_print_buffer{};

  std::array<bool, 128> m_learn_down{};
  int m_learn_down_count{};
  std::array<int, max_voices> m_group{};
  int m_group_count{};
  std::array<int, max_voices> m_captured{};
  int m_captured_count{};
  bool m_trigger_down{};

  std::array<std::array<bool, 128>, 16> m_mpe_voice{};
  std::array<int16_t, 16> m_in_bend{8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192,
                                    8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192};
  std::array<int16_t, 16> m_out_bend{m_in_bend};

  double m_rate{48000.};
  int64_t m_now{};
  uint64_t m_rng{};
  uint64_t m_free_seed{};
  int m_seed{};
  bool m_alternate_down{};
  bool m_stop_requested{};
};
}
}
