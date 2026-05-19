#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/lockfree_queue.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>

#include <clap/ext/state.h>
#include <libremidi/detail/conversion.hpp>

#include <atomic>
#include <ranges>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Clap
{
namespace
{
// Snapshot a plug-in's state via clap.state (returns empty if the plug-in
// doesn't implement the extension). [main-thread]
inline QByteArray snapshot_clap_state(const clap_plugin_t* plugin)
{
  QByteArray out;
  if(!plugin)
    return out;
  auto state = static_cast<const clap_plugin_state_t*>(
      plugin->get_extension(plugin, CLAP_EXT_STATE));
  if(!state || !state->save)
    return out;

  clap_ostream_t stream{};
  stream.ctx = &out;
  stream.write
      = [](const clap_ostream_t* s, const void* buf, uint64_t sz) -> int64_t {
    auto* b = static_cast<QByteArray*>(s->ctx);
    const auto old = b->size();
    b->resize(old + sz);
    std::memcpy(b->data() + old, buf, sz);
    return static_cast<int64_t>(sz);
  };

  if(!state->save(plugin, &stream))
    out.clear();
  return out;
}

// Apply a previously-captured snapshot to a fresh plug-in instance.
// [main-thread]
inline void apply_clap_state(const clap_plugin_t* plugin, const QByteArray& blob)
{
  if(!plugin || blob.isEmpty())
    return;
  auto state = static_cast<const clap_plugin_state_t*>(
      plugin->get_extension(plugin, CLAP_EXT_STATE));
  if(!state || !state->load)
    return;

  struct read_ctx
  {
    const char* data;
    qsizetype size;
    qsizetype pos;
  } ctx{blob.constData(), blob.size(), 0};

  clap_istream_t stream{};
  stream.ctx = &ctx;
  stream.read
      = [](const clap_istream_t* s, void* buf, uint64_t sz) -> int64_t {
    auto* c = static_cast<read_ctx*>(s->ctx);
    const auto remaining = c->size - c->pos;
    if(remaining <= 0)
      return 0;
    const auto to_read = std::min<int64_t>(sz, remaining);
    std::memcpy(buf, c->data + c->pos, to_read);
    c->pos += to_read;
    return to_read;
  };

  state->load(plugin, &stream);
}
}

static auto dummy_audio_buffer() noexcept
{
  clap_audio_buffer_t buffer{};
  buffer.data32 = nullptr;
  buffer.data64 = nullptr;
  buffer.channel_count = 0;
  buffer.latency = 0;
  buffer.constant_mask = 0;
  return buffer;
}

struct event_storage
{
  std::vector<clap_event_midi_t> midi_events;
  std::vector<clap_event_midi2_t> midi2_events;
  std::vector<clap_event_note_t> note_events;
  std::vector<clap_event_param_value_t> param_events;
  std::vector<clap_event_header_t*> all_events;

  void clear()
  {
    midi_events.clear();
    midi2_events.clear();
    note_events.clear();
    param_events.clear();
    all_events.clear();
  }
};

class clap_node_base : public ossia::graph_node
{
public:
  std::shared_ptr<Clap::PluginHandle> handle;
  explicit clap_node_base(const Clap::Model& proc)
      : handle{proc.handle()}
      , m_param_ins{handle->m_parameters_ins}
      , m_param_outs{handle->m_parameters_outs}
      , m_midi_ins{handle->m_midi_ins}
      , m_midi_outs{handle->m_midi_outs}
  {
    set_not_fp_safe();
    midi_ins.reserve(m_midi_ins.size());
    midi_outs.reserve(m_midi_outs.size());
    parameter_ins.reserve(m_param_ins.size());
    parameter_outs.reserve(m_param_outs.size());
    m_input_events.all_events.reserve(4096);

    // Create ports based on the model
    for(auto inlet : proc.inlets())
    {
      if(qobject_cast<Process::AudioInlet*>(inlet))
      {
        audio_ins.push_back(new ossia::audio_inlet);
        m_inlets.push_back(audio_ins.back());
      }
      else if(qobject_cast<Process::MidiInlet*>(inlet))
      {
        midi_ins.push_back(new ossia::midi_inlet);
        m_inlets.push_back(midi_ins.back());
      }
      else if(auto inl = qobject_cast<Process::ControlInlet*>(inlet))
      {
        parameter_ins.push_back(new ossia::value_inlet);
        m_inlets.push_back(parameter_ins.back());
        parameter_ins.back()->data.write_value(inl->value(), 0);
      }
    }

    for(auto outlet : proc.outlets())
    {
      if(qobject_cast<Process::AudioOutlet*>(outlet))
      {
        audio_outs.push_back(new ossia::audio_outlet);
        m_outlets.push_back(audio_outs.back());
      }
      else if(qobject_cast<Process::MidiOutlet*>(outlet))
      {
        midi_outs.push_back(new ossia::midi_outlet);
        m_outlets.push_back(midi_outs.back());
      }
      else if(qobject_cast<Process::ControlOutlet*>(outlet))
      {
        parameter_outs.push_back(new ossia::value_outlet);
        m_outlets.push_back(parameter_outs.back());
      }
    }

    SCORE_ASSERT(parameter_ins.size() == m_param_ins.size());
    SCORE_ASSERT(parameter_outs.size() == m_param_outs.size());
    SCORE_ASSERT(audio_ins.size() == proc.audioInputs().size());
    SCORE_ASSERT(audio_outs.size() == proc.audioOutputs().size());
    SCORE_ASSERT(midi_ins.size() == proc.midiInputs().size());
    SCORE_ASSERT(midi_outs.size() == proc.midiOutputs().size());
  }

  std::string label() const noexcept override
  { //FIXME
    return "clap";
  }

  [[nodiscard]] bool activate_plugin(
      const clap_plugin_t* plugin, double sample_rate, uint32_t max_buffer_size)
  {
    if(!plugin)
      return false;

    m_sample_rate = sample_rate;
    m_buffer_size = max_buffer_size;

    if(plugin->activate(plugin, sample_rate, 1, max_buffer_size))
    {
      return true;
    }
    return false;
  }

  [[nodiscard]] bool start_plugin(const clap_plugin_t* plugin)
  {
    if(plugin->start_processing(plugin))
    {
      init_parameter_values(plugin);
      return true;
    }
    return false;
  }

  void stop_plugin(const clap_plugin_t* plugin) { plugin->stop_processing(plugin); }

  void init_parameter_values(const clap_plugin_t* plugin)
  {
    if(!plugin)
      return;

    int param_i = 0;
    // Get parameter extension to set initial values
    for(const auto& param_info : m_param_ins)
    {
      // Create parameter value event with default value
      clap_event_param_value_t param_event{};
      param_event.header.size = sizeof(clap_event_param_value_t);
      param_event.header.time = 0;
      param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      param_event.header.type = CLAP_EVENT_PARAM_VALUE;
      param_event.header.flags = 0;
      param_event.param_id = param_info.id;
      param_event.cookie = param_info.cookie;
      param_event.note_id = -1;
      param_event.port_index = -1;
      param_event.channel = -1;
      param_event.key = -1;
      auto& v = parameter_ins[param_i]->data.get_data();
      if(!v.empty())
        param_event.value = ossia::convert<float>(v.back().value);
      else
        param_event.value = param_info.default_value;
      param_i++;

      // Create temporary input events structure to send the parameter
      event_storage temp_events;
      temp_events.param_events.push_back(param_event);
      temp_events.all_events.push_back(
          reinterpret_cast<clap_event_header_t*>(&temp_events.param_events.back()));

      // Create input events interface
      clap_input_events evs{
          .ctx = &temp_events,
          .size = +[](const clap_input_events* list) -> uint32_t {
        auto* storage = static_cast<event_storage*>(list->ctx);
        return storage->all_events.size();
      },
          .get = +[](const clap_input_events* list,
                     uint32_t index) -> const clap_event_header_t* {
        auto* storage = static_cast<event_storage*>(list->ctx);
        if(index < storage->all_events.size())
          return storage->all_events[index];
        return nullptr;
      }};

      // Create dummy output events
      event_storage temp_output;
      clap_output_events_t o_evs{
          .ctx = &temp_output,
          .try_push = [](const struct clap_output_events* list,
                         const clap_event_header_t* event) -> bool {
        return false; // Ignore output events during initialization
      }};

      // Create minimal process structure just for parameter setting
      clap_process_t process{};
      process.frames_count = 0;
      process.audio_inputs = nullptr;
      process.audio_outputs = nullptr;
      process.audio_inputs_count = 0;
      process.audio_outputs_count = 0;
      process.steady_time = -1;
      process.in_events = &evs;
      process.out_events = &o_evs;
      process.transport = nullptr;

      // Send the parameter to the plugin
      plugin->process(plugin, &process);
    }
  }

  [[nodiscard]]
  bool deactivate_plugin(const clap_plugin_t* plugin)
  {
    if(!plugin)
      return false;

    plugin->deactivate(plugin);
    return true;
  }

  auto make_transport(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    const double song_pos_beats = tk.musical_start_position;
    const double song_pos_seconds = tk.prev_date.impl * st.samplesToModel();
    uint32_t transport_flags
        = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE
          | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
    if(tk.prev_date != tk.date)
      transport_flags |= CLAP_TRANSPORT_IS_PLAYING;

    // Bar information
    const double bar_start = tk.musical_start_last_bar;
    const int32_t bar_number = static_cast<int32_t>(
        tk.musical_start_last_bar / (4.0 * tk.signature.upper / tk.signature.lower));

    clap_event_transport_t transport{
        .header = {
            .size = sizeof(clap_event_transport_t),
            .time = 0,
            .space_id = CLAP_CORE_EVENT_SPACE_ID,
            .type = CLAP_EVENT_TRANSPORT,
            .flags = 0,
        },
        .flags = transport_flags,
        .song_pos_beats = (clap_beattime)std::floor(song_pos_beats),
        .song_pos_seconds = (clap_sectime)std::floor(song_pos_seconds),
        .tempo = tk.tempo,
        .tempo_inc = 0.0, // FIXME
        .loop_start_beats = 0,
        .loop_end_beats = 0,
        .loop_start_seconds = 0,
        .loop_end_seconds = 0,
        .bar_start = (clap_beattime) std::floor(bar_start),
        .bar_number = bar_number,
        .tsig_num = static_cast<uint16_t>(tk.signature.upper),
        .tsig_denom = static_cast<uint16_t>(tk.signature.lower)
    };

    return transport;
  }
  void process_controls(uint32_t samples)
  {
    // Process control inlets and create parameter events
    std::size_t param_idx = 0;

    for(std::size_t i = 0; i < m_inlets.size(); ++i)
    {
      if(auto ctrl_in = m_inlets[i]->target<ossia::value_port>())
      {
        const auto& data = ctrl_in->get_data();
        if(!data.empty() && param_idx < m_param_ins.size())
        {
          auto& param_info = m_param_ins[param_idx];
          double value = ossia::convert<double>(data.back().value);

          // Clamp value to parameter range
          value = std::clamp(value, param_info.min_value, param_info.max_value);

          clap_event_param_value_t param_event{};
          param_event.header.size = sizeof(clap_event_param_value_t);
          param_event.header.time = 0; // Beginning of buffer for now
          param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          param_event.header.type = CLAP_EVENT_PARAM_VALUE;
          param_event.header.flags = CLAP_EVENT_IS_LIVE;
          param_event.param_id = param_info.id;
          param_event.cookie = param_info.cookie;
          param_event.note_id = -1;
          param_event.port_index = -1;
          param_event.channel = -1;
          param_event.key = -1;
          param_event.value = value;

          m_input_events.param_events.push_back(param_event);
          m_input_events.all_events.push_back(
              reinterpret_cast<clap_event_header_t*>(
                  &m_input_events.param_events.back()));
        }
        param_idx++;
      }
    }
  }

  void process_midi()
  {
    uint16_t midi_port_index = 0;
    for(ossia::midi_inlet* midi_in : midi_ins)
    {
      const auto& spec = m_midi_ins[midi_port_index];
      const auto& msgs = midi_in->data.messages;

      if(spec.supported_dialects & clap_note_dialect::CLAP_NOTE_DIALECT_MIDI2)
      {
        for(const auto& m : msgs)
        {
          clap_event_midi2_t ev{};
          ev.header.size = sizeof(clap_event_midi2_t);
          ev.header.time = m.timestamp;
          ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          ev.header.type = CLAP_EVENT_MIDI2;
          ev.header.flags = 0;
          ev.port_index = midi_port_index;
          ev.data[0] = m.data[0];
          ev.data[1] = m.data[1];
          ev.data[2] = m.data[2];
          ev.data[3] = m.data[3];

          m_input_events.midi2_events.push_back(ev);
          m_input_events.all_events.push_back(
              reinterpret_cast<clap_event_header_t*>(
                  &m_input_events.midi2_events.back()));
        }
      }
      else if(spec.supported_dialects & clap_note_dialect::CLAP_NOTE_DIALECT_CLAP)
      {
        for(const auto& m : msgs)
        {
          if(m.get_type() != libremidi::midi2::message_type::MIDI_2_CHANNEL)
            continue;
          clap_event_note_t ev{};
          ev.header.size = sizeof(clap_event_note_t);
          ev.header.time = m.timestamp;
          ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          ev.header.flags = 0;
          ev.port_index = midi_port_index;
          ev.note_id = -1;
          switch(libremidi::message_type(m.get_status_code()))
          {
            case libremidi::message_type::NOTE_ON: {
              auto [channel, note, value] = libremidi::as_01::note_off(m);
              if(value > 0)
              {
                ev.header.type = CLAP_EVENT_NOTE_ON;
                ev.channel = channel;
                ev.key = note;
                ev.velocity = value;
              }
              else
              {
                ev.header.type = CLAP_EVENT_NOTE_OFF;
                ev.channel = channel;
                ev.key = note;
                ev.velocity = 0.0;
              }
              m_input_events.note_events.push_back(ev);
              m_input_events.all_events.push_back(
                  reinterpret_cast<clap_event_header_t*>(
                      &m_input_events.note_events.back()));
              break;
            }
            case libremidi::message_type::NOTE_OFF: {
              auto [channel, note, value] = libremidi::as_01::note_off(m);
              ev.header.type = CLAP_EVENT_NOTE_OFF;
              ev.channel = channel;
              ev.key = note;
              ev.velocity = value;
              m_input_events.note_events.push_back(ev);
              m_input_events.all_events.push_back(
                  reinterpret_cast<clap_event_header_t*>(
                      &m_input_events.note_events.back()));
              break;
            }
            default:
              break;
          }
        }
      }
      else if(spec.supported_dialects & clap_note_dialect::CLAP_NOTE_DIALECT_MIDI)
      {
        for(const auto& m : msgs)
        {
          // Convert UMP to MIDI 1.0
          uint8_t midi_bytes[16];
          auto bytes_written = cmidi2_convert_single_ump_to_midi1(
              midi_bytes, sizeof(midi_bytes), (cmidi2_ump*)m.data);

          if(bytes_written > 0 && bytes_written <= 3)
          {
            clap_event_midi_t ev{};
            ev.header.size = sizeof(clap_event_midi_t);
            ev.header.time = m.timestamp;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.type = CLAP_EVENT_MIDI;
            ev.header.flags = 0;
            ev.port_index = midi_port_index;
            std::memcpy(ev.data, midi_bytes, bytes_written);

            m_input_events.midi_events.push_back(ev);
            m_input_events.all_events.push_back(
                reinterpret_cast<clap_event_header_t*>(
                    &m_input_events.midi_events.back()));
          }
        }
      }
      // FIXME sysex
      midi_port_index++;
    }
  }

  void prepare_input_events(int samples)
  {
    m_input_events.clear();
    m_output_events.clear();

    int param_event_count = 0;
    int midi_event_count = 0;
    for(auto port : this->parameter_ins)
      param_event_count += port->data.get_data().size();
    for(auto port : this->midi_ins)
      midi_event_count += port->data.messages.size();
    m_input_events.midi_events.reserve(midi_event_count * 1.1);
    m_input_events.midi2_events.reserve(midi_event_count * 1.1);
    m_input_events.note_events.reserve(midi_event_count * 1.1);
    m_input_events.param_events.reserve(
        param_event_count * 1.1
        + 2
              * this->parameter_ins
                    .size()); // Important to reserve one additional buffer of parameters for the mono case
    m_input_events.all_events.reserve((param_event_count + midi_event_count + 1) * 1.1);

    // Process parameter changes
    process_controls(samples);
    process_midi();

    // Events need to be sorted
    std::sort(
        m_input_events.all_events.begin(), m_input_events.all_events.end(),
        [](const clap_event_header_t* a, const clap_event_header_t* b) {
      return a->time < b->time;
    });
  }

  // Forward any CLAP_EVENT_PARAM_VALUE events the plugin emitted during
  // process() to the matching `parameter_outs` value_outlet so downstream
  // ossia nodes (and the GUI bargraphs) see live values.
  void dispatch_param_outputs()
  {
    if(m_output_events.param_events.empty() || m_param_outs.empty())
      return;

    for(const auto& ev : m_output_events.param_events)
    {
      for(std::size_t i = 0; i < m_param_outs.size(); ++i)
      {
        if(m_param_outs[i].id == ev.param_id && i < parameter_outs.size())
        {
          parameter_outs[i]->data.write_value(
              ossia::value{ev.value}, static_cast<int64_t>(ev.header.time));
          break;
        }
      }
    }
  }

  ossia::small_vector<ossia::audio_inlet*, 2> audio_ins;
  ossia::small_vector<ossia::audio_outlet*, 2> audio_outs;
  ossia::small_vector<ossia::midi_inlet*, 2> midi_ins;
  ossia::small_vector<ossia::midi_outlet*, 2> midi_outs;
  std::vector<ossia::value_inlet*> parameter_ins;
  std::vector<ossia::value_outlet*> parameter_outs;
  clap_event_transport_t m_current_transport;

  event_storage m_input_events;
  event_storage m_output_events;

  const std::vector<clap_param_info_t>& m_param_ins;
  const std::vector<clap_param_info_t>& m_param_outs;
  const std::vector<clap_note_port_info_t>& m_midi_ins;
  const std::vector<clap_note_port_info_t>& m_midi_outs;
  double m_sample_rate{44100.0};
  uint32_t m_buffer_size{512};
};

// Normal implementation
class clap_node : public clap_node_base
{
public:
  std::vector<double> dummy_io_buffer;
  clap_node(const Clap::Model& proc, Clap::PluginHandle& handle, int sampleRate, int bs)
      : clap_node_base{proc}
      , m_instance{handle}
  {
    m_expected_audio_inputs = proc.audioInputs();
    m_expected_audio_outputs = proc.audioOutputs();

    if(handle.activated)
      (void)deactivate_plugin(handle.plugin);
    m_activated = activate_plugin(m_instance.plugin, sampleRate, bs);
    handle.activated = m_activated;
    dummy_io_buffer.resize(2 * bs);
  }

  ~clap_node()
  {
    // We do not deactivate in the audio thread where the dtor of clap_node runs
    // but later in the main thread (driven by Model::resetExecution / ~Model
    // through the synced handle.activated flag set in the constructor).
  }

  void do_process(
      clap_process_t& process, event_storage& input_storage,
      event_storage& output_storage)
  {
    // Process audio
    process.steady_time = -1;
    process.transport = &m_current_transport;

    // Setup input events
    clap_input_events evs{
        .ctx = &input_storage,
        .size = +[](const clap_input_events* list) -> uint32_t {
      auto* storage = static_cast<event_storage*>(list->ctx);
      return storage->all_events.size();
    },
        .get = +[](const clap_input_events* list,
                   uint32_t index) -> const clap_event_header_t* {
      auto* storage = static_cast<event_storage*>(list->ctx);
      if(index < storage->all_events.size())
        return storage->all_events[index];
      return nullptr;
    }};

    // Setup output events
    clap_output_events_t o_evs{
        .ctx = &output_storage,
        .try_push = [](const struct clap_output_events* list,
                       const clap_event_header_t* event) -> bool {
      auto* storage = static_cast<event_storage*>(list->ctx);
      if(!event || event->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return false;
      switch(event->type)
      {
        case CLAP_EVENT_MIDI:
          if(event->size >= sizeof(clap_event_midi_t))
          {
            storage->midi_events.push_back(
                *reinterpret_cast<const clap_event_midi_t*>(event));
            return true;
          }
          return false;
        case CLAP_EVENT_PARAM_VALUE:
          if(event->size >= sizeof(clap_event_param_value_t))
          {
            storage->param_events.push_back(
                *reinterpret_cast<const clap_event_param_value_t*>(event));
            return true;
          }
          return false;
        default:
          // FIXME notes, note-end, etc.
          return false;
      }
    }};

    process.in_events = &evs;
    process.out_events = &o_evs;

    m_instance.plugin->process(m_instance.plugin, &process);

    dispatch_param_outputs();

    // Process MIDI output
    std::size_t midi_port_index = 0;
    for(std::size_t i = 0; i < m_outlets.size(); ++i)
    {
      if(auto midi_out = m_outlets[i]->target<ossia::midi_port>())
      {
        auto& port_messages = midi_out->messages;
        port_messages.clear();

        for(const auto& midi_event : m_output_events.midi_events)
        {
          if(midi_event.port_index == midi_port_index)
          {
            libremidi::ump msg;
            if(cmidi2_midi1_channel_voice_to_midi2(midi_event.data, 3, msg.data))
            {
              msg.timestamp = midi_event.header.time;
              port_messages.push_back(msg);
            }
          }
        }
        midi_port_index++;
      }
    }
  }

  PluginHandle& m_instance;
  std::vector<clap_audio_port_info_t> m_expected_audio_inputs{};
  std::vector<clap_audio_port_info_t> m_expected_audio_outputs{};

  std::vector<clap_audio_buffer_t> input_buffers;
  std::vector<clap_audio_buffer_t> output_buffers;

  bool m_activated{};
  bool m_processing{};
};

class clap_node_32 final : public clap_node
{
  std::vector<std::vector<float>> input_channel_storage;
  std::vector<std::vector<float>> output_channel_storage;
  std::vector<std::vector<float*>> input_channel_ptrs;
  std::vector<std::vector<float*>> output_channel_ptrs;

public:
  using clap_node::clap_node;

  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(!m_instance.plugin)
      return;

    // Activate plugin if needed
    if(!m_activated)
      return;
    if(!m_processing)
    {
      m_processing = start_plugin(m_instance.plugin);
      if(!m_processing)
        return;
    }

    auto [offset, samples] = e.timings(t);
    if(samples == 0)
      return;

    m_current_transport = make_transport(t, e);

    // Clear previous data
    input_buffers.clear();
    output_buffers.clear();
    input_channel_storage.clear();
    output_channel_storage.clear();
    input_channel_ptrs.clear();
    output_channel_ptrs.clear();

    prepare_input_events(samples);

    // Setup audio input buffers
    // We must create exactly the number of buffers the plugin expects
    int audio_in_idx = 0;
    for(ossia::audio_inlet* audio_in : audio_ins)
    {
      const auto& audio_info = this->m_expected_audio_inputs[audio_in_idx];
      clap_audio_buffer_t buffer = dummy_audio_buffer();
      buffer.channel_count = audio_info.channel_count;

      audio_in->data.set_channels(buffer.channel_count);
      auto& channels = audio_in->data.get();
      if(!channels.empty())
      {
        auto& storage = input_channel_storage.emplace_back();
        storage.resize(buffer.channel_count);

        auto& ptrs = input_channel_ptrs.emplace_back();
        ptrs.resize(buffer.channel_count);

        storage.resize(samples * buffer.channel_count + 16);
        for(uint32_t c = 0; c < buffer.channel_count; ++c)
        {
          ptrs[c] = storage.data() + c * samples;

          // Convert from double to float
          if(c < channels.size())
          {
            auto& channel = channels[c];
            //SCORE_SOFT_ASSERT(channel.size() >= e.bufferSize());
            channel.resize(std::max((int)channel.size(), (int)e.bufferSize()));
            const double* src = channels[c].data() + offset;
            float* dst = ptrs[c];
            for(uint32_t s = 0; s < samples; ++s)
            {
              dst[s] = static_cast<float>(src[s]);
            }
          }
          else
          {
            // Zero fill if no input data
            std::fill(ptrs[c], ptrs[c] + samples, 0.0f);
          }
        }

        buffer.data32 = ptrs.data();
      }

      input_buffers.push_back(buffer);
      audio_in_idx++;
    }

    // Setup output buffers
    int audio_out_idx = 0;
    const bool needs_stereo_main_out
        = audio_ins.empty() && !audio_outs.empty()
          && !midi_ins.empty(); // FIXME check if instrument?
    for(ossia::audio_outlet* audio_out : audio_outs)
    {
      const auto& audio_info = this->m_expected_audio_outputs[audio_out_idx];
      clap_audio_buffer_t buffer = dummy_audio_buffer();
      buffer.channel_count = audio_info.channel_count;

      const int ossia_channel_count = audio_out_idx == 0 && needs_stereo_main_out
                                          ? std::max((int)buffer.channel_count, 2)
                                          : buffer.channel_count;
      audio_out->data.set_channels(ossia_channel_count);
      auto& channels = audio_out->data.get();

      // Allocate float storage for output
      auto& storage = output_channel_storage.emplace_back();
      storage.resize(buffer.channel_count);

      auto& ptrs = output_channel_ptrs.emplace_back();
      ptrs.resize(buffer.channel_count);

      storage.clear();
      storage.resize(samples * buffer.channel_count + 16);
      for(uint32_t c = 0; c < ossia_channel_count; ++c)
      {
        channels[c].resize(e.bufferSize());
      }
      for(uint32_t c = 0; c < buffer.channel_count; ++c)
      {
        ptrs[c] = storage.data() + c * samples;
      }

      buffer.data32 = ptrs.data();
      output_buffers.push_back(buffer);
      audio_out_idx++;
    }

    // Ensure we have exactly the number of buffers the plugin expects
    // *before* taking .data() pointers below.
    while(input_buffers.size() < m_expected_audio_inputs.size())
      input_buffers.push_back(dummy_audio_buffer());
    while(output_buffers.size() < m_expected_audio_outputs.size())
      output_buffers.push_back(dummy_audio_buffer());

    clap_process_t process{};
    process.frames_count = samples;
    process.audio_inputs = input_buffers.data();
    process.audio_outputs = output_buffers.data();
    process.audio_inputs_count = m_expected_audio_inputs.size();
    process.audio_outputs_count = m_expected_audio_outputs.size();
    do_process(process, m_input_events, m_output_events);

    // Convert audio output from float back to double
    audio_out_idx = 0;
    for(ossia::audio_outlet* audio_out : audio_outs)
    {
      if(audio_out_idx < output_channel_storage.size())
      {
        auto& channels = audio_out->data.get();
        const auto& storage = output_channel_storage[audio_out_idx];
        const uint32_t plugin_channels
            = m_expected_audio_outputs[audio_out_idx].channel_count;

        if(audio_out_idx == 0 && needs_stereo_main_out && plugin_channels == 1)
        {
          // Basic mono instrument case (e.g. Nekobi): duplicate L → R only
          // when the plugin's main out is actually mono. For ≥2-channel
          // outs the second channel holds valid audio.
          const float* src = storage.data();
          double* dst_l = channels[0].data() + offset;
          double* dst_r = channels[1].data() + offset;
          for(uint32_t s = 0; s < samples; ++s)
          {
            dst_l[s] = static_cast<double>(src[s]);
            dst_r[s] = dst_l[s];
          }
        }
        else
        {
          const uint32_t channel_max
              = std::min<uint32_t>(channels.size(), plugin_channels);
          for(uint32_t c = 0; c < channel_max; ++c)
          {
            const float* src = storage.data() + c * samples;
            double* dst = channels[c].data() + offset;
            for(uint32_t s = 0; s < samples; ++s)
              dst[s] = static_cast<double>(src[s]);
          }
        }
      }
      audio_out_idx++;
    }
  }
};

class clap_node_64 final : public clap_node
{
  std::vector<std::vector<double*>> input_channel_ptrs;
  std::vector<std::vector<double*>> output_channel_ptrs;

public:
  using clap_node::clap_node;
  ~clap_node_64() { }
  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(!m_instance.plugin)
      return;

    // Activate plugin if needed
    if(!m_activated)
      return;
    if(!m_processing)
    {
      m_processing = start_plugin(m_instance.plugin);
      if(!m_processing)
        return;
    }

    auto [offset, samples] = e.timings(t);
    if(samples == 0)
      return;

    m_current_transport = make_transport(t, e);

    // Prepare buffers
    input_buffers.clear();
    output_buffers.clear();
    m_input_events.clear();
    m_output_events.clear();

    // Process parameter changes
    prepare_input_events(samples);

    // Setup audio input buffers
    std::size_t audio_in_idx = 0;
    for(ossia::audio_inlet* audio_in : audio_ins)
    {
      const auto& audio_info = this->m_expected_audio_inputs[audio_in_idx];
      clap_audio_buffer_t buffer = dummy_audio_buffer();
      buffer.channel_count = audio_info.channel_count;

      audio_in->data.set_channels(buffer.channel_count);
      auto& channels = audio_in->data.get();
      if(!channels.empty())
      {
        // Pass exactly what the plugin expects. The previous code clamped
        // this to 2 channels, dropping surround / multi-channel inputs.
        auto& ptrs = input_channel_ptrs.emplace_back();
        ptrs.resize(buffer.channel_count);

        for(uint32_t c = 0; c < buffer.channel_count; ++c)
        {
          if(c < channels.size())
          {
            auto& channel = channels[c];
            channel.resize(std::max<std::size_t>(channel.size(), e.bufferSize()));
            ptrs[c] = const_cast<double*>(channels[c].data() + offset);
          }
          else
          {
            ptrs[c] = dummy_io_buffer.data();
          }
        }

        buffer.data64 = ptrs.data();
      }

      this->input_buffers.push_back(buffer);
      audio_in_idx++;
    }

    // Setup output buffers
    std::size_t audio_out_idx = 0;
    const bool needs_stereo_main_out
        = audio_ins.empty() && !audio_outs.empty()
          && !midi_ins.empty(); // FIXME check if instrument?
    for(ossia::audio_outlet* audio_out : audio_outs)
    {
      const auto& audio_info = this->m_expected_audio_outputs[audio_out_idx];
      clap_audio_buffer_t buffer = dummy_audio_buffer();
      buffer.channel_count = audio_info.channel_count;

      const int ossia_channel_count = audio_out_idx == 0 && needs_stereo_main_out
                                          ? std::max((int)buffer.channel_count, 2)
                                          : buffer.channel_count;
      audio_out->data.set_channels(ossia_channel_count);
      auto& channels = audio_out->data.get();

      auto& ptrs = output_channel_ptrs.emplace_back();
      ptrs.resize(buffer.channel_count);

      for(uint32_t c = 0; c < ossia_channel_count; ++c)
      {
        channels[c].resize(e.bufferSize());
      }
      for(uint32_t c = 0; c < buffer.channel_count; ++c)
      {
        ptrs[c] = channels[c].data() + offset;
      }

      buffer.data64 = ptrs.data();
      this->output_buffers.push_back(buffer);
      audio_out_idx++;
    }

    // Pad both buffer lists *before* taking .data() pointers so we don't
    // hand the plugin a stale pointer after a reallocation, and so the two
    // counts actually match what the plugin is told below.
    while(this->input_buffers.size() < m_expected_audio_inputs.size())
      this->input_buffers.push_back(dummy_audio_buffer());
    while(this->output_buffers.size() < m_expected_audio_outputs.size())
      this->output_buffers.push_back(dummy_audio_buffer());

    clap_process_t process{};
    process.frames_count = samples;
    process.audio_inputs = this->input_buffers.data();
    process.audio_outputs = this->output_buffers.data();
    process.audio_inputs_count = m_expected_audio_inputs.size();
    process.audio_outputs_count = m_expected_audio_outputs.size();
    do_process(process, m_input_events, m_output_events);

    // Basic mono instrument case (e.g. Nekobi): only duplicate if the plug-in
    // really has a mono main out. For ≥2-channel outputs the right channel is
    // valid and must not be clobbered.
    if(needs_stereo_main_out && !m_expected_audio_outputs.empty()
       && m_expected_audio_outputs[0].channel_count == 1)
    {
      auto& l = audio_outs[0]->data.channel(0);
      auto& r = audio_outs[0]->data.channel(1);
      r.assign(l.begin(), l.end());
    }
  }
};

