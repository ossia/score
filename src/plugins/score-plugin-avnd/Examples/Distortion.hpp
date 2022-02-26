#pragma once
#include <Crousti/Attributes.hpp>
#include <Crousti/Concepts.hpp>
#include <Crousti/Widgets.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <cmath>


namespace examples
{
struct Distortion
{
  /**
   * An audio effect plug-in must provide some metadata: name, author, etc.
   * UUIDs are important to uniquely identify plug-ins: you can use uuidgen for instance.
   */
  meta_attribute(pretty_name, "My pretty distortion");
  meta_attribute(script_name, disto_123);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "b9237d2a-1651-4dcc-920b-80e5e619c6c4");

  /** We define the input ports of our process: in this case,
   *  there's an audio input, a gain slider.
   */
  struct {
    avnd::audio_input_bus<"Input"> audio;
    avnd::hslider_f32<"Gain", avnd::range{.min = 0.f, .max = 100.f, .init = 10.f}> gain;
  } inputs;

  /** And the output ports: only an audio output on this one.
   * We force stereo output no matter how many inputs there are.
   */
  struct {
    avnd::fixed_audio_bus<"Output", double, 2> audio;
  } outputs;

  /** In this buffer, as an example we will store a mono signal. **/
  ossia::audio_channel temp_buffer;

  /** Will be called upon creation, and whenever the buffer size / sample rate changes **/
  void prepare(avnd::setup info)
  {
    /*
    // Reserve memory for two channels, for the input and the output.
    inputs.audio.samples.reserve(2, st.bufferSize());
    outputs.audio.samples.reserve(2, st.bufferSize());

    */

    // Also reserve some memory in our temporary mono buffer.
    temp_buffer.reserve(info.frames);

    // Note that channels in score are dynamic ; your process should not expect
    // a fixed number of channels, even across a single execution: it could be applied to 2 channels
    // at the start of a score, and 5 channels at the end.

    // It is still a work-in-progress to define a real-time-safe way to support this dynamic behaviour;
    // it is however guaranteed that there won't be other allocations during execution if
    // enough space had been reserved.
  }

  /**
   * Our actual function for transforming the inputs into outputs.
   *
   * Note that when using multichannel_audio_view and multichannel_audio,
   * there's no guarantee on the size of the data: on a token_request for 64 samples,
   * there may be for instance only 23 samples of 2-channel input,
   * while the output of the node could be 10 channels.
   *
   * It is up to the author to set-up the output according to its wishes for the plug-in.
   **/
  void operator()(int64_t N)
  {
    auto& gain = inputs.gain;

    // How many input channels
    const auto chans = inputs.audio.channels;

    // First sum all the input channels into a mono buffer
    temp_buffer.clear();
    for (int i = 0; i < chans; i++)
    {
      // The buffers are accessed through spans.
      auto in = inputs.audio.channel(i, N);

      // Filled with zeros
      // temp_buffer.resize(std::max(temp_buffer.size(), in.size()));

      // Sum samples from the input buffer
      const int64_t samples_to_read = N; //std::min(N, int64_t(in.size()));
      for(int64_t j = 0; j < samples_to_read; j++)
      {
        temp_buffer[j] += in[j];
      }
    }

    // Then output a stereo buffer of that with distortion, and reversed phase.
    // We fix the output channels to 2 for this example.
    //const auto output_chans = 2;

    // We could have made things a bit simpler by just resizing our temp_buffer to N
    // and using that for the output ; this may save a few samples of silence though.
    const int64_t samples_to_write = std::min(N, int64_t(temp_buffer.size()));

    //output.resize(output_chans, samples_to_write);

    // Write to the channels once they are allocated:
    auto out_l = outputs.audio.channel(0, N);
    auto out_r = outputs.audio.channel(1, N);

    for(int64_t j = 0; j < samples_to_write; j++)
    {
      // Tanh
      out_l[j] = std::tanh(temp_buffer[j] * gain.value);

      // Bitcrush
      out_l[j] = int(out_l[j] * 4.) / 4.;

      // Invert phase on the right side
      out_r[j] = 1. - out_l[j];
    }
  }
};
}
