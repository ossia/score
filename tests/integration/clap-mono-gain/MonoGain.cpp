// A mono CLAP effect, which score runs as one instance per channel (and a
// stereo one, which score runs as a single instance): its
// output is its input times its gain and its trim. Its parameters are a
// read-only level, a hidden bypass and the gain, so that the gain is the
// third parameter but the first control. The trim is not a parameter: it is
// only in the plug-in's state, as a loaded model or sample would be.
//
// It counts what the host does that it should not, for clap_mono_gain_report:
//  - a main-thread function called on another thread, as thread-check says
//  - a parameter gesture sent to it: gestures go from the plug-in to the host
//  - a cookie that is not this instance's: the gain's points into it
//  - an event whose size is not its type's, or a note or MIDI event, since
//    it has no note port
//  - a call its state does not allow: process or stop_processing while not
//    processing, deactivate while processing, activate while active, flush
//    from the main thread while active, destroy while active
//  - a call into the library while it is not initialised
//  - the entry's init called again before its deinit
// clap_mono_gain_set_from_ui() moves the gain of an instance as its editor
// would: it sends the new value from its next process call.

#include <clap/clap.h>

#include <atomic>
#include <cstring>

namespace
{
const char* const features[]
    = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_MONO, nullptr};

const char* const stereo_features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr};

const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "io.ossia.test.mono-gain",
    .name = "Mono gain",
    .vendor = "ossia",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "Its input times its gain",
    .features = features,
};

//! The same on a stereo port, which score runs as one instance
const clap_plugin_descriptor_t stereo_descriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "io.ossia.test.stereo-gain",
    .name = "Stereo gain",
    .vendor = "ossia",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "Its input times its gain",
    .features = stereo_features,
};

constexpr clap_id level_param = 9;
constexpr clap_id bypass_param = 5;
constexpr clap_id gain_param = 3;

struct instance
{
  clap_plugin_t plugin{};
  const clap_host_t* host{};
  const clap_host_thread_check_t* thread_check{};
  //! Where the cookie of the gain points
  double gain{0.5};
  double trim{1.};
  bool active{};
  bool processing{};
  //! What its editor set, sent from its next process call
  std::atomic<bool> ui_pending{};
  std::atomic<double> ui_value{};
  uint32_t channels{1};
  //! Whether its process calls run tasks in the host's thread pool
  std::atomic<bool> use_pool{};
};

std::atomic<int> g_instances{};
std::atomic<int> g_wrong_thread{}, g_gestures{}, g_foreign_cookies{}, g_bad_events{},
    g_bad_state{}, g_double_init{}, g_most_params{}, g_activations{}, g_late_calls{};
std::atomic<int> g_initialised{};
std::atomic<int> g_active{};
std::atomic<double> g_last_rate{};

instance& self(const clap_plugin_t* p)
{
  if(g_initialised.load() <= 0)
    g_late_calls++;
  return *static_cast<instance*>(p->plugin_data);
}

void main_thread_only(const clap_plugin_t* p)
{
  auto& s = self(p);
  if(s.thread_check && !s.thread_check->is_main_thread(s.host))
    g_wrong_thread++;
}

uint32_t audio_count(const clap_plugin_t* p, bool)
{
  main_thread_only(p);
  return 1;
}
bool audio_get(
    const clap_plugin_t* p, uint32_t index, bool is_input, clap_audio_port_info_t* info)
{
  main_thread_only(p);
  if(index > 0)
    return false;
  const auto channels = self(p).channels;
  info->id = 0;
  std::strcpy(info->name, is_input ? "In" : "Out");
  info->channel_count = channels;
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->port_type = channels == 1 ? CLAP_PORT_MONO : CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}
const clap_plugin_audio_ports_t audio_ports = {audio_count, audio_get};