// Special case for monophonic nodes
class clap_node_mono : public clap_node_base
{
public:
  // Declared early so member function signatures (make_poly_instance) and the
  // m_incoming queue declared further down can refer to it.
  struct poly_plugin
  {
    const clap_plugin_t* plugin{};
    bool activated{};
    bool processing{};
    operator const clap_plugin_t*() const noexcept { return plugin; }
  };

  clap_node_mono(
      const Clap::Model& proc, Clap::PluginHandle& handle, int sampleRate, int bs)
      : clap_node_base{proc}
      , m_instance{handle}
      , m_plugin_id{proc.pluginId().toUtf8()}
  {
    // Pre-reserve enough capacity that the dynamic grower (which feeds
    // m_poly from the audio thread via drain_incoming → push_back) never
    // triggers a reallocation in normal use. 256 * sizeof(poly_plugin)
    // ≈ 4 kB up front, and covers up to 256 channels of polyphony without
    // any audio-thread malloc; beyond that vector will still grow, just
    // with a non-RT-friendly reallocation cost each doubling.
    m_poly.reserve(256);
    m_poly.push_back({handle.plugin, false});
    if(handle.activated)
      (void)deactivate_plugin(handle.plugin);
    m_poly.back().activated = activate_plugin(m_instance.plugin, sampleRate, bs);
    // Mirror the activation state on the shared handle so that
    // Clap::Model::~Model / resetExecution can drive the deactivate of
    // m_poly[0] from the main thread, symmetrically with clap_node.
    handle.activated = m_poly.back().activated;

    // The constructor runs on the main thread, so create_plugin/init are
    // spec-correct here. If any of the extra instances fail to construct we
    // tear down the ones we already built so we never leak plugin handles.
    try
    {
      for(int i = 0; i < 7; i++)
        add_poly_instance(sampleRate, bs);
    }
    catch(...)
    {
      destroy_extra_instances();
      throw;
    }
  }

