#pragma once
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

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
  std::array<int8_t, 128> in_flight{};

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

  Engine()
  {
    // Rebuilding the arpeggio on the audio thread stays within these for any
    // reasonable chord: 16 held notes, back and forth, 8 repeats, 4 octaves
    // above or below (2 in both directions).
    notes.reserve(128);
    arpeggio.reserve(1024);
  }

  void update()
  {
    // Create the content of the arpeggio
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
        arpeggio.clear();
        arpeggio.resize(1);
        for(std::pair note : notes)
        {
          arpeggio[0].push_back(note);
          // Add octave duplicates directly into the chord
          for(int i = 1; i < previous_octave; i++)
          {
            // 0 = Both, 1 = Above, 2 = Below
            if(previous_octave_mode != 2) // Above or Both
            {
              int up = note.first + 12 * i;
              if(up <= 127)
                arpeggio[0].push_back({static_cast<byte>(up), note.second});
            }
            if(previous_octave_mode != 1) // Below or Both
            {
              int down = note.first - 12 * i;
              if(down >= 0)
                arpeggio[0].push_back({static_cast<byte>(down), note.second});
            }
          }
        }
        return; // Skip normal octavize and repeat for chord mode
      }
      case 5: // Random - note selection happens in process()
        arpeggiate(1);
        break;
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
      for(auto& note : arpeggio[play_index])
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
  halp_meta(category, "Midi")
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
