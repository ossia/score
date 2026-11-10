// A CLAP plug-in that is another one: it loads the .clap named by
// clap_checker_set_target() (or $CLAP_CHECKER_TARGET), offers the same
// plug-ins, forwards every call to them, and checks on the way that the host
// calls them as the specification says it may: on the right thread, in the
// right state, with blocks and events the plug-in can take. The sweep worker
// runs both hosts through it and reads what each did wrong with
// clap_checker_take().
//
// What threads are is the host's own thread-check extension: a host without
// one cannot be checked for them.
//
// Only the extensions below are forwarded; the others (the GUI among them)
// are not offered, as the plug-in would get this object instead of its own.

#include <clap/all.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <dlfcn.h>

#define CHECKER_EXPORT extern "C" __attribute__((visibility("default")))

namespace
{
struct violation
{
  int count{};
  std::string first;
};

std::mutex g_mutex;
std::map<std::string, violation> g_violations;
std::string g_target;

void report(const std::string& code, const std::string& message)
{
  std::lock_guard l{g_mutex};
  auto& v = g_violations[code];
  if(v.count++ == 0)
    v.first = message;
}

struct target_lib
{
  void* lib{};
  const clap_plugin_entry_t* entry{};
  int inits{};
  std::string path;
} g_lib;

enum class th
{
  unknown,
  main,
  audio,
  other
};

const char* name(th t)
{
  switch(t)
  {
    case th::main:
      return "main";
    case th::audio:
      return "audio";
    case th::other:
      return "neither main nor audio";
    default:
      return "unknown";
  }
}

struct wrapper
{
  clap_plugin_t self{};
  const clap_plugin_t* inner{};
  const clap_host_t* host{};
  const clap_host_thread_check_t* tc{};
  std::string id;

  bool inited{};
  bool active{};
  bool processing{};
  std::atomic<bool> in_process{};
  uint32_t min_frames{}, max_frames{};

  std::map<clap_id, clap_param_info_t> params;
  std::vector<clap_audio_port_info_t> audio_ins, audio_outs;
  std::vector<clap_note_port_info_t> note_ins;

  const clap_plugin_params_t* params_ext{};
  const clap_plugin_state_t* state_ext{};
  const clap_plugin_audio_ports_t* audio_ports_ext{};
  const clap_plugin_note_ports_t* note_ports_ext{};
  const clap_plugin_latency_t* latency_ext{};
  const clap_plugin_tail_t* tail_ext{};
  const clap_plugin_timer_support_t* timer_ext{};
  const clap_plugin_posix_fd_support_t* fd_ext{};
  const clap_plugin_thread_pool_t* pool_ext{};
  const clap_plugin_preset_load_t* preset_ext{};
  const clap_plugin_voice_info_t* voice_ext{};
  const clap_plugin_note_name_t* note_name_ext{};
  const clap_plugin_render_t* render_ext{};

  th thread() const
  {
    if(!tc || !tc->is_main_thread || !tc->is_audio_thread)
      return th::unknown;
    if(tc->is_main_thread(host))
      return th::main;
    if(tc->is_audio_thread(host))
      return th::audio;
    return th::other;
  }

  void on(th expected, const char* fn)
  {
    const auto t = thread();
    if(t == th::unknown || t == expected)
      return;
    report(
        std::string("thread:") + fn,
        std::string(fn) + " is [" + name(expected) + "-thread], called on the "
            + name(t) + " thread");
  }

  void require(bool cond, const char* code, const std::string& message)
  {
    if(!cond)
      report(code, message);
  }

  void cache_params()
  {
    params.clear();
    if(!params_ext || !params_ext->count || !params_ext->get_info)
      return;
    const uint32_t n = params_ext->count(inner);
    for(uint32_t i = 0; i < n; i++)
    {
      clap_param_info_t info{};
      if(params_ext->get_info(inner, i, &info))
        params[info.id] = info;
    }
  }