  void add_poly_instance(int sampleRate, int bs)
  {
    m_poly.push_back(make_poly_instance(sampleRate, bs, /*state*/ {}));
  }

  // [main-thread] Build a fresh, init'd, activated polyphonic instance.
  // Optionally clones state into it via clap.state (so AIDA-X & friends
  // keep their loaded model / preset on each instance).
  // Throws std::runtime_error on create_plugin / init failure.
  poly_plugin make_poly_instance(int sampleRate, int bs, const QByteArray& state)
  {
    poly_plugin p{nullptr, false, false};
    p.plugin = m_instance.factory->create_plugin(
        m_instance.factory, &m_instance.host, m_plugin_id.data());
    if(!p.plugin)
      throw std::runtime_error("Could not create plug-in instance");

    if(!p.plugin->init(p.plugin))
    {
      p.plugin->destroy(p.plugin);
      throw std::runtime_error("Could not init plug-in instance");
    }
    // Apply the snapshot *before* activate, matching how Model::loadPlugin
    // restores state for the primary instance.
    apply_clap_state(p.plugin, state);
    p.activated = activate_plugin(p.plugin, sampleRate, bs);
    return p;
  }

  // [audio-thread] Drain any new instances that the main-thread grower
  // has handed us through m_incoming.
  void drain_incoming() noexcept
  {
    poly_plugin p;
    while(m_incoming.try_dequeue(p))
      m_poly.push_back(p);
  }

