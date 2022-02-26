#pragma once
#include <Crousti/Attributes.hpp>
#include <Crousti/Widgets.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/controls.hpp>
namespace examples
{
/**
 * This example exhibits a simple multi-channel effect processor.
 */
struct AudioEffectExample
{
  meta_attribute(pretty_name, "My example effect");
  meta_attribute(script_name, effect_123);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "c8b57fff-c34c-4772-8f72-fe5267527ece");

  /**
   * Here we have a special case, which happens to be the most common case in audio
   * development. If our inputs start with an audio port of the shape
   *
   *     const double** samples;
   *     std::size_t channels;
   *
   * and our outputs starts with an audio port of shape
   *
   *     double** samples;
   *
   * then it is assumed that we are writing an effect processor, where the outputs
   * should match the inputs. There will be as many output channels as input channels,
   * with enough samples allocated to write from 0 to N.
   *
   * In all the other cases, it is necessary to specify the number of channels for the output
   * as in the Sidechain example.
   */
  struct {
    struct {
      meta_attribute(name, "In");
      const double** samples{};
      std::size_t channels{};
    } audio;

    struct TT {
      meta_attribute(name, "Gain");
      meta_attribute_widget(knob);
      meta_attribute_range(0.f, 100.f, 10.f);

      float value = 10;
    } gain;
    static_assert(avnd::parameter<TT>);
  } inputs;

  struct {
    struct {
      meta_attribute(name, "Out");
      double** samples{};
    } audio;
  } outputs;

  /** Most basic effect: multiply N samples of inputs by a gain into equivalent outputs **/
  void operator()(std::size_t N)
  {
    auto& gain = inputs.gain;
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    const auto chans = p1.channels;

    // Process the input buffer
    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      for (std::size_t j = 0; j < N; j++)
      {
        out[j] = in[j] * gain.value;
      }
    }
  }
};
}