uint32_t params_count(const clap_plugin_t* p)
{
  main_thread_only(p);
  return 3;
}
bool params_info(const clap_plugin_t* p, uint32_t index, clap_param_info_t* info)
{
  main_thread_only(p);
  if(index > 2)
    return false;
  *info = {};
  info->min_value = 0;
  info->max_value = 1;
  if(index == 0)
  {
    info->id = level_param;
    info->flags = CLAP_PARAM_IS_READONLY;
    std::strcpy(info->name, "Level");
  }
  else if(index == 1)
  {
    info->id = bypass_param;
    info->flags = CLAP_PARAM_IS_HIDDEN | CLAP_PARAM_IS_STEPPED;
    std::strcpy(info->name, "Bypass");
  }
  else
  {
    info->id = gain_param;
    info->cookie = &self(p).gain;
    std::strcpy(info->name, "Gain");
    info->default_value = 0.5;
  }
  return true;
}
bool params_value(const clap_plugin_t* p, clap_id id, double* value)
{
  main_thread_only(p);
  if(id == level_param || id == bypass_param)
    *value = 0;
  else if(id == gain_param)
    *value = self(p).gain;
  else
    return false;
  return true;
}
bool params_to_text(const clap_plugin_t*, clap_id, double, char* out, uint32_t size)
{
  if(size > 0)
    out[0] = 0;
  return true;
}
bool params_from_text(const clap_plugin_t*, clap_id, const char*, double*)
{
  return false;
}

void apply(instance& s, const clap_input_events_t* in)
{
  int params = 0;
  for(uint32_t i = 0, n = in->size(in); i < n; i++)
  {
    auto* h = in->get(in, i);
    if(h->space_id != CLAP_CORE_EVENT_SPACE_ID)
      continue;
    switch(h->type)
    {
      case CLAP_EVENT_PARAM_GESTURE_BEGIN:
      case CLAP_EVENT_PARAM_GESTURE_END:
        g_gestures++;
        if(h->size != sizeof(clap_event_param_gesture_t))
          g_bad_events++;
        break;
      case CLAP_EVENT_PARAM_VALUE: {
        if(h->size != sizeof(clap_event_param_value_t))
        {
          g_bad_events++;
          break;
        }
        params++;
        auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
        if(e->param_id != gain_param)
          break;
        if(e->cookie && e->cookie != &s.gain)
          g_foreign_cookies++;
        else
          s.gain = e->value;
        break;
      }
      default:
        // Notes and MIDI as well: it has no note port to receive them on
        g_bad_events++;
        break;
    }
  }
  for(int prev = g_most_params; params > prev;)
    if(g_most_params.compare_exchange_weak(prev, params))
      break;
}

void params_flush(
    const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t*)
{
  // [active ? audio-thread : main-thread]
  auto& s = self(p);
  if(s.thread_check && s.active == s.thread_check->is_main_thread(s.host))
    g_bad_state++;
  apply(s, in);
}
const clap_plugin_params_t params
    = {params_count,   params_info,      params_value,
       params_to_text, params_from_text, params_flush};

bool state_save(const clap_plugin_t* p, const clap_ostream_t* stream)
{
  main_thread_only(p);
  const double v[2]{self(p).gain, self(p).trim};
  return stream->write(stream, v, sizeof(v)) == sizeof(v);
}
bool state_load(const clap_plugin_t* p, const clap_istream_t* stream)
{
  main_thread_only(p);
  double v[2]{};
  auto* bytes = reinterpret_cast<char*>(v);
  int64_t got = 0;
  while(got < int64_t(sizeof(v)))
  {
    const int64_t n = stream->read(stream, bytes + got, sizeof(v) - got);
    if(n <= 0)
      return false;
    got += n;
  }
  self(p).gain = v[0];
  self(p).trim = v[1];
  return true;
}
const clap_plugin_state_t state = {state_save, state_load};