  // [audio-thread] Tell the grower we need at least `n` total instances.
  // Monotonically-increasing: a brief drop to a smaller channel count
  // does not cause us to shrink the pool.
  void request_pool_size(std::size_t n) noexcept
  {
    auto prev = m_requested_pool.load(std::memory_order_relaxed);
    while(n > prev
          && !m_requested_pool.compare_exchange_weak(
              prev, n, std::memory_order_release, std::memory_order_relaxed))
      ;
  }

  // Tear down extra polyphonic instances (skipping m_poly[0], which is
  // owned by the Clap::Model). Used by the constructor's catch and by the
  // destructor's queued main-thread cleanup.
  void destroy_extra_instances() noexcept
  {
    for(std::size_t i = 1; i < m_poly.size(); ++i)
    {
      auto& p = m_poly[i];
      if(!p.plugin)
        continue;
      if(p.processing)
      {
        p.plugin->stop_processing(p.plugin);
        p.processing = false;
      }
      if(p.activated)
      {
        p.plugin->deactivate(p.plugin);
        p.activated = false;
      }
      p.plugin->destroy(p.plugin);
      p.plugin = nullptr;
    }
  }

  ~clap_node_mono()
  {
    // Stop any instance that was still processing. ~clap_node_mono runs on
    // the audio thread, so stop_processing is allowed here (it is the only
    // CLAP entry point we may call from there). Deactivation and destruction
    // of the extra instances is deferred to the main thread below.
    for(std::size_t i = 0; i < m_poly.size(); ++i)
    {
      if(m_poly[i].processing)
      {
        m_poly[i].plugin->stop_processing(m_poly[i].plugin);
        m_poly[i].processing = false;
      }
    }

    // Drain any instances the grower had pushed but the audio thread did
    // not consume yet. They are already created/init'd/activated, so we
    // must deactivate + destroy them on the main thread alongside m_poly.
    std::vector<poly_plugin> pending;
    {
      poly_plugin tmp{};
      while(m_incoming.try_dequeue(tmp))
        pending.push_back(tmp);
    }

    // m_poly[0] is the Clap::Model's shared handle: Model::~Model and the
    // resetExecution handler take care of deactivating it on the main
    // thread (and they will, because the ctor synced handle.activated).
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [poly = std::move(m_poly), pending = std::move(pending)]() mutable {
      auto destroy = [](const poly_plugin& p) {
        if(!p.plugin)
          return;
        if(p.activated)
          p.plugin->deactivate(p.plugin);
        p.plugin->destroy(p.plugin);
      };
      for(std::size_t i = 1; i < poly.size(); ++i)
        destroy(poly[i]);
      for(auto& p : pending)
        destroy(p);
    });
  }

  void do_process(
      const clap_plugin_t* plug, clap_process_t& process, event_storage& input_storage,
      event_storage& output_storage, int current_channel)
  {
    // Process audio
    process.steady_time = -1;
    process.transport = &m_current_transport;

    // Setup input events
    clap_input_events evs{
        .ctx = &input_storage,
        .size = +[](const clap_input_events* list) -> uint32_t {
      auto* storage = static_cast<event_storage*>(list->ctx);
      return storage->all_events.size();
    },
        .get = +[](const clap_input_events* list,
                   uint32_t index) -> const clap_event_header_t* {
      auto* storage = static_cast<event_storage*>(list->ctx);
      if(index < storage->all_events.size())
        return storage->all_events[index];
      return nullptr;
    }};

    clap_output_events_t o_evs{
        .ctx = &output_storage,
        .try_push = [](const struct clap_output_events* list,
                       const clap_event_header_t* event) -> bool {
      auto* storage = static_cast<event_storage*>(list->ctx);
      if(!event || event->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return false;
      if(event->type == CLAP_EVENT_PARAM_VALUE
         && event->size >= sizeof(clap_event_param_value_t))
      {
        storage->param_events.push_back(
            *reinterpret_cast<const clap_event_param_value_t*>(event));
        return true;
      }
      return false;
    }};

    process.in_events = &evs;
    process.out_events = &o_evs;

    plug->process(plug, &process);

    // In poly mode, we have to save the parameter changes from the first node and replicate
    // it to the other nodes to handle the case where the user moves something in the UI.
    auto params = this->handle->ext_params;
    if(params && current_channel == 0)
    {
      const int orig = input_storage.param_events.size();
      int cur = input_storage.param_events.size();
      const int param_count = params->count(plug);
      if(param_count > 0)
      {
        input_storage.param_events.resize(
            input_storage.param_events.size() + param_count);
        clap_event_param_value_t* cur_p = &input_storage.param_events[cur];
        for(auto& p : m_param_ins)
        {
          double current_value;
          // Read current parameter value from plugin
          if(params->get_value(plug, p.id, &current_value))
          {
            clap_event_param_value_t& param_event = *cur_p;
            param_event.header.size = sizeof(clap_event_param_value_t);
            param_event.header.time = 0; // Beginning of buffer for now
            param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            param_event.header.type = CLAP_EVENT_PARAM_VALUE;
            param_event.header.flags = CLAP_EVENT_IS_LIVE;
            param_event.param_id = p.id;
            param_event.cookie = p.cookie;
            param_event.note_id = -1;
            param_event.port_index = -1;
            param_event.channel = -1;
            param_event.key = -1;
            param_event.value = current_value;
            ++cur;
            ++cur_p;
          }
        }

        if(cur > orig)
        {
          auto it = std::upper_bound(
              input_storage.all_events.begin(), input_storage.all_events.end(), 0,
              [](auto& t1, auto& ev2) { return t1 < ev2->time; });
          const auto r = std::span<clap_event_param_value_t>(
                             input_storage.param_events.data() + orig,
                             input_storage.param_events.data() + cur)
                         | std::views::transform([](clap_event_param_value_t& elem) {
            return &elem.header;
          });
          input_storage.all_events.insert(it, std::begin(r), std::end(r));
        }
      }
    }
  }

  PluginHandle& m_instance;
  std::vector<poly_plugin> m_poly;
  std::string m_plugin_id;

  // Lock-free channel for the main-thread grower (Executor::grow_pool_tick)
  // to hand fully-constructed instances to the audio thread. Single
  // producer (the QTimer slot) / single consumer (run()), but mpmc is fine
  // and what the rest of the codebase uses.
  ossia::mpmc_queue<poly_plugin> m_incoming;

  // Highest poly count the audio thread has ever needed for this node.
  // The main-thread grower reads this and creates additional instances
  // until the queue + m_poly reach this size.
  std::atomic<std::size_t> m_requested_pool{0};
};

