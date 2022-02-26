#pragma once

namespace examples
{
/**
 * This example shows that there is absolutely
 * no magic involved in the definition of the plug-ins:
 * it is technically possible to define nodes that would be viable
 * for an entirely bootstrapped environment with not a single standard header,
 * be it C or C++.
 * Of course this is only given as an example - things are a bit more verbose
 * than in other examples.
 */
struct ZeroDependencyAudioEffect
{
  /** The meta_attribute macro used the other examples is simply some sugar for
   * defining a static function returning a value.
   *
   * The absolute minimal set of metadatas on an effect is:
   * - A name (to show to the user)
   * - An unique identifier (in order to be able to save / reload across machines,
   *   something identifying the class with a reasonably low collision potential is necessary,
   *   given how many audio effects are called "Chorus" or "Distortion", the name is not enough.
   */
  static constexpr auto name() { return "My zero-dependency effect"; }
  static constexpr auto uuid() { return "99fcf199-280c-4e7f-8be3-c34290073bb9"; }

  struct {
    struct {
      // Same for the inlet / outlet metadata.
      static constexpr auto name() { return "Input"; }
      const double** samples{};
      int channels{};
    } audio;

    struct {
      static constexpr auto name() { return "Yea"; }
      // Controls are just pre-made types with some metadata
      // relevant for setting them up it up.
      static constexpr auto control() {
        struct {
          const int min = 0;
          const int max = 30;
          const int init = 1;
        } c; return c;
      }

      int value = 10;
    } gain;
  } inputs;

  struct {
    struct {
      static constexpr auto name() { return "Output"; }
      double** samples{};
    } audio;
  } outputs;

  void operator()(int N)
  {
    const auto factor = inputs.gain.value;
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    const auto chans = p1.channels;

    // Process the input buffer
    for (int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      // Cronch cronch cronch
      for (int j = 0; j < N; j++)
      {
        out[j] = in[j];
        for(int i = 0; i < factor; i++)
          out[j] *= in[j] * factor;
        out[j] *= in[j] * (1 + factor);
        while((1. - out[j]*out[j]) > 1.0)
          out[j] = 1. - out[j];
        out[j] = out[j] < -0.999 ? -0.999 : out[j] > 0.999 ? 0.999 : out[j];
      }
    }
  }
};
}