bool init(const clap_plugin_t* p)
{
  auto& s = self(p);
  s.thread_check = static_cast<const clap_host_thread_check_t*>(
      s.host->get_extension(s.host, CLAP_EXT_THREAD_CHECK));
  return true;
}
void destroy(const clap_plugin_t* p)
{
  auto* s = &self(p);
  if(s->active)
    g_bad_state++;
  g_instances--;
  delete s;
}
bool activate(const clap_plugin_t* p, double rate, uint32_t, uint32_t)
{
  main_thread_only(p);
  auto& s = self(p);
  if(s.active)
    g_bad_state++;
  s.active = true;
  g_active++;
  g_activations++;
  g_last_rate = rate;
  return true;
}
void deactivate(const clap_plugin_t* p)
{
  main_thread_only(p);
  auto& s = self(p);
  if(!s.active || s.processing)
    g_bad_state++;
  if(s.active)
    g_active--;
  s.active = false;
}
bool start_processing(const clap_plugin_t* p)
{
  auto& s = self(p);
  if(!s.active || s.processing)
    g_bad_state++;
  s.processing = true;
  return true;
}
void stop_processing(const clap_plugin_t* p)
{
  auto& s = self(p);
  if(!s.active || !s.processing)
    g_bad_state++;
  s.processing = false;
}
void reset(const clap_plugin_t* p)
{
  if(!self(p).active)
    g_bad_state++;
}

clap_process_status process(const clap_plugin_t* p, const clap_process_t* process)
{
  auto& s = self(p);
  if(!s.processing)
    g_bad_state++;
  apply(s, process->in_events);
  if(s.ui_pending.exchange(false))
  {
    s.gain = s.ui_value;
    clap_event_param_value_t ev{};
    ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    ev.param_id = gain_param;
    ev.cookie = &s.gain;
    ev.note_id = ev.port_index = ev.channel = ev.key = -1;
    ev.value = s.gain;
    process->out_events->try_push(process->out_events, &ev.header);
  }

  if(s.use_pool)
  {
    if(auto* pool = static_cast<const clap_host_thread_pool_t*>(
           s.host->get_extension(s.host, CLAP_EXT_THREAD_POOL)))
      pool->request_exec(s.host, 2);
  }

  auto& in = process->audio_inputs[0];
  auto& out = process->audio_outputs[0];
  const double k = s.gain * s.trim;
  for(uint32_t c = 0; c < s.channels; c++)
  {
    for(uint32_t i = 0; i < process->frames_count; i++)
    {
      const double v = in.data32 ? in.data32[c][i] : in.data64[c][i];
      if(out.data32)
        out.data32[c][i] = float(v * k);
      else
        out.data64[c][i] = v * k;
    }
  }
  return CLAP_PROCESS_CONTINUE;
}

std::atomic<int> g_pool_tasks{};
void pool_exec(const clap_plugin_t*, uint32_t)
{
  g_pool_tasks++;
}
const clap_plugin_thread_pool_t thread_pool = {pool_exec};

// [thread-safe] in clap/plugin.h, but clap-helpers before 1.2 has it
// [main-thread] and terminates the host that does otherwise: Odin2 and the
// plug-ins of clap-juce-extensions of that time
const void* get_extension(const clap_plugin_t* p, const char* id)
{
  main_thread_only(p);
  if(!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
    return &audio_ports;
  if(!std::strcmp(id, CLAP_EXT_PARAMS))
    return &params;
  if(!std::strcmp(id, CLAP_EXT_STATE))
    return &state;
  if(!std::strcmp(id, CLAP_EXT_THREAD_POOL))
    return &thread_pool;
  return nullptr;
}
void on_main_thread(const clap_plugin_t*) { }

uint32_t factory_count(const clap_plugin_factory_t*)
{
  return 2;
}
const clap_plugin_descriptor_t*
factory_descriptor(const clap_plugin_factory_t*, uint32_t index)
{
  return index == 0 ? &descriptor : index == 1 ? &stereo_descriptor : nullptr;
}
const clap_plugin_t*
factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id)
{
  if(g_initialised.load() <= 0)
    g_late_calls++;
  const bool stereo = !std::strcmp(id, stereo_descriptor.id);
  if(!stereo && std::strcmp(id, descriptor.id) != 0)
    return nullptr;
  auto* s = new instance;
  s->host = host;
  s->channels = stereo ? 2 : 1;
  g_instances++;
  auto& p = s->plugin;
  p.desc = stereo ? &stereo_descriptor : &descriptor;
  p.plugin_data = s;
  p.init = init;
  p.destroy = destroy;
  p.activate = activate;
  p.deactivate = deactivate;
  p.start_processing = start_processing;
  p.stop_processing = stop_processing;
  p.reset = reset;
  p.process = process;
  p.get_extension = get_extension;
  p.on_main_thread = on_main_thread;
  return &p;
}
const clap_plugin_factory_t factory
    = {factory_count, factory_descriptor, factory_create};

