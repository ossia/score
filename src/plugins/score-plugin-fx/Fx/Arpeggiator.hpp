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

struct Node
{
  halp_meta(name, "Arpeggiator")
  halp_meta(c_name, "Arpeggiator")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#arpeggiator")
  halp_meta(description, "Arpeggiator")
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
      case 5: // Random - note selection happens in operator()
        arpeggiate(1);
        break;
    }

    // Apply repeat: duplicate each step N times
    if(previous_repeat > 1)
    {
      decltype(arpeggio) repeated;
      repeated.reserve(arpeggio.size() * previous_repeat);
      for(auto& c : arpeggio)
      {
        for(int r = 0; r < previous_repeat; r++)
          repeated.push_back(c);
      }
      arpeggio = std::move(repeated);
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
        if(res >= 0.f && res <= 127.f)
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

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    // Store the current chord in a buffer
    auto& self = *this;
    auto& midi = this->inputs.midi;
    auto& out = this->outputs.midi;
    const auto& msgs = midi;
    const int octave = inputs.octave;
    const int octave_mode = inputs.octave_mode.value;
    const int repeat = inputs.repeat;
    const int arpeggio_mode = inputs.arpeggios.value;

    if(msgs.size() > 0)
    {
      // Update the "running" notes
      for(auto& note : msgs)
      {
        if(note.get_message_type() == libremidi::message_type::NOTE_ON)
        {
          self.notes.insert({note.bytes[1], note.bytes[2]});
        }
        else if(note.get_message_type() == libremidi::message_type::NOTE_OFF)
        {
          self.notes.erase(note.bytes[1]);
        }
      }
    }

    // Update the arpeggio itself
    const bool mustUpdateArpeggio = msgs.size() > 0 || octave != self.previous_octave
                                    || octave_mode != self.previous_octave_mode
                                    || repeat != self.previous_repeat
                                    || arpeggio_mode != self.previous_arpeggio;
    self.previous_octave = octave;
    self.previous_octave_mode = octave_mode;
    self.previous_repeat = repeat;
    self.previous_arpeggio = arpeggio_mode;

    if(mustUpdateArpeggio)
    {
      self.update();
    }

    if(self.arpeggio.empty())
    {
      for(int k = 0; k < 128; k++)
      {
        while(self.in_flight[k] > 0)
        {
          out.note_off(1, k, 0).timestamp = 0;
          self.in_flight[k]--;
        }
      }
      return;
    }

    if(self.index >= self.arpeggio.size())
      self.index = 0;

    // Play the next note / chord if we're on a quantification marker
    for(auto [date, q] :
        tk.get_quantification_date_with_bars(inputs.quantification.value))
    {
      if(date >= tk.frames)
        return;

      // Finish previous notes
      for(int k = 0; k < 128; k++)
      {
        while(self.in_flight[k] > 0)
        {
          out.note_off(1, k, 0).timestamp = date;
          self.in_flight[k]--;
        }
      }

      // Select the next index: random for Random mode, sequential otherwise
      std::size_t play_index;
      if(arpeggio_mode == 5) // Random
      {
        std::uniform_int_distribution<std::size_t> dist(0, self.arpeggio.size() - 1);
        play_index = dist(rng);
      }
      else
      {
        play_index = self.index;
        self.index = (self.index + 1) % self.arpeggio.size();
      }

      // Start the next note in the chord
      auto& chord = self.arpeggio[play_index];

      for(auto& note : chord)
      {
        self.in_flight[note.first]++;
        out.note_on(1, note.first, note.second).timestamp = date;
      }
    }
  }
};
}
}
