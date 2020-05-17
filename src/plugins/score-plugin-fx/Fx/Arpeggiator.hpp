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

    chord previous_chord;
    int index{};
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
    if (msgs.size() > 0)
    {
      for (auto& note : msgs)
      {
        if (note.get_message_type() == rtmidi::message_type::NOTE_ON)
        {
          self.notes.insert({note.bytes[1], note.bytes[2]});
        }
        else if (note.get_message_type() == rtmidi::message_type::NOTE_OFF)
        {
          self.notes.erase(note.bytes[1]);
        }
      }

      auto notes_to_arpeggio = [&](int size_mult) {
        self.arpeggio.clear();
        self.arpeggio.reserve(self.notes.container.size() * size_mult);
        for (std::pair note : self.notes.container)
        {
          self.arpeggio.push_back(chord{note});
        }
      };

      switch (arpeggio)
      {
        case 0:
          notes_to_arpeggio(1);
          break;
        case 1:
          notes_to_arpeggio(1);
          std::reverse(self.arpeggio.begin(), self.arpeggio.end());
          break;
        case 2:
          notes_to_arpeggio(2);
          self.arpeggio.insert(self.arpeggio.end(), self.arpeggio.begin(), self.arpeggio.end());
          std::reverse(self.arpeggio.begin() + self.notes.container.size(), self.arpeggio.end());
          break;
        case 3:
          notes_to_arpeggio(2);
          self.arpeggio.insert(self.arpeggio.end(), self.arpeggio.begin(), self.arpeggio.end());
          std::reverse(self.arpeggio.begin(), self.arpeggio.begin() + self.notes.container.size());
          break;
        case 4:
          self.arpeggio.clear();
          self.arpeggio.resize(1);
          for (std::pair note : self.notes.container)
          {
            self.arpeggio[0].push_back(note);
          }
          break;
      }

      std::size_t orig_size = self.arpeggio.size();

      for (int i = 1; i < octave; i++)
      {
        for (std::size_t j = 0; j < orig_size; j++)
        {
          self.arpeggio.push_back(self.arpeggio[j]);
          for (std::pair<byte, byte>& note : self.arpeggio.back())
            note.first *= octave;
        }
      }
    }

    if (self.arpeggio.empty())
    {
      if (!self.previous_chord.empty())
      {
        for (auto& note : self.previous_chord)
          out.messages.push_back(rtmidi::message::note_off(1, note.first, 0));
        self.previous_chord.clear();
      }

      return;
    }

    if (self.index > self.arpeggio.size())
      self.index = 0;

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
        out.messages.push_back(rtmidi::message::note_on(1, note.first, note.second));
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
