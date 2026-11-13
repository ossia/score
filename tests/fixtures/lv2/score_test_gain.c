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
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/worker/worker.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

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

/* urn:score:test:atom-notify: the same, on atom ports that support no MIDI,
 * as B.Choppr's: a UI sends its messages there and reads the answers.
 *
 * Ports:
 *   0  atom in  "control"  atom:Sequence
 *   1  atom out "notify"   atom:Sequence
 */
static const LV2_Descriptor descriptor_atom_notify
    = {"urn:score:test:atom-notify", instantiate_notify, connect_port_notify, NULL,
       run_notify,                   NULL,               cleanup,             NULL};

/* urn:score:test:transport: reads time:Position objects, as a sequencer
 * following the host transport does (starting on a change of time:speed
 * from 0).
 *
 * Ports:
 *   0  atom in     "control"  atom:Sequence, supports time:Position
 *   1  control out "speed"    the last time:speed received
 *   2  control out "starts"   how many times the speed went from 0 to non-0
 *   3  control out "frame"    the last time:frame received
 *   4  control out "beat"     the last time:beat received
 */
typedef struct
{
  const LV2_Atom_Sequence* in;
  float* speed_out;
  float* starts_out;
  float* frame_out;
  float* beat_out;
  LV2_URID object, position, speed_key, frame_key, beat_key;
  float speed;
  float starts;
  float frame;
  float beat;
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
  self->frame_key = map->map(map->handle, LV2_TIME__frame);
  self->beat_key = map->map(map->handle, LV2_TIME__beat);
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
  else if(port == 3)
    self->frame_out = (float*)data;
  else if(port == 4)
    self->beat_out = (float*)data;
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
      const LV2_Atom* frame = NULL;
      const LV2_Atom* beat = NULL;
      lv2_atom_object_get(
          obj, self->speed_key, &speed, self->frame_key, &frame, self->beat_key, &beat,
          0);
      if(frame && frame->size == sizeof(int64_t))
        self->frame = (float)((const LV2_Atom_Long*)frame)->body;
      if(beat && beat->size == sizeof(double))
        self->beat = (float)((const LV2_Atom_Double*)beat)->body;
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
  if(self->frame_out)
    *self->frame_out = self->frame;
  if(self->beat_out)
    *self->beat_out = self->beat;
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

/* urn:score:test:rate
 *   0  control in "rate" (lv2:sampleRate)
 *   1  control in "gain"
 *   2  audio out "out" = rate
 *   3  control out "rate_echo" (lv2:sampleRate) = rate
 *   4  control out "gain_echo" = gain
 */
typedef struct
{
  const float* rate;
  const float* gain;
  float* out;
  float* rate_echo;
  float* gain_echo;
} test_rate;

static LV2_Handle instantiate_rate(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_rate));
}

static void connect_port_rate(LV2_Handle instance, uint32_t port, void* data)
{
  test_rate* self = (test_rate*)instance;
  switch(port)
  {
    case 0:
      self->rate = (const float*)data;
      break;
    case 1:
      self->gain = (const float*)data;
      break;
    case 2:
      self->out = (float*)data;
      break;
    case 3:
      self->rate_echo = (float*)data;
      break;
    case 4:
      self->gain_echo = (float*)data;
      break;
    default:
      break;
  }
}

static void run_rate(LV2_Handle instance, uint32_t n_samples)
{
  test_rate* self = (test_rate*)instance;
  for(uint32_t i = 0; i < n_samples; i++)
    self->out[i] = *self->rate;
  *self->rate_echo = *self->rate;
  *self->gain_echo = *self->gain;
}

static const LV2_Descriptor descriptor_rate
    = {"urn:score:test:rate", instantiate_rate, connect_port_rate, NULL,
       run_rate,              NULL,             cleanup,           NULL};

/* urn:score:test:thru
 *   0  atom in "in" (MIDI, time:Position)
 *   1  atom out "out" (MIDI, time:Position): every event of "in"
 *   2  control out "shortest": the fewest frames of a run so far
 *   3  control out "sequence": 1 when "in" is an atom:Sequence, as plug-ins
 *      checking the type of their input (JUCE's) require
 * Connecting any port writes "shortest" where it is connected so far, as
 * Rubber Band updates its latency: score_test_thru_first_write() says where
 * the first of those went since score_test_thru_forget().
 */
