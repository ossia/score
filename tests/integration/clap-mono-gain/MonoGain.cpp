// A mono CLAP effect, which score runs as one instance per channel: its
// output is its input times its gain. Its first parameter is a read-only
// level, so that the gain is the second parameter but the first control.
//
// It counts what the host does that it should not, for clap_mono_gain_report:
//  - a main-thread function called on another thread, as thread-check says
//  - a parameter gesture sent to it: gestures go from the plug-in to the host
//  - a cookie that is not this instance's: the gain's points into it
//  - an event whose size is not its type's
// clap_mono_gain_set_from_ui() moves the gain of the first instance as its
// editor would: it sends the new value from its next process call.

#include <clap/clap.h>

#include <atomic>
#include <cstring>

namespace
{
const char* const features[]
    = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_MONO, nullptr};

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

constexpr clap_id level_param = 9;
constexpr clap_id gain_param = 3;

struct instance
{
  clap_plugin_t plugin{};
  const clap_host_t* host{};
  const clap_host_thread_check_t* thread_check{};
  //! Where the cookie of the gain points
  double gain{0.5};
};

//! The oldest living instance, the one whose editor is shown
std::atomic<instance*> g_first{};
std::atomic<bool> g_ui_pending{};
std::atomic<double> g_ui_value{};
std::atomic<int> g_wrong_thread{}, g_gestures{}, g_foreign_cookies{}, g_bad_events{};

instance& self(const clap_plugin_t* p)
{
  return *static_cast<instance*>(p->plugin_data);
}

void main_thread_only(const clap_plugin_t* p)
{
  auto& s = self(p);
  if(s.thread_check && !s.thread_check->is_main_thread(s.host))
    g_wrong_thread++;
}

uint32_t audio_count(const clap_plugin_t*, bool)
{
  return 1;
}
bool audio_get(
    const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info)
{
  if(index > 0)
    return false;
  info->id = 0;
  std::strcpy(info->name, is_input ? "In" : "Out");
  info->channel_count = 1;
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->port_type = CLAP_PORT_MONO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}
const clap_plugin_audio_ports_t audio_ports = {audio_count, audio_get};

uint32_t params_count(const clap_plugin_t* p)
{
  main_thread_only(p);
  return 2;
}
bool params_info(const clap_plugin_t* p, uint32_t index, clap_param_info_t* info)
{
  main_thread_only(p);
  if(index > 1)
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
  if(id == level_param)
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
        if(h->size < sizeof(clap_event_header_t))
          g_bad_events++;
        break;
    }
  }
}

void params_flush(
    const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t*)
{
  apply(self(p), in);
}
const clap_plugin_params_t params
    = {params_count,   params_info,      params_value,
       params_to_text, params_from_text, params_flush};

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
  instance* expected = s;
  g_first.compare_exchange_strong(expected, nullptr);
  delete s;
}
bool activate(const clap_plugin_t*, double, uint32_t, uint32_t)
{
  return true;
}
void deactivate(const clap_plugin_t*) { }
bool start_processing(const clap_plugin_t*)
{
  return true;
}
void stop_processing(const clap_plugin_t*) { }
void reset(const clap_plugin_t*) { }

clap_process_status process(const clap_plugin_t* p, const clap_process_t* process)
{
  auto& s = self(p);
  apply(s, process->in_events);
  if(g_first == &s && g_ui_pending.exchange(false))
  {
    s.gain = g_ui_value;
    clap_event_param_value_t ev{};
    ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    ev.param_id = gain_param;
    ev.cookie = &s.gain;
    ev.note_id = ev.port_index = ev.channel = ev.key = -1;
    ev.value = s.gain;
    process->out_events->try_push(process->out_events, &ev.header);
  }

  auto& in = process->audio_inputs[0];
  auto& out = process->audio_outputs[0];
  for(uint32_t i = 0; i < process->frames_count; i++)
  {
    const double v = in.data32 ? in.data32[0][i] : in.data64[0][i];
    if(out.data32)
      out.data32[0][i] = float(v * s.gain);
    else
      out.data64[0][i] = v * s.gain;
  }
  return CLAP_PROCESS_CONTINUE;
}

const void* get_extension(const clap_plugin_t*, const char* id)
{
  if(!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
    return &audio_ports;
  if(!std::strcmp(id, CLAP_EXT_PARAMS))
    return &params;
  return nullptr;
}
void on_main_thread(const clap_plugin_t*) { }

uint32_t factory_count(const clap_plugin_factory_t*)
{
  return 1;
}
const clap_plugin_descriptor_t*
factory_descriptor(const clap_plugin_factory_t*, uint32_t)
{
  return &descriptor;
}
const clap_plugin_t*
factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id)
{
  if(std::strcmp(id, descriptor.id) != 0)
    return nullptr;
  auto* s = new instance;
  s->host = host;
  instance* none = nullptr;
  g_first.compare_exchange_strong(none, s);
  auto& p = s->plugin;
  p.desc = &descriptor;
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
  return true;
}
void entry_deinit() { }
const void* entry_get_factory(const char* id)
{
  return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &factory;
}
}

extern "C" {
CLAP_EXPORT const clap_plugin_entry_t clap_entry
    = {CLAP_VERSION_INIT, entry_init, entry_deinit, entry_get_factory};

//! The first instance's editor sets its gain
CLAP_EXPORT void clap_mono_gain_set_from_ui(double v)
{
  g_ui_value = v;
  g_ui_pending = true;
}

//! What the host did wrong since the last call
struct clap_mono_gain_errors
{
  int wrong_thread;
  int gestures;
  int foreign_cookies;
  int bad_events;
};
CLAP_EXPORT clap_mono_gain_errors clap_mono_gain_report()
{
  return {
      g_wrong_thread.exchange(0), g_gestures.exchange(0), g_foreign_cookies.exchange(0),
      g_bad_events.exchange(0)};
}
}
