#pragma once
#include <Engine/Node/PdNode.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <random>
namespace Nodes
{
namespace Quantifier
{
struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "Quantifier";
    static const constexpr auto objectKey = "Quantifier";
    static const constexpr auto category = "Midi";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("b8e2e5ad-17e4-43de-8d79-660a29d5c4f4");
  };

  struct Note { uint8_t pitch{}; uint8_t vel{}; uint8_t chan{}; };
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

  static const constexpr auto info =
      Control::create_node()
      .midi_ins({{"in"}})
      .midi_outs({{"out"}})
      .controls(Control::Widgets::QuantificationChooser(),
                Control::Widgets::DurationChooser(),
                Control::Widgets::TempoChooser()
                )
      .build();
  using control_policy = Control::DefaultTick;

  static void run(
      const ossia::midi_port& p1,
      const Control::timed_vec<float>& startq,
      const Control::timed_vec<float>& dur,
      const Control::timed_vec<float>& tempo_vec,
      ossia::midi_port& p2,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state& st,
      State& self)
  {
    static std::mt19937 m;

    // When a message is received, we have three cases :
    // 1. Just an impulse: use base note & base vel
    // 2. Just an int: it's the velocity, use base note
    // 3. A tuple [int, int]: it's note and velocity

    // Then once we have a pair [int, int] we randomize and we output a note on.

    // At the end, scan running_notes: if any is going to end in this buffer, end it too.

    auto start = startq.rbegin()->second;
    auto duration = dur.rbegin()->second;
    auto tempo = tempo_vec.rbegin()->second;


    // how much time does a whole note last at this tempo given the current sr
    const auto whole_dur = 240.f / tempo; // in seconds
    const auto whole_samples = whole_dur * st.sampleRate;

    for(const mm::MidiMessage& in : p1.messages)
    {
      if(!in.isNoteOnOrOff())
      {
        p2.messages.push_back(in);
        continue;
      }

      Note note{in[1], in[2], (uint8_t)in.getChannel()};

      if(note.vel != 0)
      {
        if(start == 0.f) // No quantification, start directly
        {
          auto no = mm::MakeNoteOn(note.chan, note.pitch, note.vel);
          no.timestamp = in.timestamp;

          p2.messages.push_back(no);
          if(duration > 0.f)
          {
            auto end = tk.date + (int64_t)no.timestamp + (int64_t)(whole_samples * duration);
            self.running_notes.push_back({note, end});
          }
          else if(duration == 0.f)
          {
            // Stop at the next sample
            auto noff = mm::MakeNoteOff(note.chan, note.pitch, note.vel);
            noff.timestamp = no.timestamp;
            p2.messages.push_back(noff);
          }
          // else do nothing and just wait for a note off
        }
        else
        {
          // Find next time that matches the requested quantification
          const auto start_q = whole_samples * start;
          ossia::time_value next_date{int64_t(start_q * int64_t(1 + tk.date / start_q))};
          self.to_start.push_back({note, next_date});
        }
      }
      else
      {
        // Just stop
        auto noff = mm::MakeNoteOff(note.chan, note.pitch, note.vel);
        noff.timestamp = in.timestamp;
        p2.messages.push_back(noff);
      }
    }

    for(auto it = self.to_start.begin(); it != self.to_start.end(); )
    {
      auto& note = *it;
      if(note.date > prev_date && note.date < tk.date)
      {
        auto no = mm::MakeNoteOn(note.note.chan, note.note.pitch, note.note.vel);
        no.timestamp = note.date - prev_date;
        p2.messages.push_back(no);

        if(duration > 0.f)
        {
          auto end = note.date + (int64_t)(whole_samples * duration);
          self.running_notes.push_back({note.note, end});
        }
        else if (duration == 0.f)
        {
          // Stop at the next sample
          auto noff = mm::MakeNoteOff(note.note.chan, note.note.pitch, note.note.vel);
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

    for(auto it = self.running_notes.begin(); it != self.running_notes.end(); )
    {
      auto& note = *it;
      if(note.date > prev_date && note.date < tk.date)
      {
        auto noff = mm::MakeNoteOff(note.note.chan, note.note.pitch, note.note.vel);
        noff.timestamp = note.date - prev_date;
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