class clap_node_mono_32 final : public clap_node_mono
{
  std::vector<float> input_channel_storage;
  std::vector<float> output_channel_storage;

public:
  using clap_node_mono::clap_node_mono;

  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(!m_instance.plugin)
      return;

    auto [offset, samples] = e.timings(t);
    if(samples == 0)
      return;

    m_current_transport = make_transport(t, e);

    // Clear previous data
    input_channel_storage.clear();
    output_channel_storage.clear();
    input_channel_storage.resize(samples);
    output_channel_storage.resize(samples);

    prepare_input_events(samples);

    float* ins_pointer[1]{input_channel_storage.data()};
    float* outs_pointer[1]{output_channel_storage.data()};

    // Setup audio input buffers
    const auto audio_in = audio_ins[0];
    const auto audio_out = audio_outs[0];
    auto& in_channels = audio_in->data.get();
    const auto upstream_channels = audio_in->data.channels();
    if(upstream_channels == 0)
      return;
    // Pick up any newly-created polyphonic instances handed to us by the
    // Executor's main-thread grower, then announce our current need so it
    // can keep growing the pool. The audio thread never creates plug-ins
    // itself (CLAP requires create_plugin/init on the main thread).
    drain_incoming();
    request_pool_size(upstream_channels);
    const std::size_t poly_channels
        = std::min<std::size_t>(upstream_channels, m_poly.size());
    // FIXME constant mode of audio inputs
    if(std::all_of(
           audio_in->data.begin(), audio_in->data.end(),
           [](const auto& channel) { return channel.size() == 0; }))
      return;
    audio_out->data.set_channels(poly_channels);
    auto& out_channels = audio_out->data.get();
    const uint32_t buffer_size = e.bufferSize();
    clap_audio_buffer_t in_buffer = dummy_audio_buffer();
    clap_audio_buffer_t out_buffer = dummy_audio_buffer();
    in_buffer.channel_count = 1;
    in_buffer.data32 = ins_pointer;
    out_buffer.channel_count = 1;
    out_buffer.data32 = outs_pointer;