  void cache_ports()
  {
    audio_ins.clear();
    audio_outs.clear();
    note_ins.clear();
    if(audio_ports_ext)
    {
      for(int dir = 0; dir < 2; dir++)
      {
        const uint32_t n = audio_ports_ext->count(inner, dir == 0);
        for(uint32_t i = 0; i < n; i++)
        {
          clap_audio_port_info_t info{};
          if(audio_ports_ext->get(inner, i, dir == 0, &info))
            (dir == 0 ? audio_ins : audio_outs).push_back(info);
        }
      }
    }
    if(note_ports_ext)
    {
      const uint32_t n = note_ports_ext->count(inner, true);
      for(uint32_t i = 0; i < n; i++)
      {
        clap_note_port_info_t info{};
        if(note_ports_ext->get(inner, i, true, &info))
          note_ins.push_back(info);
      }
    }
  }

  //! The events of a list given to process() or flush(); frames is 0 for flush
  void check_events(const clap_input_events_t* in, uint32_t frames, const char* where)
  {
    if(!in || !in->size || !in->get)
    {
      report(std::string("events-null:") + where, std::string(where) + ": no input event list");
      return;
    }
    const uint32_t n = in->size(in);
    uint32_t last = 0;
    for(uint32_t i = 0; i < n; i++)
    {
      const clap_event_header_t* h = in->get(in, i);
      if(!h)
      {
        report("event-null", std::string(where) + ": in_events->get returned null");
        continue;
      }
      if(h->size < sizeof(clap_event_header_t))
        report("event-size", std::string(where) + ": event smaller than its header");
      if(frames > 0)
      {
        if(h->time < last)
          report(
              "events-order",
              std::string(where) + ": event " + std::to_string(i) + " at "
                  + std::to_string(h->time) + " after one at " + std::to_string(last));
        if(h->time >= frames)
          report(
              "event-time",
              std::string(where) + ": event at " + std::to_string(h->time)
                  + " in a block of " + std::to_string(frames));
      }
      last = std::max(last, h->time);
      if(h->space_id != CLAP_CORE_EVENT_SPACE_ID)
        continue;
      switch(h->type)
      {
        case CLAP_EVENT_PARAM_VALUE:
        case CLAP_EVENT_PARAM_MOD: {
          const bool mod = h->type == CLAP_EVENT_PARAM_MOD;
          const auto sz = mod ? sizeof(clap_event_param_mod_t) : sizeof(clap_event_param_value_t);
          if(h->size < sz)
          {
            report("event-size", std::string(where) + ": short parameter event");
            break;
          }
          clap_id pid;
          double v;
          void* cookie;
          if(mod)
          {
            auto* e = reinterpret_cast<const clap_event_param_mod_t*>(h);
            pid = e->param_id;
            v = e->amount;
            cookie = e->cookie;
          }
          else
          {
            auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
            pid = e->param_id;
            v = e->value;
            cookie = e->cookie;
          }
          // A cookie is the one this instance gave, or none
          if(auto it = params.find(pid);
             cookie && it != params.end() && cookie != it->second.cookie)
            report(
                "param-cookie",
                std::string(where) + ": parameter " + std::to_string(pid)
                    + " with a cookie that is not this instance's");
          auto it = params.find(pid);
          if(it == params.end())
          {
            if(!params.empty())
              report(
                  "param-id",
                  std::string(where) + ": parameter id " + std::to_string(pid)
                      + " that the plug-in does not have");
            break;
          }
          if(!std::isfinite(v))
            report("param-nonfinite", std::string(where) + ": non-finite parameter value");
          else if(!mod)
          {
            const auto& info = it->second;
            const double eps = 1e-6 * std::max(1., std::abs(info.max_value - info.min_value));
            if(v < info.min_value - eps || v > info.max_value + eps)
              report(
                  "param-range",
                  std::string(where) + ": parameter " + std::to_string(pid) + " set to "
                      + std::to_string(v) + " outside [" + std::to_string(info.min_value)
                      + ", " + std::to_string(info.max_value) + "]");
          }
          break;
        }
        case CLAP_EVENT_PARAM_GESTURE_BEGIN:
        case CLAP_EVENT_PARAM_GESTURE_END:
          // clap/ext/params.h: a knob turned on the host's interface is a
          // value; gestures are the plug-in telling the host about its own
          report(
              "event-gesture",
              std::string(where) + ": a parameter gesture event from the host");
          break;
        case CLAP_EVENT_NOTE_ON:
        case CLAP_EVENT_NOTE_OFF:
        case CLAP_EVENT_NOTE_CHOKE:
        case CLAP_EVENT_NOTE_END:
          if(h->size >= sizeof(clap_event_note_t))
          {
            auto* e = reinterpret_cast<const clap_event_note_t*>(h);
            check_note_port(e->port_index, CLAP_NOTE_DIALECT_CLAP, where, "a CLAP note event");
            if(h->type == CLAP_EVENT_NOTE_ON && (e->key < 0 || e->key > 127 || e->channel < 0 || e->channel > 15))
              report("note-on-wildcard", std::string(where) + ": note on without a specific key and channel");
          }
          break;
        case CLAP_EVENT_MIDI:
          if(h->size >= sizeof(clap_event_midi_t))
            check_note_port(
                reinterpret_cast<const clap_event_midi_t*>(h)->port_index,
                CLAP_NOTE_DIALECT_MIDI, where, "a MIDI event");
          break;
        case CLAP_EVENT_MIDI_SYSEX:
          if(h->size >= sizeof(clap_event_midi_sysex_t))
            check_note_port(
                reinterpret_cast<const clap_event_midi_sysex_t*>(h)->port_index,
                CLAP_NOTE_DIALECT_MIDI, where, "a MIDI sysex event");
          break;
        case CLAP_EVENT_MIDI2:
          if(h->size >= sizeof(clap_event_midi2_t))
            check_note_port(
                reinterpret_cast<const clap_event_midi2_t*>(h)->port_index,
                CLAP_NOTE_DIALECT_MIDI2, where, "a MIDI 2 event");
          break;
        default:
          break;
      }
    }
  }

