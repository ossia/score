#pragma once
#include <Engine/Node/PdNode.hpp>

#include <random>

namespace Nodes
{
namespace Quantifier
{
using Note = Control::Note;
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Quantifier";
    static const constexpr auto objectKey = "Quantifier";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::MidiEffect;
    static const constexpr auto description = "Quantifies a MIDI input";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("b8e2e5ad-17e4-43de-8d79-660a29d5c4f4");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr midi_out midi_outs[]{"out"};
    static const constexpr auto controls = std::make_tuple(
        Control::Widgets::QuantificationChooser(),
        Control::FloatSlider{"Tightness", 0.f, 1.f, 0.8f},
        Control::Widgets::DurationChooser(),
        Control::Widgets::TempoChooser());
  };

  struct NoteIn
  {
    Note note{};
    ossia::time_value date{};
  };
  struct State
  {
    std::vector<NoteIn> to_start;
    std::vector<NoteIn> running_notes;
  };

  using control_policy = ossia::safe_nodes::default_tick;

  static void
  run(const ossia::midi_port& p1,
      const ossia::safe_nodes::timed_vec<float>& startq,
      const ossia::safe_nodes::timed_vec<float>& tightness,
      const ossia::safe_nodes::timed_vec<float>& dur,
      const ossia::safe_nodes::timed_vec<float>& tempo_vec,
      ossia::midi_port& p2,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    auto start = startq.rbegin()->second;
    double precision = tightness.rbegin()->second;
    auto duration = dur.rbegin()->second;
    auto tempo = tempo_vec.rbegin()->second;

    // how much time does a whole note last at this tempo given the current sr
    const auto whole_dur = 240.f / tempo; // in seconds
    const auto whole_samples = whole_dur * st.sampleRate();

    for (const libremidi::message& in : p1.messages)
    {
      if (!in.is_note_on_or_off())
      {
        p2.messages.push_back(in);
        continue;
      }

      Note note{in[1], in[2], (uint8_t)in.get_channel()};

      if (in.get_message_type() == libremidi::message_type::NOTE_ON && note.vel != 0)
      {
        if (start == 0.f) // No quantification, start directly
        {
          auto no = libremidi::message::note_on(note.chan, note.pitch, note.vel);
          no.timestamp = in.timestamp;

          p2.messages.push_back(no);
          if (duration > 0.f)
          {
            auto end = tk.date + (int64_t)no.timestamp + (int64_t)(whole_samples * duration);
            self.running_notes.push_back({note, end});
          }
          else if (duration == 0.f)
          {
            // Stop at the next sample
            auto noff = libremidi::message::note_off(note.chan, note.pitch, note.vel);
            noff.timestamp = no.timestamp;
            p2.messages.push_back(noff);
          }
          // else do nothing and just wait for a note off
        }
        else
        {
          // Find next time that matches the requested quantification
          const auto start_q = whole_samples * start;
          auto perf_date
              = int64_t(start_q * int64_t(1 + tk.date.impl * st.modelToSamples() / start_q));
          int64_t actual_date
              = (1. - precision) * tk.date.impl * st.modelToSamples() + precision * perf_date;
          ossia::time_value next_date{actual_date};
          self.to_start.push_back({note, next_date});
        }
      }
      else
      {
        // Just stop
        auto noff = libremidi::message::note_off(note.chan, note.pitch, note.vel);
        noff.timestamp = in.timestamp;
        p2.messages.push_back(noff);
      }
    }

    // TODO : also handle the case where we're quite close from the *previous*
    // accessible value, eg we played a bit late
    for (auto it = self.to_start.begin(); it != self.to_start.end();)
    {
      auto& note = *it;
      if (note.date > tk.prev_date && note.date.impl < tk.date.impl)
      {
        auto no = libremidi::message::note_on(note.note.chan, note.note.pitch, note.note.vel);
        no.timestamp = tk.to_physical_time_in_tick(note.date, st.modelToSamples());
        p2.messages.push_back(no);

        if (duration > 0.f)
        {
          auto end = note.date + (int64_t)(whole_samples * duration);
          self.running_notes.push_back({note.note, end});
        }
        else if (duration == 0.f)
        {
          // Stop at the next sample
          auto noff = libremidi::message::note_off(note.note.chan, note.note.pitch, note.note.vel);
          noff.timestamp = no.timestamp;
          p2.messages.push_back(noff);
        }

        it = self.to_start.erase(it);
      }
      else
      {
        ++it;
      }
    }

    for (auto it = self.running_notes.begin(); it != self.running_notes.end();)
    {
      auto& note = *it;
      if (note.date > tk.prev_date && note.date.impl < tk.date.impl)
      {
        auto noff = libremidi::message::note_off(note.note.chan, note.note.pitch, note.note.vel);
        noff.timestamp = tk.to_physical_time_in_tick(note.date, st.modelToSamples());
        p2.messages.push_back(noff);
        it = self.running_notes.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
};
}
}