    for(std::size_t current_channel = 0; current_channel < poly_channels;
        ++current_channel)
    {
      // 0. Activate plug-in if necessary
      auto& cur = m_poly[current_channel];
      if(!cur.activated)
        return;

      if(!cur.processing)
      {
        cur.processing = start_plugin(cur.plugin);
        if(!cur.processing)
          return;
      }

      auto& channel = in_channels[current_channel];

      // 1. Copy input (extend the upstream buffer to the full block size in
      //    case the producer wrote fewer frames than the engine block).
      {
        channel.resize(std::max<std::size_t>(channel.size(), buffer_size));
        SCORE_ASSERT(input_channel_storage.size() >= samples);
        const double* src = channel.data() + offset;
        float* dst = input_channel_storage.data();
        for(uint32_t s = 0; s < samples; ++s)
          dst[s] = static_cast<float>(src[s]);
      }

      // 2. Clear output
      std::fill_n(output_channel_storage.data(), samples, 0);

      // 3. Process this channel
      clap_process_t process{};
      process.frames_count = samples;
      process.audio_inputs = &in_buffer;
      process.audio_outputs = &out_buffer;
      process.audio_inputs_count = 1;
      process.audio_outputs_count = 1;
      do_process(cur, process, m_input_events, m_output_events, current_channel);

      // 4. Copy float back to matching output channel. Resize to the full
      //    block size first — writing samples frames starting at `offset`
      //    would otherwise overflow when `offset > 0`.
      {
        auto& out_channel = out_channels[current_channel];
        out_channel.resize(std::max<std::size_t>(out_channel.size(), buffer_size));
        const float* src = output_channel_storage.data();
        double* dst = out_channel.data() + offset;
        for(uint32_t s = 0; s < samples; ++s)
          dst[s] = static_cast<double>(src[s]);
      }
    }

