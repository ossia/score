#pragma once
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
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
    halp::combo_pair<int> values[5]{{"Forward", 0}, {"Backward", 1}, {"F->B", 2}, {"B->F", 3}, {"Chord", 4}};
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
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#arpeggiator")
  halp_meta(description, "Arpeggiator")
  halp_meta(uuid, "0b98c7cd-f831-468f-81e3-706d6a97d705")

  // FIXME "note" bus instead of midi bus ; the host handles passing all the non note messages
  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    Arpeggios arpeggios;
    halp::hslider_i32<"Octave", halp::irange{1, 7, 1}> octave;
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

  float previous_octave{};
  int previous_arpeggio{};
  std::size_t index{};

  void update()
  {
    // Create the content of the arpeggio
    switch(previous_arpeggio)
    {
      case 0:
        arpeggiate(1);
        break;
      case 1:
        arpeggiate(1);
        std::reverse(arpeggio.begin(), arpeggio.end());
        break;
      case 2:
        arpeggiate(2);
        duplicate_vector(arpeggio);
        std::reverse(arpeggio.begin() + notes.size(), arpeggio.end());
        break;
      case 3:
        arpeggiate(2);
        duplicate_vector(arpeggio);
        std::reverse(arpeggio.begin(), arpeggio.begin() + notes.size());
        break;
      case 4:
        arpeggio.clear();
        arpeggio.resize(1);
        for(std::pair note : notes)
        {
          arpeggio[0].push_back(note);
        }
        break;
    }

    const std::size_t orig_size = arpeggio.size();

    // Create the octave duplicates
    for(int i = 1; i < previous_octave; i++)
    {
      octavize(orig_size, i);
    }
    for(int i = 1; i < previous_octave; i++)
    {
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
    const int arpeggio = inputs.arpeggios.value;
    self.previous_octave = octave;
    self.previous_arpeggio = arpeggio;

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
    const bool mustUpdateArpeggio = msgs.size() > 0 || octave != self.previous_octave || arpeggio != self.previous_arpeggio;
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
    // FIXME use the one
    for(auto [date, q] : tk.get_quantification_date_with_bars(inputs.quantification.value))
    {
      // SCORE_SOFT_ASSERT(date >= 0);
      // SCORE_SOFT_ASSERT(date < tk.frames);

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

      // Start the next note in the chord
      auto& chord = self.arpeggio[self.index];

      for(auto& note : chord)
      {
        self.in_flight[note.first]++;
        out.note_on(1, note.first, note.second).timestamp = date;
      }

      // New chord to stop
      self.index = (self.index + 1) % (self.arpeggio.size());
    }
  }
};
}
}
