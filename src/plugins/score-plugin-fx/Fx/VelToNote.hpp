#pragma once
#include <Fx/Quantifier.hpp>

#include <ossia/detail/math.hpp>

#include <Engine/Node/PdNode.hpp>

#include <rnd/random.hpp>
namespace Nodes
{
namespace PulseToNote
{
struct Node
{
  using Note = Control::Note;
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Pulse to Note";
    static const constexpr auto objectKey = "VelToNote";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Other;
    static const constexpr auto description
        = "Converts a message into MIDI.\n"
          "If the input is an impulse, the output will be the default pitch "
          "at the default velocity.\n"
          "If the input is a single integer in [0; 127], the output will be "
          "the relevant note at the default velocity"
          "If the input is an array of two values between [0; 127], the "
          "output will be the relevant note.";

    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("2c6493c3-5449-4e52-ae04-9aee3be5fb6a");

    static const constexpr value_in value_ins[]{{"in", true}};
    static const constexpr midi_out midi_outs[]{"out"};
    static const constexpr auto controls = std::make_tuple(
        Control::ComboBox<float, std::size(Control::Widgets::notes)>("Start quant.", 2, Control::Widgets::notes),
        Control::FloatSlider{"Tightness", 0.f, 1.f, 0.8f},
        Control::ComboBox<float, std::size(Control::Widgets::notes)>("End quant.", 2, Control::Widgets::notes),
        Control::Widgets::MidiSpinbox("Default pitch"),
        Control::Widgets::MidiSpinbox("Default vel."),
        Control::Widgets::OctaveSlider("Pitch shift", -5, 5),
        Control::Widgets::OctaveSlider("Pitch random", 0, 2),
        Control::Widgets::OctaveSlider("Vel. random", 0, 2),
        Control::Widgets::MidiChannel("Channel"));
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
  struct val_visitor
  {
    State& st;
    uint8_t base_note{};
    uint8_t base_vel{};

    Note operator()() { return {base_note, base_vel}; }
    Note operator()(ossia::impulse) { return {base_note, base_vel}; }
    template <typename T>
    Note operator()(const T&)
    {
      return {base_note, base_vel};
    }
    Note operator()(float note) { return {(uint8_t)note, base_vel}; }
    Note operator()(char note) { return {(uint8_t)note, base_vel}; }
    Note operator()(int note) { return {(uint8_t)note, base_vel}; }
    Note operator()(int note, int vel)
    {
      return {(uint8_t)note, (uint8_t)vel};
    }
    Note operator()(int note, float vel)
    {
      return {(uint8_t)note, (uint8_t)(vel * 127.f)};
    }
    Note operator()(const std::vector<ossia::value>& v)
    {
      switch (v.size())
      {
        case 0:
          return operator()();
        case 1:
          return operator()(ossia::convert<int>(v[0]));
        case 2:
        {
          int note = ossia::convert<int>(v[0]);
          switch (v[1].get_type())
          {
            case ossia::val_type::FLOAT:
              return operator()(note, *v[1].v.target<float>());
            case ossia::val_type::INT:
              return operator()(note, *v[1].v.target<int>());
            default:
              return operator()(note, ossia::convert<int>(v[1]));
          }
        }
        default:
          return operator()(
              ossia::convert<int>(v[0]), ossia::convert<int>(v[1]));
      }
    }
    template <std::size_t N>
    Note operator()(const std::array<float, N>& v)
    {
      static_assert(N >= 2);
      return operator()(v[0], v[1]);
    }
  };

