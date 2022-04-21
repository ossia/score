#pragma once
#include <iostream>
#include <halp/meta.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <experimental/mdspan>

class StereoToMono
{
public:
    halp_meta(name, "StereoToMono")
    halp_meta(c_name, "avnd_stereotomono")
    halp_meta(uuid, "82bdb9c5-9cf1-440e-8675-c0caf4fc59b9")

    using setup = halp::setup;
    using tick = halp::tick;

    struct
    {
      halp::dynamic_audio_bus<"Input", double> audio;
    } inputs;

    struct
    {
      halp::dynamic_audio_bus<"Output", double> audio;
    } outputs;

    /*MaSpat()
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
        if(inputs.audio.channels == 0)
            return;

        // Process the input buffer
        //printf("test : %d", t.frames);

        auto* l_in = inputs.audio[0];
        auto* r_in = inputs.audio[1];

        auto* l_out = outputs.audio[0];
        auto* r_out = outputs.audio[1];

        //float& prev = this->previous_values[i];

        for (int j = 0; j < t.frames; j++)
        {
            auto out = (l_in[j] + r_in[j])/2.0f;
            l_out[j] = out;
            r_out[j] = out;
            //prev = out;
        }


        //std::intptr_t ptr = (std::intptr_t) &smb_pitchShift_apply;
        //std::cout << ptr << std::endl;
    }

private:
  std::vector<float> previous_values{};
};



