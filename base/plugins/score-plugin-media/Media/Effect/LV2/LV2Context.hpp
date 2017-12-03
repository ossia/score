#pragma once
#include <score/tools/Todo.hpp>
#include <lilv/lilvmm.hpp>
#include <lv2/lv2plug.in/ns/ext/worker/worker.h>
#include <lv2/lv2plug.in/ns/ext/data-access/data-access.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/buf-size/buf-size.h>
#include <lv2/lv2plug.in/ns/ext/data-access/data-access.h>
#include <lv2/lv2plug.in/ns/ext/dynmanifest/dynmanifest.h>
#include <lv2/lv2plug.in/ns/ext/event/event.h>
#include <lv2/lv2plug.in/ns/ext/instance-access/instance-access.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/morph/morph.h>
#include <lv2/lv2plug.in/ns/ext/options/options.h>
#include <lv2/lv2plug.in/ns/ext/parameters/parameters.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/port-groups/port-groups.h>
#include <lv2/lv2plug.in/ns/ext/port-props/port-props.h>
#include <lv2/lv2plug.in/ns/ext/presets/presets.h>
#include <lv2/lv2plug.in/ns/ext/resize-port/resize-port.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/uri-map/uri-map.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/worker/worker.h>

#include <readerwriterqueue.h>

#include <cstdio>
#include <cstdarg>
#include <boost/bimap.hpp>
#include <functional>
#include <atomic>
#include <vector>
#include <chobo/small_vector.hpp>

namespace Media
{

namespace LV2
{
struct EffectContext;
struct GlobalContext;
struct HostContext
{
    Lilv::Node make_node(const char* desc) {
      Lilv::Node n{nullptr};
      n.me = lilv_new_uri(world.me, desc);
      return n;
    }

    LV2::GlobalContext* global{};
    LV2::EffectContext* current{};
    LV2_Feature const * const * features{};
    Lilv::World& world;

    LV2_Atom_Forge forge{};

    Lilv::Node input_class{make_node(LILV_URI_INPUT_PORT)};
    Lilv::Node output_class{make_node(LILV_URI_OUTPUT_PORT)};
    Lilv::Node control_class{make_node(LILV_URI_CONTROL_PORT)};
    Lilv::Node audio_class{make_node(LILV_URI_AUDIO_PORT)};
    Lilv::Node atom_class{make_node(LV2_ATOM__AtomPort)};
    Lilv::Node event_class{make_node(LILV_URI_EVENT_PORT)};
    Lilv::Node cv_class{make_node(LV2_CORE__CVPort)};
    Lilv::Node work_interface{make_node(LV2_WORKER__interface)};
    Lilv::Node work_schedule{make_node(LV2_WORKER__schedule)};

    int32_t midi_buffer_size = 2048;
    // Lilv::Node midi_event_class{make_node(LILV_URI_MIDI_EVENT)};
    uint32_t midi_event_id{};
    uint32_t atom_chunk_id{};
    uint32_t atom_sequence_id{};
    uint32_t null_id{};
};

struct EffectContext
{
    Lilv::Plugin plugin{nullptr};
    LilvInstance* instance{};
    const LV2_Worker_Interface* worker{};
    LV2_Extension_Data_Feature data{};

    std::function<void()> on_outControlsChanged;
    std::vector<char> worker_data;
    std::atomic_bool worker_response{false};
};

struct GlobalContext
{
  public:
    GlobalContext(int buffer_size, LV2::HostContext& host);
    void loadPlugins();
    LV2_Feature const* const* features() const { return lv2_features.data(); }

    using urid_map_t = boost::bimap<std::string, LV2_URID>;
    LV2::HostContext& host;
    int sampleRate = 44100;

    std::vector<LV2_Options_Option> options;

    LV2_URID urid_map_cur = 1;
    urid_map_t urid_map;

    LV2_URID_Map map;
    LV2_URID_Unmap unmap;
    LV2_Event_Feature event;
    LV2_Worker_Schedule worker;
    LV2_Worker_Schedule worker_state;

    LV2_Log_Log logger;
    LV2_Extension_Data_Feature ext_data{nullptr};

    LV2_Resize_Port_Resize ext_port_resize;
    LV2_State_Make_Path state_make_path;

    LV2_Feature map_feature{LV2_URID__map, &map};
    LV2_Feature unmap_feature{LV2_URID__unmap, &unmap};
    LV2_Feature event_feature{LV2_EVENT_URI, &event};
    LV2_Feature options_feature{LV2_OPTIONS__options, nullptr};
    LV2_Feature worker_feature{LV2_WORKER__schedule, &worker};
    LV2_Feature worker_state_feature{LV2_WORKER__schedule, &worker};
    LV2_Feature logger_feature{LV2_LOG__log, &logger};
    LV2_Feature ext_data_feature{LV2_DATA_ACCESS_URI, &ext_data};
    LV2_Feature ext_port_resize_feature{LV2_RESIZE_PORT_URI, &ext_port_resize};
    LV2_Feature state_make_path_feature{LV2_STATE__makePath, &state_make_path};

    LV2_Feature state_load_default_feature{LV2_STATE__loadDefaultState, nullptr};
    //LV2_Feature state_thread_safe_restore_feature{LV2_STATE__threadSafeRestore, nullptr};

    LV2_Feature bounded{LV2_BUF_SIZE__boundedBlockLength, nullptr};
    LV2_Feature pow2{LV2_BUF_SIZE__powerOf2BlockLength, nullptr};

    std::vector<LV2_Feature*> lv2_features;

};


struct LV2Data
{
    LV2Data(LV2::HostContext& h, LV2::EffectContext& ctx);

    ~LV2Data()
    {

    }

    LV2::HostContext& host;
    LV2::EffectContext& effect;
    chobo::small_vector<int, 4> audio_in_ports, audio_out_ports;
    chobo::small_vector<int, 8> control_in_ports, control_out_ports, control_other_ports;
    chobo::small_vector<int, 2> midi_in_ports, midi_out_ports, midi_other_ports, cv_ports;
};
}
}