  void check_note_port(int port, uint32_t dialect, const char* where, const char* what)
  {
    if(port < 0 || std::size_t(port) >= note_ins.size())
    {
      report(
          "event-note-port",
          std::string(where) + ": " + what + " on note port " + std::to_string(port)
              + ", the plug-in has " + std::to_string(note_ins.size()));
      return;
    }
    if(!(note_ins[port].supported_dialects & dialect))
      report(
          "event-dialect",
          std::string(where) + ": " + what + " on note port " + std::to_string(port)
              + ", which does not support that dialect");
  }

  void check_audio(const clap_process_t* p)
  {
    auto side = [&](const clap_audio_buffer_t* bufs, uint32_t count,
                    const std::vector<clap_audio_port_info_t>& ports, const char* dir) {
      if(count != ports.size())
      {
        report(
            std::string("audio-count:") + dir,
            std::string("process: ") + std::to_string(count) + " " + dir
                + " buffers, the plug-in has " + std::to_string(ports.size())
                + " ports");
        return;
      }
      if(count > 0 && !bufs)
      {
        report("audio-null", std::string("process: null ") + dir + " buffer list");
        return;
      }
      for(uint32_t i = 0; i < count; i++)
      {
        const auto& b = bufs[i];
        if(b.channel_count != ports[i].channel_count)
          report(
              std::string("audio-channels:") + dir,
              std::string("process: ") + dir + " port " + std::to_string(i) + " given "
                  + std::to_string(b.channel_count) + " channels, it has "
                  + std::to_string(ports[i].channel_count));
        if(b.channel_count == 0)
          continue;
        if(!b.data32 && !b.data64)
        {
          report(
              "audio-null",
              std::string("process: ") + dir + " port " + std::to_string(i)
                  + " without buffers");
          continue;
        }
        if(b.data64 && !(ports[i].flags & CLAP_AUDIO_PORT_SUPPORTS_64BITS))
          report(
              "audio-64",
              std::string("process: 64-bit buffers on ") + dir + " port "
                  + std::to_string(i) + ", which does not support them");
        const uint32_t n = std::min(b.channel_count, ports[i].channel_count);
        for(uint32_t c = 0; c < n; c++)
          if(b.data32 ? !b.data32[c] : !b.data64[c])
            report(
                "audio-null",
                std::string("process: null channel on ") + dir + " port "
                    + std::to_string(i));
      }
    };
    side(p->audio_inputs, p->audio_inputs_count, audio_ins, "input");
    side(p->audio_outputs, p->audio_outputs_count, audio_outs, "output");
  }
};

wrapper* W(const clap_plugin_t* p)
{
  return static_cast<wrapper*>(p->plugin_data);
}

// clap.params
uint32_t params_count(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "params.count");
  return w->params_ext->count(w->inner);
}
bool params_get_info(const clap_plugin_t* p, uint32_t i, clap_param_info_t* info)
{
  auto* w = W(p);
  w->on(th::main, "params.get_info");
  return w->params_ext->get_info(w->inner, i, info);
}
bool params_get_value(const clap_plugin_t* p, clap_id id, double* v)
{
  auto* w = W(p);
  w->on(th::main, "params.get_value");
  return w->params_ext->get_value(w->inner, id, v);
}
bool params_value_to_text(
    const clap_plugin_t* p, clap_id id, double v, char* out, uint32_t cap)
{
  auto* w = W(p);
  w->on(th::main, "params.value_to_text");
  return w->params_ext->value_to_text
         && w->params_ext->value_to_text(w->inner, id, v, out, cap);
}
bool params_text_to_value(const clap_plugin_t* p, clap_id id, const char* t, double* v)
{
  auto* w = W(p);
  w->on(th::main, "params.text_to_value");
  return w->params_ext->text_to_value
         && w->params_ext->text_to_value(w->inner, id, t, v);
}
void params_flush(
    const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out)
{
  auto* w = W(p);
  w->on(w->active ? th::audio : th::main, "params.flush");
  w->require(!w->in_process.load(), "flush-during-process", "params.flush during process()");
  w->check_events(in, 0, "params.flush");
  w->params_ext->flush(w->inner, in, out);
}
const clap_plugin_params_t params_wrap{
    params_count,         params_get_info,      params_get_value,
    params_value_to_text, params_text_to_value, params_flush};

