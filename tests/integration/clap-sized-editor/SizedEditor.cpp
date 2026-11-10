// Two CLAP plug-ins with an editor that only reports sizes: one whose editor
// has a fixed size, one whose editor can be resized. They draw nothing; the
// host's window is what is checked. clap_sized_editor_size() says the last
// size the host set, clap_sized_editor_scale() the last scale (0 for none),
// clap_sized_editor_off_main_thread() how many calls of the gui extension,
// all [main-thread] in clap/ext/gui.h, the host made from another thread.
// Sizes are in the pixels of the platform's window API: physical on X11,
// points on Cocoa.

#include <clap/clap.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace
{
const char* const features[] = {CLAP_PLUGIN_FEATURE_UTILITY, nullptr};

constexpr const char* fixed_id = "io.ossia.test.fixed-editor";
constexpr const char* resizable_id = "io.ossia.test.resizable-editor";

clap_plugin_descriptor_t make_descriptor(const char* id, const char* name)
{
  return {
      .clap_version = CLAP_VERSION_INIT,
      .id = id,
      .name = name,
      .vendor = "ossia",
      .url = "",
      .manual_url = "",
      .support_url = "",
      .version = "1.0.0",
      .description = "An editor of a known size",
      .features = features,
  };
}
const clap_plugin_descriptor_t descriptors[]
    = {make_descriptor(fixed_id, "Fixed editor"),
       make_descriptor(resizable_id, "Resizable editor")};

struct instance
{
  clap_plugin_t plugin{};
  const clap_host_t* host{};
  std::atomic_int off_main_thread{};
  bool resizable{};
  bool created{};
  uint32_t width{320}, height{200};
  int set_sizes{};
  double scale{};
};

instance& self(const clap_plugin_t* p)
{
  return *static_cast<instance*>(p->plugin_data);
}

//! Counts a call of the gui extension made off the main thread
instance& gui_self(const clap_plugin_t* p)
{
  auto& s = self(p);
  auto* check = static_cast<const clap_host_thread_check_t*>(
      s.host->get_extension(s.host, CLAP_EXT_THREAD_CHECK));
  if(check && !check->is_main_thread(s.host))
    s.off_main_thread++;
  return s;
}

#if defined(__APPLE__)
const char* const window_api = CLAP_WINDOW_API_COCOA;
#else
const char* const window_api = CLAP_WINDOW_API_X11;
#endif

bool gui_is_api_supported(const clap_plugin_t* p, const char* api, bool floating)
{
  gui_self(p);
  return !floating && std::strcmp(api, window_api) == 0;
}
bool gui_get_preferred_api(const clap_plugin_t* p, const char** api, bool* floating)
{
  gui_self(p);
  *api = window_api;
  *floating = false;
  return true;
}
bool gui_create(const clap_plugin_t* p, const char*, bool)
{
  gui_self(p).created = true;
  return true;
}
void gui_destroy(const clap_plugin_t* p)
{
  gui_self(p).created = false;
}
bool gui_set_scale(const clap_plugin_t* p, double scale)
{
  gui_self(p).scale = scale;
  return false;
}
bool gui_get_size(const clap_plugin_t* p, uint32_t* w, uint32_t* h)
{
  *w = gui_self(p).width;
  *h = gui_self(p).height;
  return true;
}
bool gui_can_resize(const clap_plugin_t* p)
{
  return gui_self(p).resizable;
}
bool gui_get_resize_hints(const clap_plugin_t* p, clap_gui_resize_hints_t* hints)
{
  if(!gui_self(p).resizable)
    return false;
  *hints = {true, true, false, 0, 0};
  return true;
}
bool gui_adjust_size(const clap_plugin_t* p, uint32_t* w, uint32_t* h)
{
  if(!gui_self(p).resizable)
    return false;
  *w = std::max<uint32_t>(*w, 100);
  *h = std::max<uint32_t>(*h, 100);
  return true;
}
bool gui_set_size(const clap_plugin_t* p, uint32_t w, uint32_t h)
{
  auto& s = gui_self(p);
  if(!s.resizable)
    return false;
  s.width = w;
  s.height = h;
  s.set_sizes++;
  return true;
}
bool gui_set_parent(const clap_plugin_t* p, const clap_window_t*)
{
  gui_self(p);
  return true;
}
bool gui_set_transient(const clap_plugin_t* p, const clap_window_t*)
{
  gui_self(p);
  return false;
}
void gui_suggest_title(const clap_plugin_t* p, const char*)
{
  gui_self(p);
}
bool gui_show(const clap_plugin_t* p)
{
  gui_self(p);
  return true;
}
bool gui_hide(const clap_plugin_t* p)
{
  gui_self(p);
  return true;
}
const clap_plugin_gui_t gui
    = {gui_is_api_supported, gui_get_preferred_api, gui_create,  gui_destroy,
       gui_set_scale,        gui_get_size,          gui_can_resize, gui_get_resize_hints,
       gui_adjust_size,      gui_set_size,          gui_set_parent, gui_set_transient,
       gui_suggest_title,    gui_show,              gui_hide};

bool plugin_init(const clap_plugin_t*)
{
  return true;
}
void plugin_destroy(const clap_plugin_t* p)
{
  delete &self(p);
}
bool plugin_activate(const clap_plugin_t*, double, uint32_t, uint32_t)
{
  return true;
}
void plugin_deactivate(const clap_plugin_t*) { }
bool plugin_start_processing(const clap_plugin_t*)
{
  return true;
}
void plugin_stop_processing(const clap_plugin_t*) { }
void plugin_reset(const clap_plugin_t*) { }
clap_process_status plugin_process(const clap_plugin_t*, const clap_process_t*)
{
  return CLAP_PROCESS_CONTINUE;
}
const void* plugin_get_extension(const clap_plugin_t*, const char* id)
{
  return std::strcmp(id, CLAP_EXT_GUI) == 0 ? &gui : nullptr;
}
void plugin_on_main_thread(const clap_plugin_t*) { }

uint32_t factory_count(const clap_plugin_factory_t*)
{
  return 2;
}
const clap_plugin_descriptor_t*
factory_descriptor(const clap_plugin_factory_t*, uint32_t index)
{
  return index < 2 ? &descriptors[index] : nullptr;
}
const clap_plugin_t*
factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id)
{
  const bool resizable = std::strcmp(id, resizable_id) == 0;
  if(!resizable && std::strcmp(id, fixed_id) != 0)
    return nullptr;
  auto* s = new instance;
  s->host = host;
  s->resizable = resizable;
  s->plugin = {
      .desc = &descriptors[resizable ? 1 : 0],
      .plugin_data = s,
      .init = plugin_init,
      .destroy = plugin_destroy,
      .activate = plugin_activate,
      .deactivate = plugin_deactivate,
      .start_processing = plugin_start_processing,
      .stop_processing = plugin_stop_processing,
      .reset = plugin_reset,
      .process = plugin_process,
      .get_extension = plugin_get_extension,
      .on_main_thread = plugin_on_main_thread,
  };
  return &s->plugin;
}
const clap_plugin_factory_t factory = {factory_count, factory_descriptor, factory_create};

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

//! The last size the host set, and how many times it set one
CLAP_EXPORT int clap_sized_editor_size(const clap_plugin_t* p, uint32_t* w, uint32_t* h)
{
  *w = self(p).width;
  *h = self(p).height;
  return self(p).set_sizes;
}

CLAP_EXPORT double clap_sized_editor_scale(const clap_plugin_t* p)
{
  return self(p).scale;
}

CLAP_EXPORT int clap_sized_editor_off_main_thread(const clap_plugin_t* p)
{
  return self(p).off_main_thread;
}
}