  static constexpr uint8_t midi_clamp(int num)
  {
    return (uint8_t)ossia::clamp(num, 0, 127);
  }
  static void
  run(const ossia::value_port& p1,
      const ossia::safe_nodes::timed_vec<float>& startq,
      const ossia::safe_nodes::timed_vec<float>& tightness,
      const ossia::safe_nodes::timed_vec<float>& endq,
      const ossia::safe_nodes::timed_vec<int>& basenote,
      const ossia::safe_nodes::timed_vec<int>& basevel,
      const ossia::safe_nodes::timed_vec<int>& shift_note,
      const ossia::safe_nodes::timed_vec<int>& note_random,
      const ossia::safe_nodes::timed_vec<int>& vel_random,
      const ossia::safe_nodes::timed_vec<int>& chan_vec,
      ossia::midi_port& p2,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    // TODO : when arrays like [ 1, 25, 12, 37, 10, 40 ] are received
    // send relevant chords

    // When a message is received, we have three cases :
    // 1. Just an impulse: use base note & base vel
    // 2. Just an int: it's the velocity, use base note
    // 3. A tuple [int, int]: it's note and velocity

    // Then once we have a pair [int, int] we randomize and we output a note
    // on.

    // At the end, scan running_notes: if any is going to end in this buffer,
    // end it too.

    auto start = startq.rbegin()->second;
    double precision = tightness.rbegin()->second;
    auto end = endq.rbegin()->second;
    auto shiftnote = shift_note.rbegin()->second;
    auto base_note = midi_clamp(basenote.rbegin()->second);
    auto base_vel = midi_clamp(basevel.rbegin()->second);
    auto rand_note = note_random.rbegin()->second;
    auto rand_vel = vel_random.rbegin()->second;
    auto chan = chan_vec.rbegin()->second;

    const double sampleRatio = st.modelToSamples();
    for (auto& in : p1.get_data())
    {
      auto note = in.value.apply(val_visitor{self, base_note, base_vel});

      if (rand_note != 0)
        note.pitch
            += rnd::rand(-rand_note, rand_note);
      if (rand_vel != 0)
        note.vel += rnd::rand(-rand_vel, rand_vel);

      note.pitch = ossia::clamp((int)note.pitch + shiftnote, 0, 127);
      note.vel = ossia::clamp((int)note.vel, 0, 127);

      if (note.vel != 0)
      {
        if (start == 0.f) // No quantification, start directly
        {
          auto& no = p2.note_on(chan, note.pitch, note.vel);
          no.timestamp = in.timestamp;
          if (end > 0.f)
          {
            self.running_notes.push_back({note, tk.from_physical_time_in_tick(in.timestamp, sampleRatio)});
          }
          else if (end == 0.f)
          {
            // Stop at the next sample
            p2.note_off(chan, note.pitch, note.vel).timestamp = no.timestamp;
          }
          // else do nothing and just wait for a note off
        }
        else
        {
          // Find next time that matches the requested quantification
          self.to_start.push_back({note, ossia::time_value{}});
        }
      }
      else
      {
        // Just stop
        p2.note_off(chan, note.pitch, note.vel).timestamp = in.timestamp;
      }
    }

    if(start != 0.f)
    {
      if(auto date = tk.get_quantification_date(1. / start))
      {
        start_all_notes(*date, tk.to_physical_time_in_tick(*date, sampleRatio), chan, end, p2, self);
      }
    }
    else
    {
      start_all_notes(tk.prev_date, tk.to_physical_time_in_tick(tk.prev_date, sampleRatio), chan, end, p2, self);
    }

    if(end != 0.f)
    {
      if(auto date = tk.get_quantification_date(1. / end))
      {
        stop_notes(tk.to_physical_time_in_tick(*date, sampleRatio), chan, p2, self);
      }
    }
    else
    {
      stop_notes(tk.to_physical_time_in_tick(tk.prev_date, sampleRatio), chan, p2, self);
    }
  }

  static void start_all_notes(
      ossia::time_value date,
      ossia::physical_time date_phys,
      int chan,
      float endq,
      ossia::midi_port& p2,
      State& self) noexcept
  {
    for (auto& note : self.to_start)
    {
      p2.note_on(chan, note.note.pitch, note.note.vel).timestamp = date_phys;

      if (endq > 0.f)
      {
        self.running_notes.push_back({{note.note}, date});
      }
      else if (endq == 0.f)
      {
        // Stop at the next sample
        p2.note_off(chan, note.note.pitch, note.note.vel).timestamp = date_phys;
      }
    }
    self.to_start.clear();
  }

  static void stop_notes(
      ossia::physical_time date_phys,
      int chan,
      ossia::midi_port& p2,
      State& self)
  {
    for(auto& note : self.running_notes)
    {
      p2.note_off(chan, note.note.pitch, note.note.vel).timestamp = date_phys;
    }
    self.running_notes.clear();
/*
    for (auto it = self.running_notes.begin(); it != self.running_notes.end();)
    {
      auto& note = *it;
      // note.date is the date at which the note was started.

      if (note.date.impl > tk.date.impl)
      {
        // Note was started "in the future", stop it right now as it means we went back in time
        p2.note_off(chan, note.note.pitch, note.note.vel).timestamp = tk.to_physical_time_in_tick(tk.prev_date, sampleRatio);
        it = self.running_notes.erase(it);
      }
      else if(note.date.impl < tk.prev_date.impl)
      {
        // if we're
        it = self.running_notes.erase(it);
      }
      else
      {
        ++it;
      }

      if (note.date > tk.prev_date && note.date.impl < tk.date.impl)
      {
        p2.note_off(chan, note.note.pitch, note.note.vel)
          .timestamp = (note.date - tk.prev_date).impl * st.modelToSamples();

        it = self.running_notes.erase(it);
      }
      else
      {
        ++it;
      }
    }
*/
  }
};
}
}
