#pragma once
#include <iostream>
#include <halp/meta.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <experimental/mdspan>

#include <saf.h>
#include <rotator.h>
#include <cblas.h>

class Rotator
{
public:
    halp_meta(name, "Rotator")
    halp_meta(c_name, "avnd_rotator")
    halp_meta(uuid, "82bdb9c5-9cf8-440e-8675-c0xaf4fc59b9")

    using setup = halp::setup;
    using tick = halp::tick;

    struct
    {
      halp::dynamic_audio_bus<"Input", double> audio;
      halp::hslider_f32<"Weight", halp::range{.min = 0., .max = 1., .init = 0.5}> weight;
      halp::hslider_f32<"L/R", halp::range{.min = -1, .max = 1, .init = 0}> toto;
    } inputs;

    struct
    {
      halp::dynamic_audio_bus<"Output", double> audio;
    } outputs;

   /* Rotator()
    {

    }

    ~Rotator()
    {

    }*/
/*
    void test__saf_example_rotator(void){
        int ch, nSH, i, j, delay, framesize;
        void* hRot;
        float direction_deg[2], ypr[3], Rzyx[3][3];
        float** inSig, *y, **shSig_frame, **shSig_rot_frame;
        float** shSig, **shSig_rot, **shSig_rot_ref, **Mrot;

        // Config
        const float acceptedTolerance = 0.000001f;
        const int order = 4;
        const int fs = 48000;
        const int signalLength = fs*2;
        direction_deg[0] = 90.0f; // encode to loudspeaker direction: index 8
        direction_deg[1] = 0.0f;
        ypr[0] = -0.4f;
        ypr[1] = -1.4f;
        ypr[2] = 2.1f;
        delay = rotator_getProcessingDelay();

        // Create and initialise an instance of rotator
        rotator_create(&hRot);
        rotator_init(hRot, fs); // Cannot be called while "process" is on-going

        // Configure rotator codec
        rotator_setOrder(hRot, (SH_ORDERS)order);
        rotator_setNormType(hRot, NORM_N3D);
        rotator_setYaw(hRot, ypr[0]*180.0f/SAF_PI); // rad->degrees
        rotator_setPitch(hRot, ypr[1]*180.0f/SAF_PI);
        rotator_setRoll(hRot, ypr[2]*180.0f/SAF_PI);

        // Define input mono signal
        nSH = ORDER2NSH(order);
        inSig = (float**)malloc2d(1,signalLength,sizeof(float));
        shSig = (float**)malloc2d(nSH,signalLength,sizeof(float));
        rand_m1_1(FLATTEN2D(inSig), signalLength); // Mono white-noise signal

        // Encode
        y = malloc1d(nSH*sizeof(float));
        getRSH(order, (float*)direction_deg, 1, y); // SH plane-wave weights
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nSH, signalLength, 1, 1.0f,
                    y, 1,
                    FLATTEN2D(inSig), signalLength, 0.0f,
                    FLATTEN2D(shSig), signalLength);

        // Rotated version reference
        Mrot = (float**)malloc2d(nSH, nSH, sizeof(float));
        yawPitchRoll2Rzyx(ypr[0], ypr[1], ypr[2], 0, Rzyx);
        getSHrotMtxReal(Rzyx, FLATTEN2D(Mrot), order);
        shSig_rot_ref = (float**)malloc2d(nSH,signalLength,sizeof(float));
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nSH, signalLength, nSH, 1.0f,
                    FLATTEN2D(Mrot), nSH,
                    FLATTEN2D(shSig), signalLength, 0.0f,
                    FLATTEN2D(shSig_rot_ref), signalLength);

        // Rotate with rotator
        framesize = rotator_getFrameSize();
        shSig_rot = (float**)malloc2d(nSH,signalLength,sizeof(float));
        shSig_frame = (float**)malloc1d(nSH*sizeof(float*));
        shSig_rot_frame = (float**)malloc1d(nSH*sizeof(float*));
        for(i=0; i<(int)((float)signalLength/(float)framesize); i++){
            for(ch=0; ch<nSH; ch++)
                shSig_frame[ch] = &shSig[ch][i*framesize];
            for(ch=0; ch<nSH; ch++)
                shSig_rot_frame[ch] = &shSig_rot[ch][i*framesize];

            rotator_process(hRot, (const float* const*)shSig_frame, shSig_rot_frame, nSH, nSH, framesize);
        }

        // ambi_enc should be equivalent to the reference, except delayed due to the
         // temporal interpolation employed in ambi_enc
        for(i=0; i<nSH; i++)
            for(j=0; j<signalLength-delay; j++)
                TEST_ASSERT_FLOAT_WITHIN(acceptedTolerance, shSig_rot_ref[i][j], shSig_rot[i][j+delay]);

        // Clean-up
        rotator_destroy(&hRot);
        free(inSig);
        free(shSig);
        free(shSig_rot_ref);
        free(Mrot);
        free(y);
        free(shSig_frame);
        free(shSig_rot_frame);
    }
*/