// clap.state
bool state_save(const clap_plugin_t* p, const clap_ostream_t* s)
{
  auto* w = W(p);
  w->on(th::main, "state.save");
  return w->state_ext->save(w->inner, s);
}
bool state_load(const clap_plugin_t* p, const clap_istream_t* s)
{
  auto* w = W(p);
  w->on(th::main, "state.load");
  return w->state_ext->load(w->inner, s);
}
const clap_plugin_state_t state_wrap{state_save, state_load};

// clap.audio-ports, clap.note-ports
uint32_t audio_ports_count(const clap_plugin_t* p, bool in)
{
  auto* w = W(p);
  w->on(th::main, "audio_ports.count");
  return w->audio_ports_ext->count(w->inner, in);
}
bool audio_ports_get(const clap_plugin_t* p, uint32_t i, bool in, clap_audio_port_info_t* info)
{
  auto* w = W(p);
  w->on(th::main, "audio_ports.get");
  return w->audio_ports_ext->get(w->inner, i, in, info);
}
const clap_plugin_audio_ports_t audio_ports_wrap{audio_ports_count, audio_ports_get};

uint32_t note_ports_count(const clap_plugin_t* p, bool in)
{
  auto* w = W(p);
  w->on(th::main, "note_ports.count");
  return w->note_ports_ext->count(w->inner, in);
}
bool note_ports_get(const clap_plugin_t* p, uint32_t i, bool in, clap_note_port_info_t* info)
{
  auto* w = W(p);
  w->on(th::main, "note_ports.get");
  return w->note_ports_ext->get(w->inner, i, in, info);
}
const clap_plugin_note_ports_t note_ports_wrap{note_ports_count, note_ports_get};

