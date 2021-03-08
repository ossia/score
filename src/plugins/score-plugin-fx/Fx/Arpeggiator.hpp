#pragma once
#include <ossia/detail/flat_map.hpp>

#include <Engine/Node/PdNode.hpp>

namespace Nodes
{
namespace Arpeggiator
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Arpeggiator";
    static const constexpr auto objectKey = "Arpeggiator";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::MidiEffect;
    static const constexpr auto description = "Arpeggiator";
    static const uuid_constexpr auto uuid = make_uuid("0b98c7cd-f831-468f-81e3-706d6a97d705");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr midi_out midi_outs[]{"out"};
    static const constexpr auto controls = std::make_tuple(
        Control::Widgets::ArpeggioChooser(),
        Control::IntSlider("Octave", 1, 7, 1),
        Control::IntSlider("Quantification", 1, 32, 8));
  };

  using byte = unsigned char;
  using chord = ossia::small_vector<std::pair<byte, byte>, 5>;

  struct State
  {
    ossia::flat_map<byte, byte> notes;
    ossia::small_vector<chord, 10> arpeggio;

    float previous_octave{};
    int previous_arpeggio{};
    chord previous_chord;
    std::size_t index{};

    void update()
    {
      // Create the content of the arpeggio
      switch (previous_arpeggio)
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
          arpeggio.insert(arpeggio.end(), arpeggio.begin(), arpeggio.end());
          std::reverse(arpeggio.begin() + notes.container.size(), arpeggio.end());
          break;
        case 3:
          arpeggiate(2);
          arpeggio.insert(arpeggio.end(), arpeggio.begin(), arpeggio.end());
          std::reverse(arpeggio.begin(), arpeggio.begin() + notes.container.size());
          break;
        case 4:
          arpeggio.clear();
          arpeggio.resize(1);
          for (std::pair note : notes.container)
          {
            arpeggio[0].push_back(note);
          }
          break;
      }

      const std::size_t orig_size = arpeggio.size();

      // Create the octave duplicates
      for (int i = 1; i < previous_octave; i++)
      {
        octavize(orig_size, i);
      }
      for (int i = 1; i < previous_octave; i++)
      {
        octavize(orig_size, -i);
      }
    }

    void arpeggiate(int size_mult)
    {
      arpeggio.clear();
      arpeggio.reserve(notes.container.size() * size_mult);
      for (std::pair note : notes.container)
      {
        arpeggio.push_back(chord{note});
      }
    }

    void octavize(std::size_t orig_size, int i)
    {
      for (std::size_t j = 0; j < orig_size; j++)
      {
        auto copy = arpeggio[j];
        for (auto it = copy.begin(); it != copy.end(); )
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
  };

  using control_policy = ossia::safe_nodes::precise_tick;
  static void
  run(const ossia::midi_port& midi,
      int arpeggio,
      float octave,
      int quantif,
      ossia::midi_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    // Store the current chord in a buffer
    auto msgs = midi.messages;
    self.previous_octave = octave;
    self.previous_arpeggio = arpeggio;

    if (msgs.size() > 0)
    {
      // Update the "running" notes
      for (auto& note : msgs)
      {
        if (note.get_message_type() == libremidi::message_type::NOTE_ON)
        {
          self.notes.insert({note.bytes[1], note.bytes[2]});
        }
        else if (note.get_message_type() == libremidi::message_type::NOTE_OFF)
        {
          self.notes.erase(note.bytes[1]);
        }
      }
    }

    // Update the arpeggio itself
    const bool mustUpdateArpeggio =
        msgs.size() > 0 ||
        octave != self.previous_octave ||
        arpeggio != self.previous_arpeggio;
    if(mustUpdateArpeggio)
    {
      self.update();
    }

    if (self.arpeggio.empty())
    {
      if (!self.previous_chord.empty())
      {
        for (auto& note : self.previous_chord)
          out.messages.push_back(libremidi::message::note_off(1, note.first, 0));
        self.previous_chord.clear();
      }

      return;
    }

    if (self.index >= self.arpeggio.size())
      self.index = 0;

    // Play the next note / chord if we're on a quantification marker
    if (auto date = tk.get_physical_quantification_date(quantif, st.modelToSamples()))
    {
      // Finish previous notes
      for (auto& note : self.previous_chord)
      {
        out.note_off(1, note.first, 0).timestamp = *date;
      }
      self.previous_chord.clear();

      // Start the next note in the chord
      auto& chord = self.arpeggio[self.index];

      for (auto& note : chord)
      {
        out.messages.push_back(libremidi::message::note_on(1, note.first, note.second));
        out.messages.back().timestamp = *date;
      }

      // New chord to stop
      self.previous_chord = chord;

      self.index = (self.index + 1) % (self.arpeggio.size());
    }
  }
};
}
}
