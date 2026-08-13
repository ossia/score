/* Minimal LV2 gain plug-in used by the LV2 loading/processing tests.
 *
 * Ports:
 *   0  audio  in   "in"
 *   1  audio  out  "out"
 *   2  control in  "gain"  [0, 4] default 1
 *   3  control out "level" peak absolute value of the last block
 */
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#include <math.h>
#include <stdlib.h>

#define SCORE_TEST_GAIN_URI "urn:score:test:gain"

typedef struct
{
  const float* in;
  float* out;
  const float* gain;
  float* level;
} test_gain;

static LV2_Handle
instantiate(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_gain));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
  test_gain* self = (test_gain*)instance;
  switch(port)
  {
    case 0:
      self->in = (const float*)data;
      break;
    case 1:
      self->out = (float*)data;
      break;
    case 2:
      self->gain = (const float*)data;
      break;
    case 3:
      self->level = (float*)data;
      break;
  }
}

static void run(LV2_Handle instance, uint32_t n_samples)
{
  test_gain* self = (test_gain*)instance;
  const float gain = self->gain ? *self->gain : 1.f;
  float peak = 0.f;

  if(self->in && self->out)
  {
    for(uint32_t i = 0; i < n_samples; i++)
    {
      const float v = self->in[i] * gain;
      self->out[i] = v;
      const float a = fabsf(v);
      if(a > peak)
        peak = a;
    }
  }

  if(self->level)
    *self->level = peak;
}

static void cleanup(LV2_Handle instance)
{
  free(instance);
}

static const LV2_Descriptor descriptor
    = {SCORE_TEST_GAIN_URI, instantiate, connect_port, NULL, run, NULL,
       cleanup,             NULL};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
  return index == 0 ? &descriptor : NULL;
}
