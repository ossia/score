// A CLAP instrument that shows the host's process calls in its output: the
// left channel holds, at each frame, how many frames the call computing it
// had; the right one the key of each note received, at its frame.

#include <clap/clap.h>

#include <algorithm>
#include <cstring>

namespace
{
const char* const features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, nullptr};

const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "io.ossia.test.block-recorder",
    .name = "Block recorder",
    .vendor = "ossia",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "Writes its process calls to its output",
    .features = features,
};

uint32_t audio_count(const clap_plugin_t*, bool is_input)
{
  return is_input ? 0 : 1;
}
bool audio_get(const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info)
{
  if(is_input || index > 0)
    return false;
  info->id = 0;
  std::strcpy(info->name, "Out");
  info->channel_count = 2;
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}
const clap_plugin_audio_ports_t audio_ports = {audio_count, audio_get};

uint32_t note_count(const clap_plugin_t*, bool is_input)
{
  return is_input ? 1 : 0;
}
bool note_get(const clap_plugin_t*, uint32_t index, bool is_input, clap_note_port_info_t* info)
{
  if(!is_input || index > 0)
    return false;
  info->id = 0;
  info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  std::strcpy(info->name, "Notes");
  return true;
}
const clap_plugin_note_ports_t note_ports = {note_count, note_get};

bool init(const clap_plugin_t*)
{
  return true;
}
void destroy(const clap_plugin_t* p)
{
  delete p;
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

void mark(const clap_process_t* process, uint32_t time, int key)
{
  auto& out = process->audio_outputs[0];
  if(time < process->frames_count)
  {
    if(out.data32)
      out.data32[1][time] = float(key);
    else if(out.data64)
      out.data64[1][time] = double(key);
  }
}

clap_process_status process(const clap_plugin_t*, const clap_process_t* process)
{
  if(process->audio_outputs_count < 1)
    return CLAP_PROCESS_ERROR;
  auto& out = process->audio_outputs[0];
  const uint32_t n = process->frames_count;
  if(out.data32)
  {
    std::fill_n(out.data32[0], n, float(n));
    std::fill_n(out.data32[1], n, 0.f);
  }
  else if(out.data64)
  {
    std::fill_n(out.data64[0], n, double(n));
    std::fill_n(out.data64[1], n, 0.);
  }

  const uint32_t events = process->in_events->size(process->in_events);
  for(uint32_t i = 0; i < events; i++)
  {
    auto* h = process->in_events->get(process->in_events, i);
    if(h->space_id != CLAP_CORE_EVENT_SPACE_ID)
      continue;
    if(h->type == CLAP_EVENT_NOTE_ON)
      mark(process, h->time, reinterpret_cast<const clap_event_note_t*>(h)->key);
    else if(h->type == CLAP_EVENT_MIDI)
    {
      auto* m = reinterpret_cast<const clap_event_midi_t*>(h);
      if((m->data[0] & 0xF0) == 0x90 && m->data[2] > 0)
        mark(process, h->time, m->data[1]);
    }
  }
  return CLAP_PROCESS_CONTINUE;
}

const void* get_extension(const clap_plugin_t*, const char* id)
{
  if(!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
    return &audio_ports;
  if(!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
    return &note_ports;
  return nullptr;
}
void on_main_thread(const clap_plugin_t*) { }

const clap_plugin_t* create(const clap_plugin_factory_t*, const clap_host_t*, const char* id)
{
  if(std::strcmp(id, descriptor.id))
    return nullptr;
  return new clap_plugin_t{
      .desc = &descriptor,
      .plugin_data = nullptr,
      .init = init,
      .destroy = destroy,
      .activate = activate,
      .deactivate = deactivate,
      .start_processing = start_processing,
      .stop_processing = stop_processing,
      .reset = reset,
      .process = process,
      .get_extension = get_extension,
      .on_main_thread = on_main_thread,
  };
}
uint32_t count(const clap_plugin_factory_t*)
{
  return 1;
}
const clap_plugin_descriptor_t* get_descriptor(const clap_plugin_factory_t*, uint32_t i)
{
  return i == 0 ? &descriptor : nullptr;
}
const clap_plugin_factory_t factory = {count, get_descriptor, create};

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

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry
    = {CLAP_VERSION_INIT, entry_init, entry_deinit, entry_get_factory};