// clap.latency, clap.tail
uint32_t latency_get(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "latency.get");
  return w->latency_ext->get(w->inner);
}
const clap_plugin_latency_t latency_wrap{latency_get};

uint32_t tail_get(const clap_plugin_t* p)
{
  auto* w = W(p);
  const auto t = w->thread();
  if(t == th::other)
    report("thread:tail.get", "tail.get is [main-thread,audio-thread], called on neither");
  return w->tail_ext->get(w->inner);
}
const clap_plugin_tail_t tail_wrap{tail_get};

// clap.timer-support, clap.posix-fd-support, clap.thread-pool
void timer_on_timer(const clap_plugin_t* p, clap_id id)
{
  auto* w = W(p);
  w->on(th::main, "timer_support.on_timer");
  w->timer_ext->on_timer(w->inner, id);
}
const clap_plugin_timer_support_t timer_wrap{timer_on_timer};

void fd_on_fd(const clap_plugin_t* p, int fd, clap_posix_fd_flags_t flags)
{
  auto* w = W(p);
  w->on(th::main, "posix_fd_support.on_fd");
  w->fd_ext->on_fd(w->inner, fd, flags);
}
const clap_plugin_posix_fd_support_t fd_wrap{fd_on_fd};

void pool_exec(const clap_plugin_t* p, uint32_t task)
{
  auto* w = W(p);
  w->pool_ext->exec(w->inner, task);
}
const clap_plugin_thread_pool_t pool_wrap{pool_exec};

// clap.preset-load, clap.voice-info, clap.note-name, clap.render
bool preset_from_location(
    const clap_plugin_t* p, uint32_t kind, const char* location, const char* key)
{
  auto* w = W(p);
  w->on(th::main, "preset_load.from_location");
  return w->preset_ext->from_location(w->inner, kind, location, key);
}
const clap_plugin_preset_load_t preset_wrap{preset_from_location};

bool voice_get(const clap_plugin_t* p, clap_voice_info_t* info)
{
  auto* w = W(p);
  w->on(th::main, "voice_info.get");
  w->require(w->active, "state:voice_info.get", "voice_info.get while not active");
  return w->voice_ext->get(w->inner, info);
}
const clap_plugin_voice_info_t voice_wrap{voice_get};

uint32_t note_name_count(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "note_name.count");
  return w->note_name_ext->count(w->inner);
}
bool note_name_get(const clap_plugin_t* p, uint32_t i, clap_note_name_t* n)
{
  auto* w = W(p);
  w->on(th::main, "note_name.get");
  return w->note_name_ext->get(w->inner, i, n);
}
const clap_plugin_note_name_t note_name_wrap{note_name_count, note_name_get};

bool render_hard_rt(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "render.has_hard_realtime_requirement");
  return w->render_ext->has_hard_realtime_requirement(w->inner);
}
bool render_set(const clap_plugin_t* p, clap_plugin_render_mode mode)
{
  auto* w = W(p);
  w->on(th::main, "render.set");
  return w->render_ext->set(w->inner, mode);
}
const clap_plugin_render_t render_wrap{render_hard_rt, render_set};

template <typename T>
const T* ext(const clap_plugin_t* inner, const char* id, const char* compat = nullptr)
{
  auto* e = static_cast<const T*>(inner->get_extension(inner, id));
  if(!e && compat)
    e = static_cast<const T*>(inner->get_extension(inner, compat));
  return e;
}