    dispatch_param_outputs();
  }
};

class clap_node_mono_64 final : public clap_node_mono
{
public:
  using clap_node_mono::clap_node_mono;
  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(!m_instance.plugin)
      return;

    auto [offset, samples] = e.timings(t);
    if(samples == 0)
      return;

    m_current_transport = make_transport(t, e);

    prepare_input_events(samples);

    double* ins_pointer[1]{};
    double* outs_pointer[1]{};

    // Setup audio input buffers
    const auto audio_in = audio_ins[0];
    const auto audio_out = audio_outs[0];
    const auto upstream_channels = audio_in->data.channels();
    if(upstream_channels == 0)
      return;
    // See clap_node_mono_32::run — never create_plugin/init from here;
    // ask the main-thread grower to do it and use whatever it's handed us
    // so far.
    drain_incoming();
    request_pool_size(upstream_channels);
    const std::size_t poly_channels
        = std::min<std::size_t>(upstream_channels, m_poly.size());
    // FIXME constant mode of audio inputs
    if(std::all_of(
           audio_in->data.begin(), audio_in->data.end(),
           [](const auto& channel) { return channel.size() == 0; }))
      return;
    const uint32_t buffer_size = e.bufferSize();
    for(auto& channel : audio_in->data.get())
    {
      channel.resize(std::max<std::size_t>(channel.size(), buffer_size));
    }
    audio_out->data.set_channels(poly_channels);
    auto& out_channels = audio_out->data.get();
    for(std::size_t c = 0; c < poly_channels; ++c)
    {
      out_channels[c].resize(
          std::max<std::size_t>(out_channels[c].size(), buffer_size));
    }
    clap_audio_buffer_t in_buffer = dummy_audio_buffer();
    clap_audio_buffer_t out_buffer = dummy_audio_buffer();
    in_buffer.channel_count = 1;
    in_buffer.data64 = ins_pointer;
    out_buffer.channel_count = 1;
    out_buffer.data64 = outs_pointer;

    auto& in_channels = audio_in->data.get();
    for(std::size_t current_channel = 0; current_channel < poly_channels;
        ++current_channel)
    {
      // 0. Activate plug-in if necessary
      auto& cur = m_poly[current_channel];
      if(!cur.activated)
        return;

      if(!cur.processing)
      {
        cur.processing = start_plugin(cur.plugin);
        if(!cur.processing)
          return;
      }

      // 1. Point the plugin at the right slice of the buffer.
      //    The previous code passed channel.data()/out_channel.data() with
      //    no `offset`, which made the plugin process frames 0..samples
      //    regardless of where the current tick actually started.
      ins_pointer[0] = const_cast<double*>(in_channels[current_channel].data())
                       + offset;
      auto* out_chan = out_channels[current_channel].data() + offset;
      outs_pointer[0] = out_chan;
      std::fill_n(out_chan, samples, 0.0);

      // 2. Process this channel
      clap_process_t process{};
      process.frames_count = samples;
      process.audio_inputs = &in_buffer;
      process.audio_outputs = &out_buffer;
      process.audio_inputs_count = 1;
      process.audio_outputs_count = 1;
      do_process(cur, process, m_input_events, m_output_events, current_channel);
    }

