#pragma once
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/math.hpp>

#include <boost/pfr.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

#include <iostream>
inline std::ostream&
operator<<(std::ostream& s, decltype(halp::tick_musical::signature) sig)
{
  return s << sig.num << " / " << sig.denom;
}
namespace Nodes
{
namespace PulseToNote
{
struct Node
{
  halp_meta(name, "Pulse to Midi")
  halp_meta(c_name, "VelToNote")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#pulse-to-note")
  halp_meta(
      description,
      "Converts a message into MIDI.\n"
      "If the input is an impulse, the output will be the default pitch "
      "at the default velocity.\n"
      "If the input is a single integer in [0; 127], the output will be "
      "the relevant note at the default velocity"
      "If the input is an array of two values between [0; 127], the "
      "output will be the relevant note.")

  halp_meta(uuid, "2c6493c3-5449-4e52-ae04-9aee3be5fb6a")

  struct
  {
    // FIXME is_event
    // FIXME all incorrect when token_request smaller than tick
    // Implement polyphonic behaviour at the avendish level?
    ossia_port<"in", ossia::value_port> port{};
    quant_selector<"Start quant."> start_quant;
    halp::hslider_f32<"Tightness", halp::range{0.f, 1.f, 0.8f}> tightness;
    quant_selector<"End quant."> end_quant;
    midi_spinbox<"Default pitch"> basenote;
    midi_spinbox<"Default vel."> basevel;
    octave_slider<"Pitch shift", -5, 5> shift_note;
    octave_slider<"Pitch random", 0, 2> note_random;
    octave_slider<"Vel. random", 0, 2> vel_random;
    midi_channel<"Channel"> channel;

  } inputs;
  struct
  {
    midi_out midi;
  } outputs;

  struct Note
  {
    uint8_t pitch{};
    uint8_t vel{};
    uint8_t chan{};
  };

  struct NoteIn
  {
    Note note{};
    int64_t date{};
  };
  std::vector<NoteIn> to_start;
  std::vector<NoteIn> running_notes;

  struct val_visitor
  {
    Node& st;
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
    Note operator()(int note, int vel) { return {(uint8_t)note, (uint8_t)vel}; }
    Note operator()(int note, float vel) { return {(uint8_t)note, (uint8_t)(vel * 127.f)}; }
    Note operator()(const std::vector<ossia::value>& v)
    {
      switch(v.size())
      {
        case 0:
          return operator()();
        case 1:
          return operator()(ossia::convert<int>(v[0]));
        case 2: {
          int note = ossia::convert<int>(v[0]);
          switch(v[1].get_type())
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
          return operator()(ossia::convert<int>(v[0]), ossia::convert<int>(v[1]));
      }
    }
    template <std::size_t N>
    Note operator()(const std::array<float, N>& v)
    {
      static_assert(N >= 2);
      return operator()(v[0], v[1]);
    }
  };

  static constexpr uint8_t midi_clamp(int num) { return (uint8_t)ossia::clamp(num, 0, 127); }

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
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

    // FIXME next version choose between these behaviours

#define STOP_AT_NEXT_SAMPLE_FOR_ZERO_END_QUANT 1

    const auto& start = inputs.start_quant.value;
    // TODO double precision = tightness.rbegin()->second;
    const auto& end = inputs.end_quant.value;
    const auto& shiftnote = inputs.shift_note.value;
    const auto& base_note = midi_clamp(inputs.basenote.value);
    const auto& base_vel = midi_clamp(inputs.basevel.value);
    const auto& rand_note = inputs.note_random.value;
    const auto& rand_vel = inputs.vel_random.value;
    const auto& chan = inputs.channel.value;

