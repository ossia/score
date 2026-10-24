#pragma once
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

#include <algorithm>
#include <array>
#include <span>

namespace Nodes
{
template <typename T>
static void duplicate_vector(T& vec)
{
  const int N = vec.size();
  vec.reserve(N * 2);
  for(int i = 0; i < N; i++)
    vec.push_back(vec[i]);
}
namespace Arpeggiator
{
struct Arpeggios
{
  halp_meta(name, "Arpeggios");
  enum widget
  {
    combobox
  };

  struct range
  {
    halp::combo_pair<int> values[6]{{"Forward", 0}, {"Backward", 1}, {"F->B", 2}, {"B->F", 3}, {"Chord", 4}, {"Random", 5}};
    int init{0};
  };

  int value{};
};

struct OctaveMode
{
  halp_meta(name, "Octave mode");
  enum widget
  {
    combobox
  };

  struct range
  {
    halp::combo_pair<int> values[3]{{"Both", 0}, {"Above", 1}, {"Below", 2}};
    int init{0};
  };

  int value{};
};

//! The arpeggio, shared by the old and the current object: they differ only in
//! how the rate is chosen.
struct Engine
{
  using byte = unsigned char;
  using chord = ossia::small_vector<std::pair<byte, byte>, 5>;

  ossia::flat_map<byte, byte> notes;
  ossia::small_vector<chord, 10> arpeggio;
  //! The Chord pattern's single step. It holds every note and its octave
  //! copies, more than a chord's inline storage: kept apart from `arpeggio`,
  //! whose steps are destroyed and made again at each update, so that its
  //! storage is allocated once.
  chord m_chord;
  std::array<int8_t, 128> in_flight{};
  //! The held notes in the order they came, for "As played".
  std::array<byte, 128> order{};
  int order_count{};

  std::array<std::pair<byte, byte>, 128> m_ext{};
  int m_ext_count{};

  int previous_octave{};
  int previous_octave_mode{};
  int previous_repeat{};
  int previous_arpeggio{};
  std::size_t index{};
  rnd::pcg rng{[] {
    std::random_device d{};
    rnd::pcg r(d);
    return r;
  }()};

  static constexpr int max_octave = 7;
  static constexpr int max_repeat = 8;
  //! The longest arpeggio: 128 notes back and forth, each repeated, copied
  //! over 6 octaves above and 6 below.
  static constexpr std::size_t max_steps
      = 128 * 2 * max_repeat * (2 * (max_octave - 1) + 1);
  //! The largest chord: 128 notes, each copied over 6 octaves above and 6 below.
  static constexpr std::size_t max_chord = 128 * (2 * (max_octave - 1) + 1);

  Engine()
  {
    // Rebuilding the arpeggio happens on the audio thread: it never grows
    // past these
    notes.reserve(128);
    arpeggio.reserve(max_steps);
    m_chord.reserve(max_chord);
  }

  //! The notes of a step of the arpeggio.
  const chord& step(std::size_t i) const noexcept
  {
    return previous_arpeggio == 4 ? m_chord : arpeggio[i];
  }

