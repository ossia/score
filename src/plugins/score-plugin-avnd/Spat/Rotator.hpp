#pragma once
#include <iostream>
//#include <vector>
//#include <iomanip>
#include <halp/meta.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
//#include <experimental/mdspan>

#include <saf.h>
//#include <rotator.h>
//#include <cblas.h>

#define MAX_ORDER 7
#define MAX_NSH ((MAX_ORDER+1)*(MAX_ORDER+1))

class Rotator
{
public:
    halp_meta(name, "Rotator")
    halp_meta(c_name, "avnd_rotator")
    halp_meta(uuid, "82bdb9f5-9cf8-440e-8675-c0caf4fc59b9")

    using setup = halp::setup;
    using tick = halp::tick;

    struct
    {
      halp::dynamic_audio_bus<"Input", float> audio;
      halp::hslider_i32<"Order", halp::range{.min = 0, .max = MAX_ORDER, .init = 0}> order;
      halp::knob_f32<"Yaw", halp::range{.min = -180.0, .max = 180.0, .init = 0}> yaw;
      halp::knob_f32<"Pitch", halp::range{.min = -180.0, .max = 180.0, .init = 0}> pitch;
      halp::knob_f32<"Roll", halp::range{.min = -180.0, .max = 180.0, .init = 0}> roll;
    } inputs;

    struct
    {
      halp::dynamic_audio_bus<"Output", float> audio;
    } outputs;

    /*Rotator()
    {

    }

    ~Rotator()
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

        order = inputs.order;
        nSH = (order+1)*(order+1);
        nSamples = t.frames;

        yaw = inputs.yaw * M_PI/180.0;
        pitch = inputs.pitch * M_PI/180.0;
        roll = inputs.roll * M_PI/180.0;

        /*while(nSH > inputs.audio.channels)
        {
            order -= 1;
            nSH = (order+1)*(order+1);
        }*/

        float** in = inputs.audio.samples;
        float** out = outputs.audio.samples;

        quaternion_data Q;
        float inFrame[MAX_NSH][nSamples];
        float M_rot[MAX_NSH][MAX_NSH];

        float Rxyz[3][3] ;
        float M_rot_tmp[MAX_NSH*MAX_NSH] ;

        int i,j,k;

        for(i=0 ; i<nSamples ; i++)
            for(j=0 ; j<nSH ; j++)
                inFrame[j][i] = in[0][i];

        yawPitchRoll2Rzyx (yaw, pitch, roll, 1, Rxyz);
        euler2Quaternion(inputs.yaw, inputs.pitch, inputs.roll, 0, 0 ? EULER_ROTATION_ROLL_PITCH_YAW : EULER_ROTATION_YAW_PITCH_ROLL, &Q);
        getSHrotMtxReal(Rxyz, (float*)M_rot_tmp, order);

        bool sameRot = true;

        for(int i=0; i<nSH; i++)
            for(int j=0; j<nSH; j++)
            {
                M_rot[i][j] = M_rot_tmp[i*nSH+j];
                if(M_rot[i][j] != prevM_rot[i][j])               
                    sameRot = false;
            }


        for(i=0; i < nSH; i++)
             for(j = 0; j < nSamples; j++)
             {
                 out[i][j]=0.0f;
                 for(k = 0; k < nSH; k++)
                     out[i][j] += M_rot[i][k] * inFrame[k][j];
             }


        if(!sameRot)
        {
            //interpolation linéaire
            float tmpFrame[MAX_NSH][nSamples];
            float fadeIn[nSamples], fadeOut[nSamples];

            for(i=0; i<nSamples; i++)
            {
                fadeIn[i] = (float)(i+1)*1.0f/(float)nSamples;
                fadeOut[i] = 1.0f-fadeIn[i];
            }

            for(i=0; i < nSH; i++)
                for(j = 0; j < nSamples; j++)
                {
                    tmpFrame[i][j]=0.0f;
                    for(k = 0; k < nSH; k++)
                        tmpFrame[i][j] += prevM_rot[i][k] * inFrame[k][j];

                    out[i][j] = out[i][j] * fadeIn[j] + tmpFrame[i][j] * fadeOut[j] ;
                    prevM_rot[i][j] = M_rot[i][j];
                }
        }

        for(i=nSH ; i<inputs.audio.channels ; i++)
            for(j = 0; j < nSamples; j++)
                out[i][j] = 0.0f;
    }



private:
  std::vector<float> previous_values{};
  int order, nSH, nSamples;
  float yaw, pitch, roll;
  float prevM_rot[MAX_NSH][MAX_NSH];
};