    dispatch_param_outputs();
  }
};

struct clap_process final : public ossia::node_process
{
  using ossia::node_process::node_process;
  void start() override { }
  void stop() override
  {
    auto* clap = static_cast<clap_node*>(this->node.get());

    if(clap->m_processing)
    {
      clap->stop_plugin(clap->m_instance.plugin);
      clap->m_processing = false;
    }
  }
  void pause() override { }
  void resume() override { }
};
struct clap_mono_process final : public ossia::node_process
{
  using ossia::node_process::node_process;
  void start() override { }
  void stop() override
  {
    auto* clap = static_cast<clap_node_mono*>(this->node.get());

    // Stop each polyphonic instance individually. The old version always
    // stopped m_instance.plugin (m_poly[0]) once per processing entry,
    // leaving instances 1..N in {active, processing} on shutdown, which
    // violates the CLAP state machine.
    for(auto& plug : clap->m_poly)
    {
      if(plug.processing && plug.plugin)
      {
        plug.plugin->stop_processing(plug.plugin);
        plug.processing = false;
      }
    }
  }
  void pause() override { }
  void resume() override { }
};

Executor::Executor(Clap::Model& proc, const Execution::Context& ctx, QObject* parent)
    : Execution::ProcessComponent_T<Clap::Model, ossia::node_process>{
          proc, ctx, "ClapComponent", parent}
{
  proc.setExecuting(true);

  const auto& h = proc.handle();
  if(!h)
    throw std::runtime_error("Plug-in unavailable");

  const bool monophonic = proc.midiInputs().size() == 0 && proc.audioInputs().size() == 1
                          && proc.audioInputs()[0].channel_count == 1
                          && proc.audioOutputs().size() == 1
                          && proc.audioOutputs()[0].channel_count == 1;

  auto& e = *ctx.execState;
  std::shared_ptr<clap_node_base> clap{};
  if(monophonic)
  {
    if(proc.supports64())
    {
      qDebug() << "CLAP: clap_node_mono_64";
      auto node = ossia::make_node<clap_node_mono_64>(
          *ctx.execState, proc, *h, e.sampleRate, e.bufferSize);
      clap = node;
      this->node = node;
      m_ossia_process = std::make_shared<clap_mono_process>(node);
    }
    else
    {
      qDebug() << "CLAP: clap_node_mono_32";
      auto node = ossia::make_node<clap_node_mono_32>(
          *ctx.execState, proc, *h, e.sampleRate, e.bufferSize);
      clap = node;
      this->node = node;
      m_ossia_process = std::make_shared<clap_mono_process>(node);
    }

    // Drive the polyphonic-pool grower on the main thread. The mono node
    // pre-allocates 8 instances in its constructor; if the upstream channel
    // count exceeds that the audio thread bumps m_requested_pool and this
    // timer creates more on the main thread (CLAP requires create_plugin /
    // init / activate / state.load to all run there).
    constexpr std::size_t kInitialPoly = 8;
    m_pool_sample_rate = e.sampleRate;
    m_pool_buffer_size = e.bufferSize;
    m_pool_pushed = kInitialPoly;
    m_pool_max_requested = kInitialPoly;
    m_grow_timer = new QTimer(this);
    m_grow_timer->setInterval(50);
    connect(m_grow_timer, &QTimer::timeout, this, [this] { grow_pool_tick(); });
    m_grow_timer->start();
  }
  else
  {
    if(proc.supports64())
    {
      qDebug() << "CLAP: clap_node_64";
      auto node = ossia::make_node<clap_node_64>(
          *ctx.execState, proc, *h, e.sampleRate, e.bufferSize);
      clap = node;
      this->node = node;
      m_ossia_process = std::make_shared<clap_process>(node);
    }
    else
    {
      qDebug() << "CLAP: clap_node_32";
      auto node = ossia::make_node<clap_node_32>(
          *ctx.execState, proc, *h, e.sampleRate, e.bufferSize);
      clap = node;
      this->node = node;
      m_ossia_process = std::make_shared<clap_process>(node);
    }
  }

  SCORE_ASSERT(clap);

  // Connect control inlet changes to the executor
  // Note: only reelvant for the polyphonic mode as the main mode is done on the
  // main thread
  std::size_t control_idx = 0;
  for(auto* inlet : proc.inlets())
  {
    if(auto* control = qobject_cast<Process::ControlInlet*>(inlet))
    {
      auto* inl = clap->parameter_ins[control_idx];

      control->setupExecution(*inl, this);
      connect(
          control, &Process::ControlInlet::valueChanged, this,
          [this, inl](const ossia::value& v) {
        if(this->process().currentlyReadingValues)
          return;
        auto weak_self = std::weak_ptr{this->node};
        in_exec([inl, val = v, node = weak_self]() mutable {
          if(auto n = node.lock())
          {
            inl->target<ossia::value_port>()->write_value(std::move(val), 0);
          }
        });
      });
      control_idx++;
    }
  }
}

void Executor::grow_pool_tick()
{
  auto mono = std::dynamic_pointer_cast<clap_node_mono>(this->node);
  if(!mono)
    return;

  // Promote brief spikes into the long-term high-water mark so we don't
  // miss a 2 → 512 → 2 transient.
  const auto requested
      = mono->m_requested_pool.load(std::memory_order_acquire);
  if(requested > m_pool_max_requested)
    m_pool_max_requested = requested;

  if(m_pool_pushed >= m_pool_max_requested)
    return;

  // Snapshot the primary plug-in's state once per growth burst so newly
  // created instances inherit loaded models / presets (AIDA-X et al.).
  // Re-captured on each call to track ongoing edits while we're growing.
  auto handle = this->process().handle();
  if(!handle || !handle->plugin)
    return;
  const QByteArray state = snapshot_clap_state(handle->plugin);

  // Create at most a few per tick — each new instance can be expensive
  // (AIDA-X loads a neural model on state.load) and we don't want to
  // stall the UI thread. The 50 ms tick + batch of 4 gives ~80
  // instances/sec, fast enough to grow 8 → 512 in ~6 s while remaining
  // responsive.
  constexpr std::size_t kBatch = 4;
  const std::size_t to_add
      = std::min(kBatch, m_pool_max_requested - m_pool_pushed);
  for(std::size_t i = 0; i < to_add; ++i)
  {
    try
    {
      auto p
          = mono->make_poly_instance(m_pool_sample_rate, m_pool_buffer_size, state);
      if(!mono->m_incoming.enqueue(p))
      {
        // Queue's heap alloc failed: roll back this instance and stop
        // growing for this tick — we'll try again on the next one.
        if(p.activated && p.plugin)
          p.plugin->deactivate(p.plugin);
        if(p.plugin)
          p.plugin->destroy(p.plugin);
        qWarning() << "CLAP: poly queue enqueue failed; will retry";
        break;
      }
      ++m_pool_pushed;
    }
    catch(const std::exception& ex)
    {
      qWarning() << "CLAP: failed to grow polyphonic pool:" << ex.what();
      // Give up trying to reach the current target; future requests
      // larger than what we've already pushed will retry naturally.
      break;
    }
  }
}
}