// clap_plugin
bool plugin_init(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->tc = static_cast<const clap_host_thread_check_t*>(
      w->host->get_extension(w->host, CLAP_EXT_THREAD_CHECK));
  w->on(th::main, "init");
  w->require(!w->inited, "state:init", "init called twice");
  if(!w->inner->init(w->inner))
    return false;
  w->inited = true;
  auto* in = w->inner;
  w->params_ext = ext<clap_plugin_params_t>(in, CLAP_EXT_PARAMS);
  w->state_ext = ext<clap_plugin_state_t>(in, CLAP_EXT_STATE);
  w->audio_ports_ext = ext<clap_plugin_audio_ports_t>(in, CLAP_EXT_AUDIO_PORTS);
  w->note_ports_ext = ext<clap_plugin_note_ports_t>(in, CLAP_EXT_NOTE_PORTS);
  w->latency_ext = ext<clap_plugin_latency_t>(in, CLAP_EXT_LATENCY);
  w->tail_ext = ext<clap_plugin_tail_t>(in, CLAP_EXT_TAIL);
  w->timer_ext = ext<clap_plugin_timer_support_t>(in, CLAP_EXT_TIMER_SUPPORT);
  w->fd_ext = ext<clap_plugin_posix_fd_support_t>(in, CLAP_EXT_POSIX_FD_SUPPORT);
  w->pool_ext = ext<clap_plugin_thread_pool_t>(in, CLAP_EXT_THREAD_POOL);
  w->preset_ext = ext<clap_plugin_preset_load_t>(
      in, CLAP_EXT_PRESET_LOAD, CLAP_EXT_PRESET_LOAD_COMPAT);
  w->voice_ext = ext<clap_plugin_voice_info_t>(in, CLAP_EXT_VOICE_INFO);
  w->note_name_ext = ext<clap_plugin_note_name_t>(in, CLAP_EXT_NOTE_NAME);
  w->render_ext = ext<clap_plugin_render_t>(in, CLAP_EXT_RENDER);
  w->cache_params();
  return true;
}

void plugin_destroy(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "destroy");
  w->require(!w->active, "state:destroy", "destroy while active");
  w->inner->destroy(w->inner);
  delete w;
}

bool plugin_activate(const clap_plugin_t* p, double rate, uint32_t minf, uint32_t maxf)
{
  auto* w = W(p);
  w->on(th::main, "activate");
  w->require(!w->active, "state:activate", "activate while active");
  w->require(rate > 0 && std::isfinite(rate), "activate-args", "activate with a bad sample rate");
  w->require(minf >= 1 && minf <= maxf, "activate-args",
      "activate with min_frames_count " + std::to_string(minf) + ", max " + std::to_string(maxf));
  w->cache_ports();
  w->cache_params();
  if(!w->inner->activate(w->inner, rate, minf, maxf))
    return false;
  w->active = true;
  w->min_frames = minf;
  w->max_frames = maxf;
  return true;
}

void plugin_deactivate(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "deactivate");
  w->require(w->active, "state:deactivate", "deactivate while not active");
  w->require(!w->processing, "state:deactivate-processing",
      "deactivate while processing: stop_processing was not called");
  w->inner->deactivate(w->inner);
  w->active = false;
  w->processing = false;
}

bool plugin_start(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::audio, "start_processing");
  w->require(w->active, "state:start_processing", "start_processing while not active");
  w->require(!w->processing, "state:start_processing-twice",
      "start_processing while already processing");
  if(!w->inner->start_processing(w->inner))
    return false;
  w->processing = true;
  return true;
}

void plugin_stop(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::audio, "stop_processing");
  w->require(w->active, "state:stop_processing", "stop_processing while not active");
  w->require(w->processing, "state:stop_processing-idle",
      "stop_processing while not processing");
  w->inner->stop_processing(w->inner);
  w->processing = false;
}

void plugin_reset(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::audio, "reset");
  w->require(w->active, "state:reset", "reset while not active");
  w->inner->reset(w->inner);
}

clap_process_status plugin_process(const clap_plugin_t* p, const clap_process_t* pr)
{
  auto* w = W(p);
  w->on(th::audio, "process");
  w->require(w->active && w->processing, "state:process",
      std::string("process while ") + (w->active ? "not processing" : "not active"));
  w->require(!w->in_process.exchange(true), "process-reentrant", "process re-entered");
  if(pr)
  {
    w->require(pr->frames_count >= 1 && pr->frames_count <= w->max_frames, "frames",
        "process with " + std::to_string(pr->frames_count) + " frames, activated for "
            + std::to_string(w->min_frames) + " to " + std::to_string(w->max_frames));
    w->check_audio(pr);
    w->check_events(pr->in_events, pr->frames_count, "process");
    w->require(pr->out_events && pr->out_events->try_push, "events-null:out",
        "process without an output event list");
  }
  const auto res = w->inner->process(w->inner, pr);
  w->in_process = false;
  return res;
}

