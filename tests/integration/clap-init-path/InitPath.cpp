// A CLAP effect that does nothing and keeps the path its entry's init() was
// given, for clap_init_path_seen().

#include <clap/clap.h>

#include <cstring>
#include <string>

namespace
{
std::string g_init_path;

const char* const features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr};

const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "io.ossia.test.init-path",
    .name = "Init path",
    .vendor = "ossia",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "Keeps the path its entry was initialised with",
    .features = features,
};

bool plugin_init(const clap_plugin_t*)
{
  return true;
}
void plugin_destroy(const clap_plugin_t* p)
{
  delete p;
}
bool plugin_activate(const clap_plugin_t*, double, uint32_t, uint32_t)
{
  return true;
}
void plugin_deactivate(const clap_plugin_t*) { }
bool plugin_start(const clap_plugin_t*)
{
  return true;
}
void plugin_stop(const clap_plugin_t*) { }
void plugin_reset(const clap_plugin_t*) { }
clap_process_status plugin_process(const clap_plugin_t*, const clap_process_t*)
{
  return CLAP_PROCESS_CONTINUE;
}
const void* plugin_get_extension(const clap_plugin_t*, const char*)
{
  return nullptr;
}
void plugin_on_main_thread(const clap_plugin_t*) { }

uint32_t factory_count(const clap_plugin_factory_t*)
{
  return 1;
}
const clap_plugin_descriptor_t* factory_descriptor(const clap_plugin_factory_t*, uint32_t i)
{
  return i == 0 ? &descriptor : nullptr;
}
const clap_plugin_t*
factory_create(const clap_plugin_factory_t*, const clap_host_t*, const char* id)
{
  if(std::strcmp(id, descriptor.id))
    return nullptr;
  return new clap_plugin_t{
      .desc = &descriptor,
      .plugin_data = nullptr,
      .init = plugin_init,
      .destroy = plugin_destroy,
      .activate = plugin_activate,
      .deactivate = plugin_deactivate,
      .start_processing = plugin_start,
      .stop_processing = plugin_stop,
      .reset = plugin_reset,
      .process = plugin_process,
      .get_extension = plugin_get_extension,
      .on_main_thread = plugin_on_main_thread,
  };
}
const clap_plugin_factory_t factory{factory_count, factory_descriptor, factory_create};

bool entry_init(const char* path)
{
  g_init_path = path ? path : "";
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

extern "C" CLAP_EXPORT const char* clap_init_path_seen()
{
  return g_init_path.c_str();
}
