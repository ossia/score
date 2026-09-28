/* Minimal LV2 gain plug-in used by the LV2 loading/processing tests.
 *
 * Ports:
 *   0  audio  in   "in"
 *   1  audio  out  "out"
 *   2  control in  "gain"   [0, 4] default 1
 *   3  control out "level"  peak absolute value of the last block
 *   4  control in  "mode"   integer [0, 3] default 0 (ignored by run)
 *   5  control in  "bypass" toggled default 0        (ignored by run)
 */
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

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

/* Same doap:name as urn:score:test:gain — regression fixture for
 * descriptor-pinned lookup vs. display-name collisions. */
static const LV2_Descriptor descriptor2
    = {SCORE_TEST_GAIN_URI "2", instantiate, connect_port, NULL, run, NULL,
       cleanup,                  NULL};

/* urn:score:test:notify: answers the first event of each block on its atom
 * input with one NOTIFY_SIZE-byte event on its atom output, as a plug-in
 * sends its whole state to a UI that just opened. Both ports
 * ask for rsz:minimumSize 65536; an answer that does not fit the output
 * buffer is dropped, as an LV2 atom forge drops what overflows.
 *
 * Ports:
 *   0  atom in  "control"  atom:Sequence, supports midi:MidiEvent
 *   1  atom out "notify"   atom:Sequence, supports midi:MidiEvent
 */
#define NOTIFY_SIZE 10000

typedef struct
{
  const LV2_Atom_Sequence* in;
  LV2_Atom_Sequence* out;
} test_notify;

static LV2_Handle instantiate_notify(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_notify));
}

static void connect_port_notify(LV2_Handle instance, uint32_t port, void* data)
{
  test_notify* self = (test_notify*)instance;
  if(port == 0)
    self->in = (const LV2_Atom_Sequence*)data;
  else if(port == 1)
    self->out = (LV2_Atom_Sequence*)data;
}

static void run_notify(LV2_Handle instance, uint32_t n_samples)
{
  (void)n_samples;
  test_notify* self = (test_notify*)instance;
  if(!self->in || !self->out)
    return;

  const uint32_t capacity = self->out->atom.size;
  self->out->atom.type = self->in->atom.type; /* atom:Sequence */
  self->out->atom.size = sizeof(LV2_Atom_Sequence_Body);
  self->out->body.unit = 0;
  self->out->body.pad = 0;

  LV2_ATOM_SEQUENCE_FOREACH(self->in, ev)
  {
    const uint32_t needed = sizeof(LV2_Atom_Sequence_Body) + sizeof(LV2_Atom_Event)
                            + lv2_atom_pad_size(NOTIFY_SIZE);
    if(needed > capacity)
      break;
    LV2_Atom_Event* out = lv2_atom_sequence_end(&self->out->body, self->out->atom.size);
    out->time.frames = 0;
    out->body.type = ev->body.type;
    out->body.size = NOTIFY_SIZE;
    memset(out + 1, 0x5a, NOTIFY_SIZE);
    self->out->atom.size += sizeof(LV2_Atom_Event) + lv2_atom_pad_size(NOTIFY_SIZE);
    break;
  }
}

static const LV2_Descriptor descriptor_notify
    = {"urn:score:test:notify", instantiate_notify, connect_port_notify, NULL,
       run_notify,              NULL,               cleanup,             NULL};

/* urn:score:test:transport: reads time:Position objects, as a sequencer
 * following the host transport does (starting on a change of time:speed
 * from 0).
 *
 * Ports:
 *   0  atom in     "control"  atom:Sequence, supports time:Position
 *   1  control out "speed"    the last time:speed received
 *   2  control out "starts"   how many times the speed went from 0 to non-0
 */
typedef struct
{
  const LV2_Atom_Sequence* in;
  float* speed_out;
  float* starts_out;
  LV2_URID object, position, speed_key;
  float speed;
  float starts;
} test_transport;

static LV2_Handle instantiate_transport(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  LV2_URID_Map* map = NULL;
  for(int i = 0; features && features[i]; i++)
    if(!strcmp(features[i]->URI, LV2_URID__map))
      map = (LV2_URID_Map*)features[i]->data;
  if(!map)
    return NULL;
  test_transport* self = (test_transport*)calloc(1, sizeof(test_transport));
  self->object = map->map(map->handle, LV2_ATOM__Object);
  self->position = map->map(map->handle, LV2_TIME__Position);
  self->speed_key = map->map(map->handle, LV2_TIME__speed);
  return self;
}