static const void* thru_first_write;
LV2_SYMBOL_EXPORT const void* score_test_thru_first_write(void)
{
  return thru_first_write;
}
LV2_SYMBOL_EXPORT void score_test_thru_forget(void)
{
  thru_first_write = NULL;
}

typedef struct
{
  const LV2_Atom_Sequence* in;
  LV2_Atom_Sequence* out;
  float* shortest;
  float* sequence;
  uint32_t min_frames;
  LV2_URID atom_Sequence;
} test_thru;

static LV2_Handle instantiate_thru(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  test_thru* self = (test_thru*)calloc(1, sizeof(test_thru));
  if(!self)
    return NULL;
  self->min_frames = UINT32_MAX;
  for(int i = 0; features && features[i]; i++)
    if(!strcmp(features[i]->URI, LV2_URID__map))
    {
      LV2_URID_Map* map = (LV2_URID_Map*)features[i]->data;
      self->atom_Sequence = map->map(map->handle, LV2_ATOM__Sequence);
    }
  return self;
}

static void connect_port_thru(LV2_Handle instance, uint32_t port, void* data)
{
  test_thru* self = (test_thru*)instance;
  if(self->shortest)
  {
    *self->shortest = (float)self->min_frames;
    if(!thru_first_write)
      thru_first_write = self->shortest;
  }
  switch(port)
  {
    case 0:
      self->in = (const LV2_Atom_Sequence*)data;
      break;
    case 1:
      self->out = (LV2_Atom_Sequence*)data;
      break;
    case 2:
      self->shortest = (float*)data;
      break;
    case 3:
      self->sequence = (float*)data;
      break;
    default:
      break;
  }
}

// Present so that the instance lives from one play to the next
static void activate_thru(LV2_Handle instance)
{
  (void)instance;
}

static void run_thru(LV2_Handle instance, uint32_t n_samples)
{
  test_thru* self = (test_thru*)instance;
  if(n_samples < self->min_frames)
    self->min_frames = n_samples;
  *self->shortest = (float)self->min_frames;
  *self->sequence = self->in->atom.type == self->atom_Sequence ? 1.f : 0.f;

  const uint32_t capacity = self->out->atom.size;
  const LV2_URID type = self->in->atom.type;
  lv2_atom_sequence_clear(self->out);
  self->out->atom.type = type;
  LV2_ATOM_SEQUENCE_FOREACH(self->in, ev)
  {
    lv2_atom_sequence_append_event(self->out, capacity, ev);
  }
}

static const LV2_Descriptor descriptor_thru
    = {"urn:score:test:thru", instantiate_thru, connect_port_thru, activate_thru,
       run_thru,              NULL,             cleanup,           NULL};

/* urn:score:test:handover
 *   0  control out "active": 1 between activate() and deactivate()
 *   1  atom port "side" (MIDI), neither an input nor an output
 *   2  atom port "aux", neither an input nor an output
 * score_test_handover_connected(p) says where port p was last connected,
 * score_test_handover_ran_with(p) where it pointed in the last run.
 */
static const void* handover_connected[3];
static const void* handover_ran_with[3];
LV2_SYMBOL_EXPORT const void* score_test_handover_connected(uint32_t port)
{
  return port < 3 ? handover_connected[port] : NULL;
}
LV2_SYMBOL_EXPORT const void* score_test_handover_ran_with(uint32_t port)
{
  return port < 3 ? handover_ran_with[port] : NULL;
}

typedef struct
{
  void* ports[3];
  float active;
} test_handover;

static LV2_Handle instantiate_handover(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_handover));
}

static void connect_port_handover(LV2_Handle instance, uint32_t port, void* data)
{
  test_handover* self = (test_handover*)instance;
  if(port < 3)
  {
    self->ports[port] = data;
    handover_connected[port] = data;
  }
}

static void activate_handover(LV2_Handle instance)
{
  ((test_handover*)instance)->active = 1.f;
}

static void deactivate_handover(LV2_Handle instance)
{
  ((test_handover*)instance)->active = 0.f;
}

static void run_handover(LV2_Handle instance, uint32_t n_samples)
{
  (void)n_samples;
  test_handover* self = (test_handover*)instance;
  for(int p = 0; p < 3; p++)
    handover_ran_with[p] = self->ports[p];
  if(self->ports[0])
    *(float*)self->ports[0] = self->active;
}

