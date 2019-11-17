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
    static const constexpr auto uuid
        = make_uuid("0b98c7cd-f831-468f-81e3-706d6a97d705");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr midi_out midi_outs[]{"out"};
    static const constexpr auto controls = std::make_tuple(
        Control::FloatSlider("Spacing", 0, 8, 1),
        Control::FloatSlider("Tempo", 60, 300, 120),
        Control::IntSlider("Quantification", 1, 32, 8),
        Control::IntSlider("Duration", 1, 32, 16));
  };

  struct State
  {
    ossia::flat_set<int> notes;
    std::array<int, 127> notesMap;

    int lastnote{};
    int index{};
  };

  using control_policy = ossia::safe_nodes::precise_tick;
  static void
  run(const ossia::midi_port& midi,
      float spacing,
      float tempo,
      int quantif,
      int duration,
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
          self.notes.insert(note.bytes[1]);
        }
        else if (note.get_message_type() == rtmidi::message_type::NOTE_OFF)
        {
          self.notes.erase(note.bytes[1]);
        }
      }

      // Compute for all octaves
      for (std::size_t i = 0; i < self.notesMap.size(); ++i)
      {
        self.notesMap[i] = self.notes.container[i % self.notes.size()]
                           + std::floor(i / self.notes.size()) * 12;
      }
    }

    // We have to have a rhythmic cursor which outputs notes at a regular pace.
    // Check if we are in the interval for outputting a note.
    auto whole_samples = (240.f / tempo) * st.sampleRate();
    {
      auto period = whole_samples / quantif; // durée d'une note de l'arpège
      auto next
          = period
            * std::floor(
                  1 + tk.prev_date.impl * st.modelToSamples() / period); // date de la prochaine note qui
                                              // doit être envoyée
      if (next < tk.date.impl * st.modelToSamples())
      {
        out.messages.push_back(rtmidi::message::note_off(1, self.lastnote, 0));

        // Send a note: have an index that goes around the array of notes.
        // We are centered on the middle of the availble notes:
        // - spacing == 0 -> the middlest note (closest to 64)
        // - spacing == 1 -> ~1 octave
        // etc..

        int min_note = self.notesMap.size() / 2 - 3 * spacing;
        int max_note = self.notesMap.size() / 2 + 3 * spacing;

        self.index = (self.index + 1) % (max_note - min_note);

        auto newnote = self.notesMap[self.index];

        int velocity = 64; // todo take average instead
        out.messages.push_back(rtmidi::message::note_on(1, newnote, velocity));
        self.lastnote = newnote;
      }
      else
      {
        // durée max d'une note
        auto period = whole_samples / duration;
        auto next = period * (1 + tk.prev_date.impl * st.modelToSamples() / period);
        if (next < tk.date.impl * st.modelToSamples())
        {
          // date de la prochaine note qui doit être coupée

          out.messages.push_back(
              rtmidi::message::note_off(1, self.lastnote, 64));
        }
      }
    }
  }
};
}
}