bool entry_init(const char*)
{
  if(g_initialised++ > 0)
    g_double_init++;
  return true;
}
void entry_deinit()
{
  if(g_initialised-- <= 0)
    g_bad_state++;
}
const void* entry_get_factory(const char* id)
{
#if defined(CLAP_MONO_GAIN_ANY_FACTORY)
  // As some plug-ins do: the plug-in factory, whatever the id asked for
  (void)id;
  return &factory;
#else
  return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &factory;
#endif
}
}

extern "C" {
CLAP_EXPORT const clap_plugin_entry_t clap_entry
    = {CLAP_VERSION_INIT, entry_init, entry_deinit, entry_get_factory};

//! The editor of an instance sets its gain
CLAP_EXPORT void clap_mono_gain_set_from_ui(const clap_plugin_t* p, double v)
{
  auto& s = *static_cast<instance*>(p->plugin_data);
  s.ui_value = v;
  s.ui_pending = true;
}

//! An instance runs tasks in the host's thread pool from its process calls
CLAP_EXPORT void clap_mono_gain_use_thread_pool(const clap_plugin_t* p)
{
  static_cast<instance*>(p->plugin_data)->use_pool = true;
}
CLAP_EXPORT int clap_mono_gain_pool_tasks()
{
  return g_pool_tasks.exchange(0);
}

//! An instance loads something that only its state keeps
CLAP_EXPORT void clap_mono_gain_set_trim(const clap_plugin_t* p, double v)
{
  static_cast<instance*>(p->plugin_data)->trim = v;
}

//! An instance asks its host for a restart, or a rescan of its parameters
CLAP_EXPORT void clap_mono_gain_request_restart(const clap_plugin_t* p)
{
  auto& s = *static_cast<instance*>(p->plugin_data);
  s.host->request_restart(s.host);
}
CLAP_EXPORT void clap_mono_gain_rescan(const clap_plugin_t* p, uint32_t flags)
{
  auto& s = *static_cast<instance*>(p->plugin_data);
  if(auto* hp = static_cast<const clap_host_params_t*>(
         s.host->get_extension(s.host, CLAP_EXT_PARAMS)))
    hp->rescan(s.host, flags);
}
//! An instance asks its host for a flush, as after loading a preset
CLAP_EXPORT void clap_mono_gain_request_flush(const clap_plugin_t* p)
{
  auto& s = *static_cast<instance*>(p->plugin_data);
  if(auto* hp = static_cast<const clap_host_params_t*>(
         s.host->get_extension(s.host, CLAP_EXT_PARAMS)))
    hp->request_flush(s.host);
}

//! What the host did wrong since the last call, and where it left the
//! plug-in
struct clap_mono_gain_errors
{
  int wrong_thread;
  int gestures;
  int foreign_cookies;
  int bad_events;
  int bad_state;
  int late_calls;
  int double_init;
  //! The most parameter values one call received
  int most_params;
  int activations;
  //! Instances now living and active, initialisations not yet ended
  int instances;
  int active;
  int initialised;
  double last_rate;
};
CLAP_EXPORT clap_mono_gain_errors clap_mono_gain_report()
{
  return {
      g_wrong_thread.exchange(0), g_gestures.exchange(0), g_foreign_cookies.exchange(0),
      g_bad_events.exchange(0),   g_bad_state.exchange(0), g_late_calls.exchange(0),
      g_double_init.exchange(0),
      g_most_params.exchange(0),  g_activations.exchange(0), g_instances.load(),
      g_active.load(),
      g_initialised.load(),       g_last_rate.load()};
}
}