static void connect_port_transport(LV2_Handle instance, uint32_t port, void* data)
{
  test_transport* self = (test_transport*)instance;
  if(port == 0)
    self->in = (const LV2_Atom_Sequence*)data;
  else if(port == 1)
    self->speed_out = (float*)data;
  else if(port == 2)
    self->starts_out = (float*)data;
}

static void run_transport(LV2_Handle instance, uint32_t n_samples)
{
  (void)n_samples;
  test_transport* self = (test_transport*)instance;
  if(self->in)
  {
    LV2_ATOM_SEQUENCE_FOREACH(self->in, ev)
    {
      if(ev->body.type != self->object)
        continue;
      const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
      if(obj->body.otype != self->position)
        continue;
      const LV2_Atom* speed = NULL;
      lv2_atom_object_get(obj, self->speed_key, &speed, 0);
      if(!speed || speed->size != sizeof(float))
        continue;
      const float s = ((const LV2_Atom_Float*)speed)->body;
      if(self->speed == 0.f && s != 0.f)
        self->starts += 1.f;
      self->speed = s;
    }
  }
  if(self->speed_out)
    *self->speed_out = self->speed;
  if(self->starts_out)
    *self->starts_out = self->starts;
}

/* activate() leaves the transport state alone, so the instance keeps it
 * from one play to the next. */
static void activate_transport(LV2_Handle instance)
{
  (void)instance;
}

static const LV2_Descriptor descriptor_transport
    = {"urn:score:test:transport", instantiate_transport, connect_port_transport,
       activate_transport,         run_transport,          NULL,
       cleanup,                    NULL};

/* urn:score:test:counter: no activate(); counts its runs since it was
 * instantiated, as a generator keeps its position.
 *
 * Ports:
 *   0  control out "runs"
 */
typedef struct
{
  float* out;
  float runs;
} test_counter;

static LV2_Handle instantiate_counter(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_counter));
}

static void connect_port_counter(LV2_Handle instance, uint32_t port, void* data)
{
  if(port == 0)
    ((test_counter*)instance)->out = (float*)data;
}

static void run_counter(LV2_Handle instance, uint32_t n_samples)
{
  (void)n_samples;
  test_counter* self = (test_counter*)instance;
  self->runs += 1.f;
  if(self->out)
    *self->out = self->runs;
}

static const LV2_Descriptor descriptor_counter
    = {"urn:score:test:counter", instantiate_counter, connect_port_counter, NULL,
       run_counter,              NULL,                cleanup,              NULL};

/* urn:score:test:envelope
 *   0  CV in "gate"
 *   1  control in "time" (minimum only)
 *   2  CV out "out" = gate + time
 *   3  control in "level"
 */
typedef struct
{
  const float* gate;
  const float* time;
  float* out;
} test_envelope;

static LV2_Handle instantiate_envelope(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_envelope));
}

static void connect_port_envelope(LV2_Handle instance, uint32_t port, void* data)
{
  test_envelope* self = (test_envelope*)instance;
  switch(port)
  {
    case 0:
      self->gate = (const float*)data;
      break;
    case 1:
      self->time = (const float*)data;
      break;
    case 2:
      self->out = (float*)data;
      break;
    default:
      break;
  }
}

static void run_envelope(LV2_Handle instance, uint32_t n_samples)
{
  test_envelope* self = (test_envelope*)instance;
  for(uint32_t i = 0; i < n_samples; i++)
    self->out[i] = self->gate[i] + *self->time;
}

static const LV2_Descriptor descriptor_envelope
    = {"urn:score:test:envelope", instantiate_envelope, connect_port_envelope, NULL,
       run_envelope,              NULL,                 cleanup,               NULL};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
  switch(index)
  {
    case 0:
      return &descriptor;
    case 1:
      return &descriptor2;
    case 2:
      return &descriptor_notify;
    case 3:
      return &descriptor_transport;
    case 4:
      return &descriptor_counter;
    case 5:
      return &descriptor_envelope;
    default:
      return NULL;
  }
}