static const LV2_Descriptor descriptor_handover
    = {"urn:score:test:handover", instantiate_handover, connect_port_handover,
       activate_handover,         run_handover,         deactivate_handover,
       cleanup,                   NULL};

/* urn:score:test:mute
 *   0  audio in   "in"
 *   1  audio out  "out": the input
 *   2  atom out   "midi_out" (MIDI): never written, as some plug-ins leave an
 *      output they have nothing to say on
 */
typedef struct
{
  const float* in;
  float* out;
} test_mute;

static LV2_Handle instantiate_mute(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, sizeof(test_mute));
}

static void connect_port_mute(LV2_Handle instance, uint32_t port, void* data)
{
  test_mute* self = (test_mute*)instance;
  if(port == 0)
    self->in = (const float*)data;
  else if(port == 1)
    self->out = (float*)data;
}

static void run_mute(LV2_Handle instance, uint32_t n_samples)
{
  test_mute* self = (test_mute*)instance;
  if(self->in && self->out)
    memmove(self->out, self->in, n_samples * sizeof(float));
}

static const LV2_Descriptor descriptor_mute
    = {"urn:score:test:mute", instantiate_mute, connect_port_mute, NULL,
       run_mute,              NULL,             cleanup,           NULL};

/* urn:score:test:worker
 *   0  audio in   "in"
 *   1  audio out  "out": the input
 * Its first run schedules one work, which takes 300 ms, as loading a file
 * does. score_test_worker_done() counts the works that ran,
 * score_test_worker_responses() the answers that reached the plug-in,
 * score_test_worker_after_cleanup() the works that ran on an instance
 * already cleaned up: its memory is kept, marked, so that they can be told.
 * score_test_worker_concurrent() counts the works that began while another
 * one ran.
 */
#define SCORE_TEST_WORKER_ALIVE 0x600dU
#define SCORE_TEST_WORKER_DEAD 0xdeadU
static volatile int worker_done;
static volatile int worker_responses;
static volatile int worker_after_cleanup;
static int worker_running;
static volatile int worker_concurrent;
LV2_SYMBOL_EXPORT int score_test_worker_concurrent(void)
{
  return worker_concurrent;
}
LV2_SYMBOL_EXPORT int score_test_worker_done(void)
{
  return worker_done;
}
LV2_SYMBOL_EXPORT int score_test_worker_responses(void)
{
  return worker_responses;
}
LV2_SYMBOL_EXPORT int score_test_worker_after_cleanup(void)
{
  return worker_after_cleanup;
}

typedef struct
{
  unsigned magic;
  const float* in;
  float* out;
  LV2_Worker_Schedule* schedule;
  int scheduled;
} test_worker;

static LV2_Handle instantiate_worker(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  test_worker* self = (test_worker*)calloc(1, sizeof(test_worker));
  if(!self)
    return NULL;
  self->magic = SCORE_TEST_WORKER_ALIVE;
  for(int i = 0; features && features[i]; i++)
    if(!strcmp(features[i]->URI, LV2_WORKER__schedule))
      self->schedule = (LV2_Worker_Schedule*)features[i]->data;
  return self;
}

static void connect_port_worker(LV2_Handle instance, uint32_t port, void* data)
{
  test_worker* self = (test_worker*)instance;
  if(port == 0)
    self->in = (const float*)data;
  else if(port == 1)
    self->out = (float*)data;
}

static void run_worker(LV2_Handle instance, uint32_t n_samples)
{
  test_worker* self = (test_worker*)instance;
  if(self->in && self->out)
    memmove(self->out, self->in, n_samples * sizeof(float));
  if(!self->scheduled && self->schedule)
  {
    static const char job[4] = "job";
    if(self->schedule->schedule_work(self->schedule->handle, sizeof(job), job)
       == LV2_WORKER_SUCCESS)
      self->scheduled = 1;
  }
}

