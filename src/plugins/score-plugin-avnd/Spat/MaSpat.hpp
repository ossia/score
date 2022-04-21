#pragma once
#include <iostream>
#include <halp/meta.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <experimental/mdspan>

class MaSpat
{
public:
    halp_meta(name, "MaSpat")
    halp_meta(c_name, "avnd_maspat")
    halp_meta(uuid, "82bdb9c5-9cf8-440e-8675-c0caf4fc59b9")

    using setup = halp::setup;
    using tick = halp::tick;

    struct
    {
      halp::dynamic_audio_bus<"Input", double> audio;
      halp::hslider_f32<"L/R", halp::range{.min = -1, .max = 1, .init = 0}> toto;
    } inputs;

    struct
    {
      halp::dynamic_audio_bus<"Output", double> audio;
    } outputs;

   /* MaSpat()
    {

    }

    ~MaSpat()
    {

    }*/
    
    halp::setup setup_info;
    void prepare(halp::setup info) {
        previous_values.resize(info.input_channels);
    }

    void operator()(halp::tick t)
    {
        if(inputs.audio.channels == 2 && outputs.audio.channels == 2)
        {
            auto coeff = inputs.toto;

            auto* l_in = inputs.audio[0];
            auto* r_in = inputs.audio[1];

            auto* l_out = outputs.audio[0];
            auto* r_out = outputs.audio[1];

            //float& prev = this->previous_values[i];

            for (int j = 0; j < t.frames; j++)
            {
              l_out[j] = l_in[j] * (1-coeff);
              r_out[j] = r_in[j] * (1+coeff);
              //prev = out[j];
            }
            return;
        }

        for (int i = 0; i < inputs.audio.channels ; i++)
        {
          auto* in = inputs.audio[i];
          auto* out = outputs.audio[i];

          float& prev = this->previous_values[i];

          for (int j = 0; j < t.frames; j++)
          {
            out[j] = in[j];
            prev = out[j];
          }
        }

        //std::intptr_t ptr = (std::intptr_t) &smb_pitchShift_apply;
        //std::cout << ptr << std::endl;
    }

private:
  std::vector<float> previous_values{};
};



