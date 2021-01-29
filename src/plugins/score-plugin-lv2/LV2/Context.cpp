
#include <LV2/Context.hpp>

#include "lv2_atom_helpers.hpp"

#include <score/tools/Debug.hpp>

#include <QDebug>

#include <lilv/lilv.h>
#include <lilv/lilvmm.hpp>

#include <iostream>

uint32_t LV2_Atom_Buffer::chunk_type;
uint32_t LV2_Atom_Buffer::sequence_type;

namespace LV2
{
namespace
{
struct WorkerData
{
  LV2::EffectContext& effect;

  uint32_t size;
  const void* data;
};

static LV2_URID do_uri_map(LV2_URI_Map_Callback_Data ptr, const char*, const char* val)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  auto& map = c.uri_map_left;

  std::string v{val};
  auto it = map.find(v);
  if (it != map.end())
  {
    return it->second;
  }
  else
  {
    auto n = c.uri_map_cur;
    map.insert(std::make_pair(v, n));
    c.uri_map_cur++;
    return n;
  }
}

static LV2_URID do_map(LV2_URID_Map_Handle ptr, const char* val)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  auto& map = c.urid_map_left;

  std::string v{val};
  auto it = map.find(v);
  if (it != map.end())
  {
    return it->second;
  }
  else
  {
    auto n = c.urid_map_cur;
    map.insert(std::make_pair(v, n));
    c.urid_map_right.insert(std::make_pair(n, v));
    c.urid_map_cur++;
    return n;
  }
}

static const char* do_unmap(LV2_URID_Unmap_Handle ptr, LV2_URID val)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  auto& map = c.urid_map_right;

  auto it = map.find(val);
  if (it != map.end())
  {
    return it->second.data();
  }
  else
  {
    return (const char*)nullptr;
  }
}

static LV2_Worker_Status do_worker(LV2_Worker_Schedule_Handle ptr, uint32_t s, const void* data)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  LV2::EffectContext* cur = c.host.current;

  if (cur && cur->worker && !cur->worker_response)
  {
    auto& w = *cur->worker;
    if (w.work)
    {
      w.work(
          cur->instance->lv2_handle,
          [](LV2_Worker_Respond_Handle sub_h, uint32_t sub_s, const void* sub_d) {
            return LV2_WORKER_ERR_UNKNOWN;
            auto sub_c = static_cast<LV2::EffectContext*>(sub_h);
            sub_c->worker_data.resize(sub_s);
            std::copy_n((const char*)sub_d, sub_s, sub_c->worker_data.data());

            sub_c->worker_response = true;
            return LV2_WORKER_SUCCESS;
          },
          cur,
          s,
          data);
      return LV2_WORKER_SUCCESS;
    }
  }
  return LV2_WORKER_ERR_UNKNOWN;
}

static LV2_Worker_Status
do_worker_state(LV2_Worker_Schedule_Handle ptr, uint32_t s, const void* data)
{
  return LV2_WORKER_ERR_UNKNOWN;
}

static int lv2_printf(LV2_Log_Handle handle, LV2_URID type, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int r = std::vprintf(format, args);
  va_end(args);
  return r;
}

static uint32_t do_event_ref(LV2_Event_Callback_Data callback_data, LV2_Event* event)
{
  SCORE_TODO;
  return 0;
}

static uint32_t do_event_unref(LV2_Event_Callback_Data callback_data, LV2_Event* event)
{
  SCORE_TODO;
  return 0;
}

static LV2_Resize_Port_Status
do_port_resize(LV2_Resize_Port_Feature_Data data, uint32_t index, size_t size)
{
  SCORE_TODO;
  return LV2_RESIZE_PORT_ERR_UNKNOWN;
}

static char* do_make_path(LV2_State_Make_Path_Handle handle, const char* path)
{
  SCORE_TODO;
  return nullptr;
}
}