static LV2_Worker_Status work_worker(
    LV2_Handle instance, LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle, uint32_t size, const void* data)
{
  (void)size;
  (void)data;
  test_worker* self = (test_worker*)instance;
  if(__atomic_add_fetch(&worker_running, 1, __ATOMIC_SEQ_CST) > 1)
    worker_concurrent++;
#ifdef _WIN32
  Sleep(300);
#else
  usleep(300000);
#endif
  __atomic_sub_fetch(&worker_running, 1, __ATOMIC_SEQ_CST);
  if(self->magic != SCORE_TEST_WORKER_ALIVE)
  {
    worker_after_cleanup++;
    return LV2_WORKER_ERR_UNKNOWN;
  }
  worker_done++;
  static const char answer[5] = "done";
  return respond(handle, sizeof(answer), answer);
}

static LV2_Worker_Status
work_response_worker(LV2_Handle instance, uint32_t size, const void* body)
{
  (void)instance;
  (void)size;
  (void)body;
  worker_responses++;
  return LV2_WORKER_SUCCESS;
}

static const LV2_Worker_Interface worker_interface
    = {work_worker, work_response_worker, NULL};

static const void* extension_data_worker(const char* uri)
{
  return !strcmp(uri, LV2_WORKER__interface) ? &worker_interface : NULL;
}

static void cleanup_worker(LV2_Handle instance)
{
  // Not freed: a work still running on it must find it marked
  ((test_worker*)instance)->magic = SCORE_TEST_WORKER_DEAD;
}

static const LV2_Descriptor descriptor_worker
    = {"urn:score:test:worker", instantiate_worker,    connect_port_worker, NULL,
       run_worker,              NULL,                  cleanup_worker,      extension_data_worker};

/* urn:score:test:x11ui: a control the plug-in does nothing with, which its
 * X11 UI (score_test_x11ui.c) reads.
 *
 * Ports:
 *   0  control in  "size"  [0, 1] default 0
 */
static LV2_Handle instantiate_x11ui(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  (void)features;
  return calloc(1, 1);
}

static void connect_port_x11ui(LV2_Handle instance, uint32_t port, void* data)
{
  (void)instance;
  (void)port;
  (void)data;
}

static void run_x11ui(LV2_Handle instance, uint32_t n_samples)
{
  (void)instance;
  (void)n_samples;
}

static const LV2_Descriptor descriptor_x11ui
    = {"urn:score:test:x11ui", instantiate_x11ui, connect_port_x11ui, NULL,
       run_x11ui,              NULL,              cleanup,            NULL};

/* urn:score:test:worker-echo: copies its input; each run schedules a work
 * that carries the instance, whose answer carries it back, as does the work
 * that answer schedules in its turn from work_response().
 *
 * Ports:
 *   0  audio  in   "in"
 *   1  audio  out  "out"
 *
 * The counters are shared by all instances; "misrouted" counts the works and
 * answers that reached an instance other than the one they carry.
 */
typedef struct
{
  const void* self;
  int from_response;
} echo_packet;

static int echo_scheduled;
static int echo_schedule_unknown;
static int echo_schedule_no_space;
static int echo_works;
static int echo_misrouted;
static int echo_responses;
static int echo_response_scheduled;
static int echo_response_schedule_failed;

