#pragma once
#include <Crousti/Concepts.hpp>
#include <Crousti/Attributes.hpp>

namespace examples
{
struct RawPortsExample
{
  meta_attribute(pretty_name, "Raw ports example");
  meta_attribute(script_name, raw_ports_test_plugin);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "be958cc2-0545-4a90-bf45-c73db0845d9c");

  /**
   * This example uses the raw inputs and outputs ports of ossia score.
   * This is the most efficient and powerful way: no copy of any data occurs.
   * But it requires more work.
   */
  struct {
    struct {
      meta_attribute(name, "Audio In");
      const ossia::audio_port* port{};
    } audio;

    struct {
      meta_attribute(name, "Value In");
      const ossia::value_port* port{};
    } value;

    struct {
      meta_attribute(name, "MIDI In");
      const ossia::midi_port* port{};
    } midi;

  } inputs;

  struct {
    struct {
      meta_attribute(name, "Audio Out");
      ossia::audio_port* port{};
    } audio;

    struct {
      meta_attribute(name, "Value Out");
      ossia::value_port* port{};
    } value;

    struct {
      meta_attribute(name, "MIDI Out");
      ossia::midi_port* port{};
    } midi;

  } outputs;


   /**
   * In this case, it is entirely up to the author to handle timing properly.
   * In particular: the audio buffers, are the raw buffers which aren't offset by  * the date at which audio processing is supposed to start;
   * the "actual" data that the node has to process is contained in the samples given by:
   *
   *   auto [start, N] = st.timings(t);
   *
   * e.g. for instance, the audio buffers go from 0 to 512 ; but the token request (indicating timing information)
   * may request this process to fill the samples 17 to 180, in order to match the temporal structure of the score.
   */
  void operator()(
      const ossia::token_request& t,
      ossia::exec_state_facade st
  )
  {
    // Invalid implementation of passthrough copy: it may copy at best useless and at worst garbage data before/after,
    // as well as overwrite useful data if the process is called multiple times for the same buffers (which will happen).

    // outputs.audio.port->samples = inputs.audio.port->samples;
    // outputs.value.port->get_data() = inputs.value.port->get_data();
    // outputs.midi.port->messages = inputs.midi.port->messages;

    // The valid implementation is a bit involved - if you use the simpler types this is done behind the scene.
    auto [start, N] = st.timings(t);

    // Copy audio input
    if(const auto channels = inputs.audio.port->channels(); channels > 0)
    {
      outputs.audio.port->set_channels(channels);
      for(std::size_t c = 0; c < channels; ++c)
      {
        auto& in = inputs.audio.port->channel(c);
        auto& out = outputs.audio.port->channel(c);
        const int64_t in_samples = in.size();
        const int64_t out_samples = out.size();

        // score guarantees that the input is filled to start + N at least.
        SCORE_ASSERT(in_samples >= start + N);

        if(out_samples < start + N)
          out.resize(start + N);

        std::copy_n(in.begin() + start, N, out.begin() + start);
      }
    }

    // Copy messages
    // timestamp is in samples, with the same referential than audio data.
    for(const auto& [value, timestamp]: inputs.value.port->get_data())
    {
      if(timestamp >= start && timestamp < start + N)
      {
        // Most of the time you are not supposed to *write* into get_data():
        // outputs.value.port->get_data().push_back(mess);

        // Instead, do this:
        outputs.value.port->write_value(value, timestamp);

        // This is because this function will do intelligent conversion (units, domain, etc) whenever possible.
      }
    }

    // Copy MIDI messages
    // timestamp is in samples, with the same referential than audio data.
    for(const auto& mess: inputs.midi.port->messages)
    {
      if(mess.timestamp >= start && mess.timestamp < start + N)
      {
        outputs.midi.port->messages.push_back(mess);
      }
    }
  }
};
}