  void update()
  {
    previous_octave = std::clamp(previous_octave, 1, max_octave);
    previous_repeat = std::clamp(previous_repeat, 1, max_repeat);

    // Create the content of the arpeggio
    bool spans_octaves = false;
    switch(previous_arpeggio)
    {
      case 0: // Forward
        arpeggiate(1);
        break;
      case 1: // Backward
        arpeggiate(1);
        std::reverse(arpeggio.begin(), arpeggio.end());
        break;
      case 2: // F->B
        arpeggiate(2);
        duplicate_vector(arpeggio);
        std::reverse(arpeggio.begin() + notes.size(), arpeggio.end());
        break;
      case 3: // B->F
        arpeggiate(2);
        duplicate_vector(arpeggio);
        std::reverse(arpeggio.begin(), arpeggio.begin() + notes.size());
        break;
      case 4: // Chord - all notes play simultaneously, octaves expand the chord
      {
        // A single step, played from m_chord: an empty one in the arpeggio
        // stands for it
        arpeggio.clear();
        arpeggio.resize(1);
        m_chord.clear();
        for(std::pair note : notes)
        {
          m_chord.push_back(note);
          // Add octave duplicates directly into the chord
          for(int i = 1; i < previous_octave; i++)
          {
            // 0 = Both, 1 = Above, 2 = Below
            if(previous_octave_mode != 2) // Above or Both
            {
              int up = note.first + 12 * i;
              if(up <= 127)
                m_chord.push_back({static_cast<byte>(up), note.second});
            }
            if(previous_octave_mode != 1) // Below or Both
            {
              int down = note.first - 12 * i;
              if(down >= 0)
                m_chord.push_back({static_cast<byte>(down), note.second});
            }
          }
        }
        return; // Skip normal octavize and repeat for chord mode
      }
      case 5: // Random - note selection happens in process()
        arpeggiate(1);
        break;
      case 6: // Up/Down: the top and the bottom once each
      case 7: // Down/Up
      {
        const std::span ext = notes_over_octaves();
        arpeggio.clear();
        const int n = ext.size();
        for(int i = 0; i < n; i++)
          arpeggio.push_back(chord{ext[i]});
        for(int i = n - 2; i > 0; i--)
          arpeggio.push_back(chord{ext[i]});
        if(previous_arpeggio == 7 && n > 1)
          std::rotate(arpeggio.begin(), arpeggio.begin() + (n - 1), arpeggio.end());
        spans_octaves = true;
        break;
      }
      case 8: // Converge: from the outside in
      case 9: // Diverge: from the inside out
      {
        const std::span ext = notes_over_octaves();
        arpeggio.clear();
        int lo = 0, hi = int(ext.size()) - 1;
        while(lo <= hi)
        {
          arpeggio.push_back(chord{ext[lo]});
          if(hi != lo)
            arpeggio.push_back(chord{ext[hi]});
          lo++;
          hi--;
        }
        if(previous_arpeggio == 9)
          std::reverse(arpeggio.begin(), arpeggio.end());
        spans_octaves = true;
        break;
      }
      case 10: // Low Up: the lowest note between each of the others
      case 11: // High Up: the highest note between each of the others
      {
        const std::span ext = notes_over_octaves();
        arpeggio.clear();
        const int n = ext.size();
        if(n == 1)
          arpeggio.push_back(chord{ext[0]});
        const int pivot = previous_arpeggio == 10 ? 0 : n - 1;
        for(int i = 0; i < n; i++)
        {
          if(i == pivot)
            continue;
          arpeggio.push_back(chord{ext[pivot]});
          arpeggio.push_back(chord{ext[i]});
        }
        spans_octaves = true;
        break;
      }
      case 12: // As played
      {
        arpeggio.clear();
        for(int i = 0; i < order_count; i++)
          if(auto it = notes.find(order[i]); it != notes.end())
            arpeggio.push_back(chord{*it});
        break;
      }
    }

    // Apply repeat: each step N times, expanded in place from the back
    if(previous_repeat > 1)
    {
      const std::size_t n = arpeggio.size();
      const std::size_t rep = previous_repeat;
      arpeggio.resize(n * rep);
      for(std::size_t i = n; i-- > 0;)
        for(std::size_t r = rep; r-- > 0;)
          arpeggio[i * rep + r] = arpeggio[i];
    }

    // These patterns already run across the octaves.
    if(spans_octaves)
      return;

    const std::size_t orig_size = arpeggio.size();

    // Create the octave duplicates based on octave mode
    // 0 = Both, 1 = Above, 2 = Below
    if(previous_octave_mode != 2) // Above or Both
    {
      for(int i = 1; i < previous_octave; i++)
        octavize(orig_size, i);
    }
    if(previous_octave_mode != 1) // Below or Both
    {
      for(int i = 1; i < previous_octave; i++)
        octavize(orig_size, -i);
    }
  }

  //! The held notes and their octave copies, lowest first, without
  //! duplicates (the copy from the lowest octave, then the lowest held note,
  //! wins): what the patterns that run across the octaves walk through.
  //! In a member buffer, as it is built on the audio thread.
  std::span<const std::pair<byte, byte>> notes_over_octaves() noexcept
  {
    std::array<int16_t, 128> velocity_of;
    velocity_of.fill(-1);
    const int lo_oct = previous_octave_mode != 1 ? -(previous_octave - 1) : 0;
    const int hi_oct = previous_octave_mode != 2 ? (previous_octave - 1) : 0;
    for(int o = lo_oct; o <= hi_oct; o++)
      for(std::pair note : notes)
      {
        const int p = note.first + 12 * o;
        if(p >= 0 && p <= 127 && velocity_of[p] < 0)
          velocity_of[p] = note.second;
      }
    m_ext_count = 0;
    for(int p = 0; p < 128; p++)
      if(velocity_of[p] >= 0)
        m_ext[m_ext_count++] = {byte(p), byte(velocity_of[p])};
    return {m_ext.data(), std::size_t(m_ext_count)};
  }