#define ECHO_COUNTER(name)                                \
  LV2_SYMBOL_EXPORT int score_test_echo_##name(void)      \
  {                                                       \
    return __atomic_load_n(&echo_##name, __ATOMIC_SEQ_CST); \
  }
ECHO_COUNTER(scheduled)
ECHO_COUNTER(schedule_unknown)
ECHO_COUNTER(schedule_no_space)
ECHO_COUNTER(works)
ECHO_COUNTER(misrouted)
ECHO_COUNTER(responses)
ECHO_COUNTER(response_scheduled)
ECHO_COUNTER(response_schedule_failed)
#undef ECHO_COUNTER

LV2_SYMBOL_EXPORT void score_test_echo_reset(void)
{
  int* all[] = {&echo_scheduled, &echo_schedule_unknown, &echo_schedule_no_space,
                &echo_works,     &echo_misrouted,        &echo_responses,
                &echo_response_scheduled, &echo_response_schedule_failed};
  for(unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++)
    __atomic_store_n(all[i], 0, __ATOMIC_SEQ_CST);
}

typedef struct
{
  const float* in;
  float* out;
  LV2_Worker_Schedule* schedule;
} test_echo;

static LV2_Handle instantiate_echo(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  test_echo* self = (test_echo*)calloc(1, sizeof(test_echo));
  if(!self)
    return NULL;
  for(int i = 0; features && features[i]; i++)
    if(!strcmp(features[i]->URI, LV2_WORKER__schedule))
      self->schedule = (LV2_Worker_Schedule*)features[i]->data;
  return self;
}

static void connect_port_echo(LV2_Handle instance, uint32_t port, void* data)
{
  test_echo* self = (test_echo*)instance;
  if(port == 0)
    self->in = (const float*)data;
  else if(port == 1)
    self->out = (float*)data;
}

static void run_echo(LV2_Handle instance, uint32_t n_samples)
{
  test_echo* self = (test_echo*)instance;
  if(self->in && self->out)
    memmove(self->out, self->in, n_samples * sizeof(float));
  if(!self->schedule)
    return;
  echo_packet p = {self, 0};
  switch(self->schedule->schedule_work(self->schedule->handle, sizeof(p), &p))
  {
    case LV2_WORKER_SUCCESS:
      __atomic_add_fetch(&echo_scheduled, 1, __ATOMIC_SEQ_CST);
      break;
    case LV2_WORKER_ERR_NO_SPACE:
      __atomic_add_fetch(&echo_schedule_no_space, 1, __ATOMIC_SEQ_CST);
      break;
    default:
      __atomic_add_fetch(&echo_schedule_unknown, 1, __ATOMIC_SEQ_CST);
      break;
  }
}

static LV2_Worker_Status work_echo(
    LV2_Handle instance, LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle, uint32_t size, const void* data)
{
  __atomic_add_fetch(&echo_works, 1, __ATOMIC_SEQ_CST);
  echo_packet p;
  if(size != sizeof(p))
  {
    __atomic_add_fetch(&echo_misrouted, 1, __ATOMIC_SEQ_CST);
    return LV2_WORKER_ERR_UNKNOWN;
  }
  memcpy(&p, data, sizeof(p));
  if(p.self != instance)
    __atomic_add_fetch(&echo_misrouted, 1, __ATOMIC_SEQ_CST);
  return respond(handle, sizeof(p), &p);
}

static LV2_Worker_Status
work_response_echo(LV2_Handle instance, uint32_t size, const void* body)
{
  test_echo* self = (test_echo*)instance;
  __atomic_add_fetch(&echo_responses, 1, __ATOMIC_SEQ_CST);
  echo_packet p;
  if(size != sizeof(p))
  {
    __atomic_add_fetch(&echo_misrouted, 1, __ATOMIC_SEQ_CST);
    return LV2_WORKER_ERR_UNKNOWN;
  }
  memcpy(&p, body, sizeof(p));
  if(p.self != instance)
    __atomic_add_fetch(&echo_misrouted, 1, __ATOMIC_SEQ_CST);
  if(!p.from_response && self->schedule)
  {
    p.from_response = 1;
    if(self->schedule->schedule_work(self->schedule->handle, sizeof(p), &p)
       == LV2_WORKER_SUCCESS)
      __atomic_add_fetch(&echo_response_scheduled, 1, __ATOMIC_SEQ_CST);
    else
      __atomic_add_fetch(&echo_response_schedule_failed, 1, __ATOMIC_SEQ_CST);
  }
  return LV2_WORKER_SUCCESS;
}

static const LV2_Worker_Interface echo_interface
    = {work_echo, work_response_echo, NULL};

static const void* extension_data_echo(const char* uri)
{
  return !strcmp(uri, LV2_WORKER__interface) ? &echo_interface : NULL;
}

static const LV2_Descriptor descriptor_echo
    = {"urn:score:test:worker-echo", instantiate_echo, connect_port_echo, NULL,
       run_echo,                     NULL,             cleanup,           extension_data_echo};

/* urn:score:test:mono-notify: a mono effect, run as one instance per channel,
 * whose UI talks to it on a MIDI input and on an atom input that is not
 * MIDI. It answers the first event of each block, on either input, with one
 * NOTIFY_SIZE-byte event on its output, and counts what it is given.
 *
 * Ports:
 *   0  audio in   "in"
 *   1  audio out  "out"      a copy of "in"
 *   2  atom in    "control"  atom:Sequence, supports midi:MidiEvent
 *   3  atom in    "atoms"    atom:Sequence
 *   4  atom out   "notify"   atom:Sequence
 *   5  control out "notes_off"  CC 120 and 123 received on "control", in all
 *   6  control out "received"   events received on "atoms", in all
 */
typedef struct
{
  const float* in;
  float* out;
  const LV2_Atom_Sequence* control;
  const LV2_Atom_Sequence* atoms;
  LV2_Atom_Sequence* notify;
  float* notes_off;
  float* received;
  LV2_URID midi_event;
  float notes_off_count;
  float received_count;
} test_mono_notify;

static LV2_Handle instantiate_mono_notify(
    const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features)
{
  (void)descriptor;
  (void)rate;
  (void)bundle_path;
  test_mono_notify* self = (test_mono_notify*)calloc(1, sizeof(test_mono_notify));
  for(int i = 0; features && features[i]; i++)
    if(!strcmp(features[i]->URI, LV2_URID__map))
    {
      LV2_URID_Map* map = (LV2_URID_Map*)features[i]->data;
      self->midi_event = map->map(map->handle, LV2_MIDI__MidiEvent);
    }
  return self;
}

static void connect_port_mono_notify(LV2_Handle instance, uint32_t port, void* data)
{
  test_mono_notify* self = (test_mono_notify*)instance;
  switch(port)
  {
    case 0: self->in = (const float*)data; break;
    case 1: self->out = (float*)data; break;
    case 2: self->control = (const LV2_Atom_Sequence*)data; break;
    case 3: self->atoms = (const LV2_Atom_Sequence*)data; break;
    case 4: self->notify = (LV2_Atom_Sequence*)data; break;
    case 5: self->notes_off = (float*)data; break;
    case 6: self->received = (float*)data; break;
    default: break;
  }
}

static void run_mono_notify(LV2_Handle instance, uint32_t n_samples)
{
  test_mono_notify* self = (test_mono_notify*)instance;
  if(!self->in || !self->out || !self->control || !self->atoms || !self->notify)
    return;
  memcpy(self->out, self->in, n_samples * sizeof(float));

  int answer = 0;
  LV2_ATOM_SEQUENCE_FOREACH(self->control, ev)
  {
    answer = 1;
    const uint8_t* msg = (const uint8_t*)(ev + 1);
    if(ev->body.type == self->midi_event && ev->body.size == 3
       && (msg[0] & 0xF0) == 0xB0 && (msg[1] == 120 || msg[1] == 123))
      self->notes_off_count += 1.f;
  }
  LV2_ATOM_SEQUENCE_FOREACH(self->atoms, ev)
  {
    answer = 1;
    self->received_count += 1.f;
  }
  if(self->notes_off)
    *self->notes_off = self->notes_off_count;
  if(self->received)
    *self->received = self->received_count;

  const uint32_t capacity = self->notify->atom.size;
  self->notify->atom.type = self->atoms->atom.type; /* atom:Sequence */
  self->notify->atom.size = sizeof(LV2_Atom_Sequence_Body);
  self->notify->body.unit = 0;
  self->notify->body.pad = 0;
  const uint32_t needed = sizeof(LV2_Atom_Sequence_Body) + sizeof(LV2_Atom_Event)
                          + lv2_atom_pad_size(NOTIFY_SIZE);
  if(!answer || needed > capacity)
    return;
  LV2_Atom_Event* out
      = lv2_atom_sequence_end(&self->notify->body, self->notify->atom.size);
  out->time.frames = 0;
  out->body.type = self->midi_event + 1; /* any type that is not MIDI */
  out->body.size = NOTIFY_SIZE;
  memset(out + 1, 0x5a, NOTIFY_SIZE);
  self->notify->atom.size += sizeof(LV2_Atom_Event) + lv2_atom_pad_size(NOTIFY_SIZE);
}

static const LV2_Descriptor descriptor_mono_notify
    = {"urn:score:test:mono-notify", instantiate_mono_notify, connect_port_mono_notify,
       NULL,                         run_mono_notify,         NULL,
       cleanup,                      NULL};

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
    case 6:
      return &descriptor_rate;
    case 7:
      return &descriptor_thru;
    case 8:
      return &descriptor_handover;
    case 9:
      return &descriptor_mute;
    case 10:
      return &descriptor_worker;
    case 11:
      return &descriptor_atom_notify;
    case 12:
      return &descriptor_x11ui;
    case 13:
      return &descriptor_echo;
    case 14:
      return &descriptor_mono_notify;
    default:
      return NULL;
  }
}