    halp::setup setup_info;
    void prepare(halp::setup info) {
        previous_values.resize(info.input_channels);

      /*  //Config
        direction_deg[0] = 90.0f; // encode to loudspeaker direction: index 8
        direction_deg[1] = 0.0f;
        ypr[0] = -0.4f;
        ypr[1] = -1.4f;
        ypr[2] = 2.1f;

        // Create and initialise an instance of rotator
        rotator_create(&hRot);
        rotator_init(hRot, fs); // Cannot be called while "process" is on-going

        // Configure rotator codec
        rotator_setOrder(hRot, (SH_ORDERS)order);
        rotator_setNormType(hRot, NORM_N3D);
        rotator_setYaw(hRot, ypr[0]*180.0f/SAF_PI); // rad->degrees
        rotator_setPitch(hRot, ypr[1]*180.0f/SAF_PI);
        rotator_setRoll(hRot, ypr[2]*180.0f/SAF_PI);

        // Define input mono signal
        nSH = ORDER2NSH(order);*/
        /*inSig = (float**)malloc2d(1,signalLength,sizeof(float));                    //signal d'entrée
        shSig = (float**)malloc2d(nSH,signalLength,sizeof(float));                  //signal de sortie (= poids des sh * sig d'entrée)
        rand_m1_1(FLATTEN2D(inSig), signalLength); // Mono white-noise signal*/

        // Encode
        //y = (float*)malloc1d(nSH*sizeof(float));
        //getRSH(order, (float*)direction_deg, 1, y);                                 // SH plane-wave weights
        /*cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nSH, signalLength, 1, 1.0f,
                   y, 1,
                   FLATTEN2D(inSig), signalLength, 0.0f,
                   FLATTEN2D(shSig), signalLength);*/

        // Rotated version reference
        /*Mrot = (float**)malloc2d(nSH, nSH, sizeof(float));
        yawPitchRoll2Rzyx(ypr[0], ypr[1], ypr[2], 0, Rzyx);
        getSHrotMtxReal(Rzyx, FLATTEN2D(Mrot), order);
        shSig_rot_ref = (float**)malloc2d(nSH,signalLength,sizeof(float));
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nSH, signalLength, nSH, 1.0f,
                    FLATTEN2D(Mrot), nSH,
                    FLATTEN2D(shSig), signalLength, 0.0f,
                    FLATTEN2D(shSig_rot_ref), signalLength);*/

      /*  framesize = rotator_getFrameSize();

        shSig_rot = (float**)malloc2d(nSH,signalLength,sizeof(float));
        shSig_frame = (float**)malloc1d(nSH*sizeof(float*));
        shSig_rot_frame = (float**)malloc1d(nSH*sizeof(float*));
*/
    }

