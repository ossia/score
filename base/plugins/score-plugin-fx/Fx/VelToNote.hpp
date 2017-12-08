#pragma once
#include <Engine/Node/PdNode.hpp>
#include <random>
namespace Nodes
{
namespace PulseToNote
{
struct ratio
{
  int num{};
  int denom{};

  friend constexpr ratio operator+(const ratio& lhs, const ratio& rhs)
  {
    return {
      lhs.num * rhs.denom + rhs.num * lhs.denom,
               lhs.denom * rhs.denom
    };
  }
};

struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "Pulse to Note";
    static const constexpr auto objectKey = "VelToNote";
    static const constexpr auto category = "Midi";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("2c6493c3-5449-4e52-ae04-9aee3be5fb6a");
  };

  struct Note { int pitch{}; int vel{}; };
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
      Process::create_node()
      .value_ins({{"in", true}})
      .midi_outs({{"out"}})
      .controls(Process::Widgets::QuantificationChooser(),
                Process::Widgets::DurationChooser(),
                Process::Widgets::MidiSpinbox("Default pitch"),
                Process::Widgets::MidiSpinbox("Default vel."),
                Process::Widgets::OctaveSlider("Pitch random", 0, 2),
                Process::Widgets::OctaveSlider("Vel. random", 0, 2)
                )
      .state<State>()
      .build();
  using control_policy = Process::DefaultTick;
  struct val_visitor
  {
    State& st;
    int base_note{};
    int base_vel{};

    template<typename... T>
    Note operator()(T&&... v)
    {
      return {base_note, base_vel};
    }
    Note operator()(int vel)
    {
      return {base_note, vel};
    }
    Note operator()(int note, int vel)
    {
      return {note, vel};
    }
    Note operator()(const std::vector<ossia::value>& v)
    {
      switch(v.size())
      {
        case 0: return operator()();
        case 1: return operator()(ossia::convert<int>(v[0]));
        default: return operator()(ossia::convert<int>(v[0]), ossia::convert<int>(v[1]));
      }
    }
    template<std::size_t N>
    Note operator()(const std::array<float, N>& v)
    {
      return operator()(v[0], v[1]);
    }
  };
  
  static void run(
      const ossia::value_port& p1,
      const Process::timed_vec<float>& startq,
      const Process::timed_vec<float>& dur,
      const Process::timed_vec<int>& basenote,
      const Process::timed_vec<int>& basevel,
      const Process::timed_vec<int>& note_random,
      const Process::timed_vec<int>& vel_random,
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
    auto base_note = basenote.rbegin()->second;
    auto base_vel = basevel.rbegin()->second;
    auto rand_note = note_random.rbegin()->second;
    auto rand_vel = vel_random.rbegin()->second;

    // Get tempo
    double tempo = 120.;
    if(auto tempo_node = st.find_node("/tempo"))
      tempo = ossia::convert<float>(tempo_node->get_parameter()->value());

    // how much time does a whole note last at this tempo given the current sr
    const auto whole_dur = 240. / tempo; // in seconds
    const auto whole_samples = whole_dur * st.sampleRate;

    for(auto& in : p1.get_data())
    {
      auto note = in.value.apply(val_visitor{self, base_note, base_vel});

      if(rand_note != 0)
        note.pitch += std::uniform_int_distribution<int>(-rand_note, rand_note)(m);
      if(rand_vel != 0)
        note.vel += std::uniform_int_distribution<int>(-rand_vel, rand_vel)(m);

      note.pitch = ossia::clamp(note.pitch, 0, 127);
      note.vel = ossia::clamp(note.vel, 0, 127);

      if(start == 0.)  // No quantification, start directly
      {
        auto no = mm::MakeNoteOn(1, note.pitch, note.vel);
        no.timestamp = in.timestamp;

        p2.messages.push_back(no);
        if(duration != 0.)
        {
          auto end = tk.date + (int64_t)no.timestamp + whole_samples * duration;
          self.running_notes.push_back({note, end});
        }
        else
        {
          // Stop at the next sample
          auto noff = mm::MakeNoteOff(1, note.pitch, note.vel);
          noff.timestamp = no.timestamp + 1;
          p2.messages.push_back(noff);
        }
      }
      else
      {
        // Find next time that matches the requested quantification
        const auto start_q = whole_samples * start;
        ossia::time_value next_date{int64_t(start_q * (1 + tk.date / start_q))};
        self.to_start.push_back({note, next_date});
      }
    }


    for(auto it = self.to_start.begin(); it != self.to_start.end(); )
    {
      auto& note = *it;
      if(note.date > prev_date && note.date < tk.date)
      {
        auto no = mm::MakeNoteOn(1, note.note.pitch, note.note.vel);
        no.timestamp = note.date - prev_date;
        p2.messages.push_back(no);

        if(duration != 0.)
        {
          auto end = note.date + whole_samples * duration;
          self.running_notes.push_back({note.note, end});
        }
        else
        {
          // Stop at the next sample
          auto noff = mm::MakeNoteOff(1, note.note.pitch, note.note.vel);
          noff.timestamp = no.timestamp + 1;
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
        auto noff = mm::MakeNoteOff(1, note.note.pitch, note.note.vel);
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

using Factories = Process::Factories<Node>;
}

}