GlobalContext::GlobalContext(int buffer_size, LV2::HostContext& host)
    : host{host}
    , uri_map{this, do_uri_map}
    , map{this, do_map}
    , unmap{this, do_unmap}
    , event{this, do_event_ref, do_event_unref}
    , worker{this, do_worker}
    , worker_state{this, do_worker_state}
    , logger{this,
             lv2_printf,
             [](auto h, auto t, auto fmt, auto lst) {
               return std::vprintf(fmt, lst);
             }}
    , ext_port_resize{this, do_port_resize}
    , state_make_path{this, do_make_path}
{
  options.reserve(8);

  static const int min = 0;
  static const int max = 4096;
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE,
      0,
      map.map(map.handle, LV2_BUF_SIZE__minBlockLength),
      sizeof(min),
      map.map(map.handle, LV2_ATOM__Int),
      &min});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE,
      0,
      map.map(map.handle, LV2_BUF_SIZE__maxBlockLength),
      sizeof(max),
      map.map(map.handle, LV2_ATOM__Int),
      &max});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE,
      0,
      map.map(map.handle, LV2_CORE__sampleRate),
      sizeof(sampleRate),
      map.map(map.handle, LV2_ATOM__Double),
      &sampleRate});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE,
      0,
      map.map(map.handle, LV2_BUF_SIZE__sequenceSize),
      sizeof(host.midi_buffer_size),
      map.map(map.handle, LV2_ATOM__Int),
      &host.midi_buffer_size});

  options.push_back(LV2_Options_Option{LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr});

  options_feature.data = options.data();

  lv2_features.push_back(&uri_map_feature);
  lv2_features.push_back(&map_feature);
  lv2_features.push_back(&unmap_feature);
  lv2_features.push_back(&event_feature);
  lv2_features.push_back(&options_feature);
  lv2_features.push_back(&worker_feature);
  lv2_features.push_back(&logger_feature);
  // lv2_features.push_back(&ext_data_feature);
  // lv2_features.push_back(&ext_port_resize_feature);
  // lv2_features.push_back(&state_make_path_feature);
  // lv2_features.push_back(&state_load_default_feature);
  // lv2_features.push_back(&state_thread_safe_restore_feature);
  lv2_features.push_back(&bounded);
  lv2_features.push_back(&pow2);
  lv2_features.push_back(nullptr); // must be a null-terminated array per LV2 API.
}

void LV2::GlobalContext::loadPlugins()
{
  host.world.load_all();

  // Atom stuff
  host.null_id = map.map(map.handle, "");
  host.midi_event_id = map.map(map.handle, LILV_URI_MIDI_EVENT);
  host.atom_sequence_id = map.map(map.handle, LV2_ATOM__Sequence);
  host.atom_chunk_id = map.map(map.handle, LV2_ATOM__Chunk);
  host.atom_eventTransfer = map.map(map.handle, LV2_ATOM__eventTransfer);
  lv2_atom_forge_init(&host.forge, &map);
}

LV2Data::LV2Data(HostContext& h, EffectContext& ctx) : host{h}, effect{ctx}
{
  for (auto res : {effect.plugin.get_required_features(), effect.plugin.get_optional_features()})
  {
    std::cerr << effect.plugin.get_name().as_string() << " requires " << std::endl;
    auto it = res.begin();
    while (it)
    {
      auto node = res.get(it);
      if (node.is_uri())
        std::cerr << "Required uri: " << node.as_uri() << std::endl;
      it = res.next(it);
    }
    std::cerr << std::endl << std::endl;
  }

  const auto numports = effect.plugin.get_num_ports();
  for (std::size_t i = 0; i < numports; i++)
  {
    Lilv::Port port = effect.plugin.get_port_by_index(i);

    std::cerr << "Port : " << lilv_node_as_string(port.get_name()) << std::endl;
    auto cl = port.get_classes();
    auto beg = lilv_nodes_begin(cl);
    while (!lilv_nodes_is_end(cl, beg))
    {
      auto node = lilv_nodes_get(cl, beg);
      std::cerr << " --> " << lilv_node_as_string(node) << std::endl;
      beg = lilv_nodes_next(cl, beg);
    }

    if (port.is_a(host.audio_class))
    {
      if (port.is_a(host.input_class))
      {
        audio_in_ports.push_back(i);
      }
      else if (port.is_a(host.output_class))
      {
        audio_out_ports.push_back(i);
      }
      else
      {
        cv_ports.push_back(i);
        qDebug() << "Audio port not input or output";
      }
    }
    else if (port.is_a(host.atom_class))
    {
      // TODO use  atom:supports midi:MidiEvent
      if (port.is_a(host.input_class))
      {
        midi_in_ports.push_back(i);
      }
      else if (port.is_a(host.output_class))
      {
        midi_out_ports.push_back(i);
      }
      else
      {
        midi_other_ports.push_back(i);
      }
    }
    else if (port.is_a(host.cv_class))
    {
      cv_ports.push_back(i);
    }
    else if (port.is_a(host.control_class))
    {
      if (port.is_a(host.input_class))
      {
        control_in_ports.push_back(i);
      }
      else if (port.is_a(host.output_class))
      {
        control_out_ports.push_back(i);
      }
      else
      {
        control_other_ports.push_back(i);
      }
    }
    else
    {
      control_other_ports.push_back(i);
    }
  }
}
}
