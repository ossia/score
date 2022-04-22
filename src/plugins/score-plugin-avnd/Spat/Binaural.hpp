#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <iostream>
//#include <experimental/mdspan>
#include <saf.h>

#define MAX_ORDER 7
#define MAX_NSH ((MAX_ORDER + 1) * (MAX_ORDER + 1))

class Binaural
{
public:
  halp_meta(name, "Binaural") halp_meta(c_name, "avnd_binaural")
      halp_meta(uuid, "82udb9f5-9cf8-440e-8675-c0caf4fc59b9")

          using setup = halp::setup;
  using tick = halp::tick;

  struct
  {
    halp::dynamic_audio_bus<"Input", float> audio;
    halp::hslider_i32<
        "Order",
        halp::range{.min = 0, .max = MAX_ORDER, .init = 0}>
        order;
    halp::knob_f32<"Yaw", halp::range{.min = -180.0, .max = 180.0, .init = 0}>
        yaw;
    halp::
        knob_f32<"Pitch", halp::range{.min = -180.0, .max = 180.0, .init = 0}>
            pitch;
    halp::knob_f32<"Roll", halp::range{.min = -180.0, .max = 180.0, .init = 0}>
        roll;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", float> audio;
  } outputs;

  /*Binaural()
    {

    }

    ~Binaural()
    {

    }*/

  halp::setup setup_info;
  void prepare(halp::setup info)
  {
    previous_values.resize(info.input_channels);
  }

  void operator()(halp::tick t)
  {
    if (inputs.audio.channels == 0)
      return;

    // Rotate source directions
    // Interpolate hrtfs and apply to each source
    // Convolve this channel with the interpolated HRTF, and add it to the binaural buffer
    // Scale by number of sources
    // Inverse-TFT
  }

private:
  std::vector<float> previous_values{};
  int order, nSH, nSamples;
  float yaw, pitch, roll;
  float prevM_rot[MAX_NSH][MAX_NSH];
};