const void* plugin_get_extension(const clap_plugin_t* p, const char* id)
{
  auto* w = W(p);
  if(!w->inited)
    return nullptr;
  // clap/plugin.h has it [thread-safe], but clap-helpers before 1.2 checks it
  // is called on the main thread and terminates the host otherwise: Odin2,
  // SpectMorph and the plug-ins of clap-juce-extensions of that time
  if(const auto t = w->thread(); t != th::main && t != th::unknown)
    report(
        "thread:get_extension",
        "get_extension called on the " + std::string(name(t))
            + " thread, which clap-helpers before 1.2 forbids");
  const std::string_view s{id};
  if(s == CLAP_EXT_PARAMS)
    return w->params_ext ? &params_wrap : nullptr;
  if(s == CLAP_EXT_STATE)
    return w->state_ext ? &state_wrap : nullptr;
  if(s == CLAP_EXT_AUDIO_PORTS)
    return w->audio_ports_ext ? &audio_ports_wrap : nullptr;
  if(s == CLAP_EXT_NOTE_PORTS)
    return w->note_ports_ext ? &note_ports_wrap : nullptr;
  if(s == CLAP_EXT_LATENCY)
    return w->latency_ext ? &latency_wrap : nullptr;
  if(s == CLAP_EXT_TAIL)
    return w->tail_ext ? &tail_wrap : nullptr;
  if(s == CLAP_EXT_TIMER_SUPPORT)
    return w->timer_ext ? &timer_wrap : nullptr;
  if(s == CLAP_EXT_POSIX_FD_SUPPORT)
    return w->fd_ext ? &fd_wrap : nullptr;
  if(s == CLAP_EXT_THREAD_POOL)
    return w->pool_ext ? &pool_wrap : nullptr;
  if(s == CLAP_EXT_PRESET_LOAD || s == CLAP_EXT_PRESET_LOAD_COMPAT)
    return w->preset_ext ? &preset_wrap : nullptr;
  if(s == CLAP_EXT_VOICE_INFO)
    return w->voice_ext ? &voice_wrap : nullptr;
  if(s == CLAP_EXT_NOTE_NAME)
    return w->note_name_ext ? &note_name_wrap : nullptr;
  if(s == CLAP_EXT_RENDER)
    return w->render_ext ? &render_wrap : nullptr;
  return nullptr;
}

void plugin_on_main_thread(const clap_plugin_t* p)
{
  auto* w = W(p);
  w->on(th::main, "on_main_thread");
  w->inner->on_main_thread(w->inner);
}

// The factory
const clap_plugin_factory_t* inner_factory()
{
  if(!g_lib.entry)
    return nullptr;
  return static_cast<const clap_plugin_factory_t*>(
      g_lib.entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
}

uint32_t factory_count(const clap_plugin_factory_t*)
{
  auto* f = inner_factory();
  return f ? f->get_plugin_count(f) : 0;
}
const clap_plugin_descriptor_t* factory_desc(const clap_plugin_factory_t*, uint32_t i)
{
  auto* f = inner_factory();
  return f ? f->get_plugin_descriptor(f, i) : nullptr;
}
const clap_plugin_t*
factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id)
{
  auto* f = inner_factory();
  if(!f)
    return nullptr;
  if(!host)
    report("create-null-host", "create_plugin with a null host");
  auto* inner = f->create_plugin(f, host, id);
  if(!inner)
    return nullptr;
  auto* w = new wrapper;
  w->inner = inner;
  w->host = host;
  w->id = id ? id : "";
  w->self.desc = inner->desc;
  w->self.plugin_data = w;
  w->self.init = plugin_init;
  w->self.destroy = plugin_destroy;
  w->self.activate = plugin_activate;
  w->self.deactivate = plugin_deactivate;
  w->self.start_processing = plugin_start;
  w->self.stop_processing = plugin_stop;
  w->self.reset = plugin_reset;
  w->self.process = plugin_process;
  w->self.get_extension = plugin_get_extension;
  w->self.on_main_thread = plugin_on_main_thread;
  return &w->self;
}
const clap_plugin_factory_t factory{factory_count, factory_desc, factory_create};

