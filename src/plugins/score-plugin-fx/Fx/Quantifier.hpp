#pragma once
#include <Fx/Types.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes
{
namespace Quantifier
{
struct Node
{
  halp_meta(name, "Midi quantify")
  halp_meta(c_name, "Quantifier")
  halp_meta(category, "Midi")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#quantifier")
  halp_meta(author, "ossia score")
  halp_meta(description, "Quantifies a MIDI input")
  halp_meta(uuid, "b8e2e5ad-17e4-43de-8d79-660a29d5c4f4")

  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    quant_selector<"Quantization"> start_quant;
    halp::hslider_f32<"Tightness", halp::range{0.f, 1.f, 0.8f}> tightness;
    duration_selector<"Duration"> duration;
    halp::hslider_f32<"Tempo", halp::range{20., 300., 120.}> tempo;

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

  // using control_policy = ossia::safe_nodes::default_tick_controls;
  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  using tick = halp::tick_flicks;
  void sequence(Note new_note, int64_t date)
  {
    for(auto note : to_start)
      if(note.note.pitch == new_note.pitch)
        return;
    for(auto note : running_notes)
      if(note.note.pitch == new_note.pitch)
        return;
    to_start.push_back(NoteIn{new_note, date});
  }
  void operator()(const tick& tk)
  {
    double start_q_ratio = inputs.start_quant.value;
    double precision = inputs.tightness.value;
    double duration = inputs.duration.value;
    double tempo = inputs.tempo.value;

    // how much time does a whole note last at this tempo given the current sr
    const auto whole_dur = 240.f / tempo; // in seconds
    const auto whole_samples = whole_dur * setup.rate;

    for(const libremidi::message& in : inputs.midi)
    {
      // FIXME note processor
      if(!in.is_note_on_or_off())
      {
        outputs.midi.push_back(in);
        continue;
      }

      Note note{in[1], in[2], (uint8_t)in.get_channel()};

      if(in.get_message_type() == libremidi::message_type::NOTE_ON && note.vel != 0)
      {
        if(start_q_ratio == 0.f) // No quantification, start directly
        {
          auto no = libremidi::channel_events::note_on(note.chan, note.pitch, note.vel);
          no.timestamp = in.timestamp;

          outputs.midi.push_back(no);
          if(duration > 0.f)
          {
            auto end = tk.position_in_frames + (int64_t)no.timestamp
                       + (int64_t)(whole_samples * duration);
            this->running_notes.push_back({note, end});
          }
          else if(duration == 0.f)
          {
            // Stop at the next sample
            auto noff = libremidi::channel_events::note_off(note.chan, note.pitch, note.vel);
            noff.timestamp = no.timestamp;
            outputs.midi.push_back(noff);
          }
          // else do nothing and just wait for a note off
        }
        else
        {
          // Find next time that matches the requested quantification
          const auto start_q = whole_samples * start_q_ratio;
          auto perf_date = int64_t(
              start_q * int64_t(1 + (tk.position_in_frames + in.timestamp) / start_q));

          //FIXME (1. - precision) * tk.date.impl * st.modelToSamples() + precision * perf_date;
          int64_t actual_date = perf_date;

          sequence(note, actual_date);
        }
      }
      else
      {
        // Just stop
        auto noff = libremidi::channel_events::note_off(note.chan, note.pitch, note.vel);
        noff.timestamp = in.timestamp;
        outputs.midi.push_back(noff);

        for(auto it = running_notes.begin(); it != running_notes.end();)
          if(it->note.pitch == note.pitch)
            it = running_notes.erase(it);
          else
            ++it;
      }
    }

    // TODO : also handle the case where we're quite close from the *previous*
    // accessible value, eg we played a bit late
    for(auto it = this->to_start.begin(); it != this->to_start.end();)
    {
      auto& note = *it;
      // FIXME how does this follow live tempo changes ?
      if(note.date >= tk.position_in_frames
         && note.date < (tk.position_in_frames + tk.frames))
      {
        auto no = libremidi::channel_events::note_on(
            note.note.chan, note.note.pitch, note.note.vel);
        no.timestamp = note.date - tk.position_in_frames;
        outputs.midi.push_back(no);

        if(duration > 0.f)
        {
          auto end = note.date + (int64_t)(whole_samples * duration);
          this->running_notes.push_back({note.note, end});
        }
        else if(duration == 0.f)
        {
          // Stop at the next sample
          auto noff = libremidi::channel_events::note_off(note.note.chan, note.note.pitch, note.note.vel);
          noff.timestamp = no.timestamp;
          outputs.midi.push_back(noff);
        }

        it = this->to_start.erase(it);
      }
      else
      {
        ++it;
      }
    }

    for(auto it = this->running_notes.begin(); it != this->running_notes.end();)
    {
      auto& note = *it;
      if(note.date >= tk.position_in_frames
         && note.date < (tk.position_in_frames + tk.frames))
      {
        auto noff = libremidi::channel_events::note_off(note.note.chan, note.note.pitch, note.note.vel);
        noff.timestamp = note.date - tk.position_in_frames;
        outputs.midi.push_back(noff);
        it = this->running_notes.erase(it);
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
