#pragma once
#include <Crousti/Attributes.hpp>
#include <Crousti/Widgets.hpp>
#include <ossia/network/dataspace/time.hpp>
#include <ossia/network/dataspace/gain.hpp>
#include <avnd/concepts/midi_port.hpp>
namespace examples
{
/**
 * This example exhibits a simple, monophonic synthesizer
 */
struct Synth
{
  meta_attribute(pretty_name, "My example synth");
  meta_attribute(script_name, synth_123);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "93eb0f78-3d97-4273-8a11-3df5714d66dc");

  struct {
    /** MIDI input: simply a list of timestamped messages.
     * Timestamp are in samples, 0 is the first sample.
     */
    struct aaa{
      meta_attribute(name, "In");
      ossia::value_vector<libremidi::message> midi_messages;
    } midi;
  } inputs;

  struct {
    struct {
      meta_attribute(name, "Out");
      meta_attribute(want_channels, 2);
      double** samples{};
    } audio;
  } outputs;

  int in_flight = 0;
  ossia::frequency last_note{};
  ossia::linear last_volume{};
  double phase = 0.;

  /** Simple monophonic synthesizer **/
  void operator()(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    // 1. Process the MIDI messages. We'll just play the latest note-on
    // in a not very sample-accurate way..
    for(auto& msg : inputs.midi.midi_messages)
    {
      // Let's ignore channels
      switch(msg.get_message_type()) {
        case libremidi::message_type::NOTE_ON:
          in_flight++;

          // Let's leverage the ossia unit conversion framework (adapted from Jamoma):
          // bytes is interpreted as a midi pitch and then converted to frequency.
          last_note = ossia::midi_pitch{msg.bytes[1]};

          // Store the velocity in linear gain
          last_volume = ossia::midigain{msg.bytes[2]};
          break;

        case libremidi::message_type::NOTE_OFF:
          in_flight--;
          break;
        default:
          break;
      }
    }

    // 2. Quit if we don't have any more note to play
    if(in_flight <= 0)
      return;

    // 3. Output some bleeps
    const auto [start, N] = st.timings(tk);
    double increment = ossia::two_pi * last_note.dataspace_value / double(st.sampleRate());
    auto& out = outputs.audio.samples;

    for (int64_t j = 0; j < N; j++)
    {
      out[0][j] = last_volume.dataspace_value * std::sin(phase);
      out[1][j] = out[0][j];

      phase += increment;
    }
  }
};
}