bool entry_init(const char*)
{
  if(g_lib.inits++ > 0)
  {
    report("entry-init-twice", "clap_entry.init called again before deinit");
    return true;
  }
  std::string target;
  {
    std::lock_guard l{g_mutex};
    target = g_target;
  }
  if(target.empty())
    if(const char* e = std::getenv("CLAP_CHECKER_TARGET"))
      target = e;
  if(target.empty())
  {
    g_lib.inits = 0;
    return false;
  }
  // Kept loaded once loaded: plug-ins that do not survive being unloaded and
  // loaded again would otherwise crash the hosts that follow
  if(g_lib.path != target)
  {
    g_lib.lib = dlopen(target.c_str(), RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
    g_lib.entry = g_lib.lib ? static_cast<const clap_plugin_entry_t*>(
                                  dlsym(g_lib.lib, "clap_entry"))
                            : nullptr;
    g_lib.path = g_lib.entry ? target : std::string{};
  }
  // clap/entry.h: on macOS init() takes the bundle, not the binary in it
  std::string init_path = target;
#if defined(__APPLE__)
  if(auto p = target.rfind(".clap/Contents/MacOS/"); p != std::string::npos)
    init_path = target.substr(0, p + 5);
#endif
  if(!g_lib.entry || !g_lib.entry->init(init_path.c_str()))
  {
    g_lib.entry = nullptr;
    g_lib.path.clear();
    g_lib.inits = 0;
    return false;
  }
  return true;
}

void entry_deinit()
{
  if(g_lib.inits <= 0)
  {
    report("entry-deinit-unpaired", "clap_entry.deinit without init");
    return;
  }
  if(--g_lib.inits == 0 && g_lib.entry)
    g_lib.entry->deinit();
}

const void* entry_get_factory(const char* id)
{
  if(g_lib.inits <= 0)
    report("entry-get-factory", "clap_entry.get_factory before init");
  if(std::string_view{id} == CLAP_PLUGIN_FACTORY_ID)
    return &factory;
  // Presets are discovered without plug-in instances: those factories are
  // the target's own. A target that gives its plug-in factory for any id
  // gives the wrapped one, as the host would see it without the checker.
  if(!g_lib.entry)
    return nullptr;
  auto* f = g_lib.entry->get_factory(id);
  if(f && f == g_lib.entry->get_factory(CLAP_PLUGIN_FACTORY_ID))
    return &factory;
  return f;
}
}

CHECKER_EXPORT const clap_plugin_entry_t clap_entry{
    CLAP_VERSION, entry_init, entry_deinit, entry_get_factory};

//! The .clap the next init loads
CHECKER_EXPORT void clap_checker_set_target(const char* path)
{
  std::lock_guard l{g_mutex};
  g_target = path ? path : "";
}

//! What was reported since the last call, as a JSON array, into buf; returns
//! the length it needs
CHECKER_EXPORT int clap_checker_take(char* buf, int cap)
{
  std::lock_guard l{g_mutex};
  std::string s = "[";
  bool first = true;
  for(auto& [code, v] : g_violations)
  {
    std::string msg;
    for(char c : v.first)
    {
      if(c == '"' || c == '\\')
        msg += '\\';
      if(c >= 0x20)
        msg += c;
    }
    s += (first ? "" : ",");
    s += "{\"code\":\"" + code + "\",\"count\":" + std::to_string(v.count)
         + ",\"message\":\"" + msg + "\"}";
    first = false;
  }
  s += "]";
  g_violations.clear();
  if(buf && cap > 0)
  {
    std::snprintf(buf, cap, "%s", s.c_str());
  }
  return int(s.size()) + 1;
}