    for(auto& in : inputs.port.value->get_data())
    {
      auto note = in.value.apply(val_visitor{*this, base_note, base_vel});

      if(rand_note != 0)
        note.pitch += rnd::rand(-rand_note, rand_note);
      if(rand_vel != 0)
        note.vel += rnd::rand(-rand_vel, rand_vel);

      note.pitch = ossia::clamp((int)note.pitch + shiftnote, 0, 127);
      note.vel = ossia::clamp((int)note.vel, 0, 127);

      if(note.vel != 0)
      {
        if(start == 0.f) // No quantification, start directly
        {
          outputs.midi.note_on(chan, note.pitch, note.vel).timestamp = in.timestamp;
          if(end > 0.f)
          {
            this->running_notes.push_back({note, tk.position_in_frames + in.timestamp});
          }
          else if(end == 0.f)
          {
            // Stop at the next sample
#if STOP_AT_NEXT_SAMPLE_FOR_ZERO_END_QUANT
            outputs.midi.note_off(chan, note.pitch, note.vel).timestamp = in.timestamp;
#endif
          }
          // else do nothing and just wait for a note off
        }
        else
        {
          // Find next time that matches the requested quantification
          this->to_start.push_back({note, {}});
        }
      }
      else
      {
        // Just stop
        outputs.midi.note_off(chan, note.pitch, note.vel).timestamp = in.timestamp;
      }
    }
    std::optional<int> start_quant_date{};

    if(!to_start.empty())
    {
      if(start != 0.f)
      {
        for(auto [date, q] : tk.get_quantification_date(4. * start))
        {
          start_all_notes(tk.position_in_frames + date, date, chan, end);
          start_quant_date = date;
          break;
        }
      }
      else
      {
        start_all_notes(tk.position_in_frames, 0, chan, end);
        start_quant_date = 0;
      }
    }

    if(end != 0.f)
    {
      for(auto [date, q] : tk.get_quantification_date(4. * end))
      {
        if(!start_quant_date || date > start_quant_date)
        {
          stop_notes(date, chan);
        }
      }
    }
    else
    {
      // FIXME this means that the notes cannot run ? instead we should just rely on note off maybe? but it's an impulse...
      // we should do this for "impulses" and individual ints, but let the normal note off
#if STOP_AT_NEXT_SAMPLE_FOR_ZERO_END_QUANT
      stop_notes(0, chan);
#endif
    }
  }

  void start_all_notes(
      ossia::physical_time abs_date, ossia::physical_time date_phys, int chan,
      float endq) noexcept
  {
    for(auto& note : this->to_start)
    {
      outputs.midi.note_on(chan, note.note.pitch, note.note.vel).timestamp = date_phys;

      if(endq > 0.f)
      {
        this->running_notes.push_back({{note.note}, abs_date});
      }
      else if(endq == 0.f)
      {
        // Stop at the next sample

#if STOP_AT_NEXT_SAMPLE_FOR_ZERO_END_QUANT
        outputs.midi.note_off(chan, note.note.pitch, note.note.vel).timestamp
            = date_phys;
#endif
      }
    }
    this->to_start.clear();
  }

  void stop_notes(ossia::physical_time date_phys, int chan)
  {
    for(auto& note : this->running_notes)
    {
      outputs.midi.note_off(chan, note.note.pitch, note.note.vel).timestamp = date_phys;
    }
    this->running_notes.clear();
    /*
    for(auto it = this->running_notes.begin(); it != this->running_notes.end();)
    {
      auto& note = *it;
      // note.date is the date at which the note was started.

      if(note.date.impl > tk.date.impl)
      {
        // Note was started "in the future", stop it right now as it means  we went back in time
        p2.note_off(chan, note.note.pitch, note.note.vel).timestamp
            = tk.to_physical_time_in_tick(tk.prev_date, sampleRatio);
        it = this->running_notes.erase(it);
      }
      else if(note.date.impl < tk.prev_date.impl)
      {
        // if we're
        it = this->running_notes.erase(it);
      }
      else
      {
        ++it;
      }

      if(note.date > tk.prev_date && note.date.impl < tk.date.impl)
      {
        p2.note_off(chan, note.note.pitch, note.note.vel).timestamp
            = (note.date - tk.prev_date).impl * st.modelToSamples();

        it = this->running_notes.erase(it);
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