  void arpeggiate(int size_mult)
  {
    arpeggio.clear();
    arpeggio.reserve(notes.size() * size_mult);
    for(std::pair note : notes)
    {
      arpeggio.push_back(chord{note});
    }
  }

  void octavize(std::size_t orig_size, int i)
  {
    for(std::size_t j = 0; j < orig_size; j++)
    {
      auto copy = arpeggio[j];
      for(auto it = copy.begin(); it != copy.end();)
      {
        auto& note = *it;
        int res = note.first + 12 * i;
        if(res >= 0 && res <= 127)
        {
          note.first = res;
          ++it;
        }
        else
        {
          it = copy.erase(it);
        }
      }

      arpeggio.push_back(std::move(copy));
    }
  }

  template <typename Out>
  void all_off(Out& out, int date)
  {
    for(int k = 0; k < 128; k++)
    {
      while(in_flight[k] > 0)
      {
        out.note_off(1, k, 0).timestamp = date;
        in_flight[k]--;
      }
    }
  }

  //! `rate` is the grid's: 1 a whole note, 4 a quarter...
  template <typename In, typename Out>
  void process(
      const In& msgs, Out& out, int octave, int octave_mode, int repeat,
      int arpeggio_mode, const halp::tick_musical& tk, double rate)
  {
    // Update the "running" notes. A note-on of velocity 0 is a note-off.
    for(auto& note : msgs)
    {
      const auto type = note.get_message_type();
      if(type == libremidi::message_type::NOTE_ON && note.bytes[2] != 0)
        notes.insert({note.bytes[1], note.bytes[2]});
      else if(
          type == libremidi::message_type::NOTE_OFF
          || type == libremidi::message_type::NOTE_ON)
        notes.erase(note.bytes[1]);
    }

    // Update the arpeggio itself
    const bool mustUpdateArpeggio = msgs.size() > 0 || octave != previous_octave
                                    || octave_mode != previous_octave_mode
                                    || repeat != previous_repeat
                                    || arpeggio_mode != previous_arpeggio;
    previous_octave = octave;
    previous_octave_mode = octave_mode;
    previous_repeat = repeat;
    previous_arpeggio = arpeggio_mode;

    if(mustUpdateArpeggio)
      update();

    if(arpeggio.empty())
    {
      all_off(out, 0);
      return;
    }

    if(index >= arpeggio.size())
      index = 0;

    // Play the next note / chord if we're on a quantification marker
    for(auto [date, q] : tk.get_quantification_date_with_bars(rate))
    {
      if(date >= tk.frames)
        return;

      // Finish previous notes
      all_off(out, date);

      // Select the next index: random for Random mode, sequential otherwise
      std::size_t play_index;
      if(arpeggio_mode == 5) // Random
      {
        std::uniform_int_distribution<std::size_t> dist(0, arpeggio.size() - 1);
        play_index = dist(rng);
      }
      else
      {
        play_index = index;
        index = (index + 1) % arpeggio.size();
      }

      // Start the next note in the chord
      for(auto& note : step(play_index))
      {
        in_flight[note.first]++;
        out.note_on(1, note.first, note.second).timestamp = date;
      }
    }
  }
};

struct Node
{
  halp_meta(name, "Arpeggiator (old)")
  halp_meta(c_name, "Arpeggiator")
  halp_meta(category, "Midi/Generators")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#arpeggiator")
  halp_meta(description, "Arpeggiator")
  halp_flag(deprecated);
  halp_meta(uuid, "0b98c7cd-f831-468f-81e3-706d6a97d705")

  // FIXME "note" bus instead of midi bus ; the host handles passing all the non note messages
  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    Arpeggios arpeggios;
    halp::hslider_i32<"Octave", halp::irange{1, 7, 1}> octave;
    OctaveMode octave_mode;
    halp::hslider_i32<"Repeat", halp::irange{1, 8, 1}> repeat;
    halp::hslider_i32<"Quantification", halp::irange{1, 32, 8}> quantification;
  } inputs;
  struct
  {
    halp::midi_out_bus<"out", libremidi::message> midi;
  } outputs;

  Engine engine;

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    engine.process(
        inputs.midi, outputs.midi, inputs.octave, inputs.octave_mode.value,
        inputs.repeat, inputs.arpeggios.value, tk, inputs.quantification.value);
  }
};
}
}