    void operator()(halp::tick t)
    {
        int nSamples = t.frames;
        //framesize = t.frames;

        for (int i = 0; i < inputs.audio.channels ; i++)
        //for (int i = 0; i < nSH ; i++)
        {

          //auto in_ = inputs.audio[0];
          //auto out_ = outputs.audio[i];
//          printf("%s", typeid(in_).name());

          //in[i] = in_;
          //out[i] = out_;

          //rotator_process(hRot, (const float* const*)shSig_frame, shSig_rot_frame, nSH, nSH, framesize);
        }
        //rotator_process(hRot, (const float* const*)in, out, nSH, nSH, framesize);

        // Clean-up
        //rotator_destroy(&hRot);
        //free(shSig);
        //free(shSig_rot_ref);
        //free(Mrot);
        //free(y);

    }

    void rotator_process
    (
        void        *  const hRot,
        double * inputs,
        double * outputs,
        int                  nInputs,
        int                  nOutputs,
        int                  nSamples
    )
    {
        //rotator_data *pData = (rotator_data*)(hRot);
    }

    // Main struct for the rotator
   /* typedef struct _rotator
    {
        // Internal buffers
        float inputFrameTD[MAX_NUM_SH_SIGNALS][ROTATOR_FRAME_SIZE];         //< Input frame of signals
        float prev_inputFrameTD[MAX_NUM_SH_SIGNALS][ROTATOR_FRAME_SIZE];    //< Previous frame of signals
        float tempFrame[MAX_NUM_SH_SIGNALS][ROTATOR_FRAME_SIZE];            //< Temporary frame
        float tempFrame_fadeOut[MAX_NUM_SH_SIGNALS][ROTATOR_FRAME_SIZE];    //< Temporary frame with linear interpolation (fade-out) applied
        float outputFrameTD[MAX_NUM_SH_SIGNALS][ROTATOR_FRAME_SIZE];        //< Output frame of SH signals
        float outputFrameTD_fadeIn[MAX_NUM_SH_SIGNALS][ROTATOR_FRAME_SIZE]; //< Output frame of SH signals with linear interpolation (fade-in) applied

        // Internal variables
        float interpolator_fadeIn[ROTATOR_FRAME_SIZE];       //< Linear Interpolator (fade-in)
        float interpolator_fadeOut[ROTATOR_FRAME_SIZE];      //< Linear Interpolator (fade-out)
        float M_rot[MAX_NUM_SH_SIGNALS][MAX_NUM_SH_SIGNALS];      //< Current SH rotation matrix [1]
        float prev_M_rot[MAX_NUM_SH_SIGNALS][MAX_NUM_SH_SIGNALS]; //< Previous SH rotation matrix [1]
        M_ROT_STATUS M_rot_status;      //< see #M_ROT_STATUS
        int fs;                         //< Host sampling rate, in Hz

        // user parameters
        quaternion_data Q;              //< Quaternion used for rotation
        int bFlipQuaternion;            //< 1: invert quaternion, 0: no inversion
        float yaw;                      //< yaw (Euler) rotation angle, in degrees
        float roll;                     //< roll (Euler) rotation angle, in degrees
        float pitch;                    //< pitch (Euler) rotation angle, in degrees
        int bFlipYaw;                   //< flag to flip the sign of the yaw rotation angle
        int bFlipPitch;                 //< flag to flip the sign of the pitch rotation angle
        int bFlipRoll;                  //< flag to flip the sign of the roll rotation angle
        int useRollPitchYawFlag;        //< rotation order flag, 1: r-p-y, 0: y-p-r
        CH_ORDER chOrdering;            //< Ambisonic channel order convention (see #CH_ORDER)
        NORM_TYPES norm;                //< Ambisonic normalisation convention (see #NORM_TYPES)
        SH_ORDERS inputOrder;           //< current input/output SH order

    } rotator_data;*/

private:
  std::vector<float> previous_values{};


/*
  int ch, nSH, framesize ;
  void* hRot;
  float direction_deg[2], ypr[3], Rzyx[3][3];
  float** inSig, *y, **shSig_frame, **shSig_rot_frame;
  float** shSig, **shSig_rot, **shSig_rot_ref, **Mrot;

  // Config
  const float acceptedTolerance = 0.000001f;
  const int order = 4;
  const int fs = 48000;
  const int signalLength = fs*2;*/
};
