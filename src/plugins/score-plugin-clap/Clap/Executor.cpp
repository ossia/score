#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>

#include <libremidi/detail/conversion.hpp>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Clap
{
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
  Clap::PluginHandle& handle;
  explicit clap_node_base(const Clap::Model& proc)
      : handle{*proc.handle()}
      , m_param_ins{proc.parameterInputs()}
      , m_param_outs{proc.parameterOutputs()}
      , m_midi_ins{proc.midiInputs()}
      , m_midi_outs{proc.midiOutputs()}
  {
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
      else if(qobject_cast<Process::ControlInlet*>(inlet))
      {
        parameter_ins.push_back(new ossia::value_inlet);
        m_inlets.push_back(parameter_ins.back());
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

  virtual void reset_execution() = 0;

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
      param_event.value = param_info.default_value; // FIXME use value set in score

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
          param_event.header.flags = 0;
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
    m_input_events.param_events.reserve(param_event_count * 1.1);
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
  }

  ~clap_node()
  {
    // We do not deactivate in the audio thread where the dtor of clap_node runs
    // but later in the main thread
  }

  void reset_execution() override
  {
    // FIXME wrong thread
    if(m_activated)
    {
      (void)deactivate_plugin(m_instance.plugin);
      m_activated = false;
    }
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
      if(event->type == CLAP_EVENT_MIDI && event->size == sizeof(clap_event_midi_t))
      {
        storage->midi_events.push_back(
            *reinterpret_cast<const clap_event_midi_t*>(event));
        return true;
      }
      // FIXME else if note...
      return false;
    }};

    process.in_events = &evs;
    process.out_events = &o_evs;

    m_instance.plugin->process(m_instance.plugin, &process);

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
      const auto& channels = audio_in->data.get();
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
          if(c < channels.size() && channels[c].size() >= samples + offset)
          {
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
    for(ossia::audio_outlet* audio_out : audio_outs)
    {
      const auto& audio_info = this->m_expected_audio_outputs[audio_out_idx];
      clap_audio_buffer_t buffer = dummy_audio_buffer();
      buffer.channel_count = audio_info.channel_count;

      audio_out->data.set_channels(buffer.channel_count);
      auto& channels = audio_out->data.get();

      // Allocate float storage for output
      auto& storage = output_channel_storage.emplace_back();
      storage.resize(buffer.channel_count);

      auto& ptrs = output_channel_ptrs.emplace_back();
      ptrs.resize(buffer.channel_count);

      storage.clear();
      storage.resize(samples * buffer.channel_count + 16);
      for(uint32_t c = 0; c < buffer.channel_count; ++c)
      {
        channels[c].resize(e.bufferSize());
        ptrs[c] = storage.data() + c * samples;
      }

      buffer.data32 = ptrs.data();
      output_buffers.push_back(buffer);
      audio_out_idx++;
    }

    // Process audio through CLAP plugin
    clap_process_t process{};
    process.frames_count = samples;
    process.audio_inputs = input_buffers.data();
    process.audio_outputs = output_buffers.data();
    // Ensure we have exactly the number of buffers the plugin expects
    while(input_buffers.size() < m_expected_audio_inputs.size())
    {
      input_buffers.push_back(dummy_audio_buffer());
    }

    while(output_buffers.size() < m_expected_audio_outputs.size())
    {
      output_buffers.push_back(dummy_audio_buffer());
    }

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

        for(uint32_t c = 0;
            c < std::min((uint32_t)channels.size(), (uint32_t)storage.size()); ++c)
        {
          const float* src = storage.data() + c * samples;
          double* dst = channels[c].data() + offset;
          for(uint32_t s = 0; s < samples; ++s)
          {
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
      const auto& channels = audio_in->data.get();
      if(!channels.empty())
      {
        buffer.channel_count = std::min((uint32_t)channels.size(), 2u);

        auto& ptrs = input_channel_ptrs.emplace_back();
        ptrs.resize(buffer.channel_count);

        for(uint32_t c = 0; c < buffer.channel_count; ++c)
        {
          if(c < channels.size() && channels[c].size() >= samples)
          {
            ptrs[c] = const_cast<double*>(channels[c].data() + offset);
          }
          else
          {
            ptrs[c] = nullptr;
          }
        }

        buffer.data64 = ptrs.data();
      }

      this->input_buffers.push_back(buffer);
      audio_in_idx++;
    }

    // Setup output buffers
    std::size_t audio_out_idx = 0;
    for(ossia::audio_outlet* audio_out : audio_outs)
    {
      const auto& audio_info = this->m_expected_audio_outputs[audio_out_idx];
      clap_audio_buffer_t buffer = dummy_audio_buffer();
      buffer.channel_count = audio_info.channel_count;

      audio_out->data.set_channels(buffer.channel_count);
      auto& channels = audio_out->data.get();

      auto& ptrs = output_channel_ptrs.emplace_back();
      ptrs.resize(buffer.channel_count);

      for(uint32_t c = 0; c < buffer.channel_count; ++c)
      {
        channels[c].resize(e.bufferSize());
        ptrs[c] = channels[c].data() + offset;
      }

      buffer.data64 = ptrs.data();
      this->output_buffers.push_back(buffer);
      audio_out_idx++;
    }

    clap_process_t process{};
    process.frames_count = samples;
    process.audio_inputs = this->input_buffers.data();
    process.audio_outputs = this->output_buffers.data();
    while(this->input_buffers.size() < m_expected_audio_inputs.size())
      this->input_buffers.push_back(dummy_audio_buffer());

    while(this->output_buffers.size() < m_expected_audio_outputs.size())
      this->input_buffers.push_back(dummy_audio_buffer());

    process.audio_inputs_count = m_expected_audio_inputs.size();
    process.audio_outputs_count = m_expected_audio_outputs.size();
    do_process(process, m_input_events, m_output_events);
  }
};

// Special case for monophonic nodes

class clap_node_mono : public clap_node_base
{
public:
  clap_node_mono(
      const Clap::Model& proc, Clap::PluginHandle& handle, int sampleRate, int bs)
      : clap_node_base{proc}
      , m_instance{handle}
      , m_plugin_id{proc.pluginId().toUtf8()}
  {
    m_poly.reserve(8);
    m_poly.push_back({handle.plugin, false});
    if(handle.activated)
      (void)deactivate_plugin(handle.plugin);
    m_poly.back().activated = activate_plugin(m_instance.plugin, sampleRate, bs);

    for(int i = 0; i < 7; i++)
    {
      add_poly_instance(sampleRate, bs);
    }
  }

  void add_poly_instance(int sampleRate, int bs)
  {
    poly_plugin p{nullptr, false};
    p.plugin = m_instance.factory->create_plugin(
        m_instance.factory, &m_instance.host, m_plugin_id.data());
    if(!p.plugin)
      throw std::runtime_error("Could not create plug-in instance");

    if(!p.plugin->init(p.plugin))
    {
      p.plugin->destroy(p.plugin);
      throw std::runtime_error("Could not init plug-in instance");
    }
    p.activated = activate_plugin(p.plugin, sampleRate, bs);
    m_poly.push_back(p);
  }

  ~clap_node_mono()
  {
    for(int i = 0; i < m_poly.size(); i++)
    {
      if(m_poly[i].processing)
      {
        stop_plugin(m_poly[i].plugin);
        m_poly[i].processing = false;
      }
    }

    // FIXME not RT-friendly
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), [poly = std::move(m_poly)]() {
      for(int i = 1; i < poly.size(); i++)
      {
        auto& p = poly[i];
        if(p.activated)
          if(p.plugin)
            p.plugin->deactivate(p.plugin);

        p.plugin->destroy(p.plugin);
      }
    });
  }

  void reset_execution() override
  {
    // FIXME
    for(int i = 0; i < m_poly.size(); i++)
    {
      if(m_poly[i].activated)
      {
        (void)deactivate_plugin(m_poly[i].plugin);
        m_poly[i].activated = false;
      }
    }
  }

  void do_process(
      const clap_plugin_t* plug, clap_process_t& process, event_storage& input_storage,
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

    // No output events in poly mode, it does not make sense
    clap_output_events_t o_evs{
        .ctx = &output_storage,
        .try_push = [](const struct clap_output_events* list,
                       const clap_event_header_t* event) -> bool { return false; }};

    process.in_events = &evs;
    process.out_events = &o_evs;

    plug->process(plug, &process);
  }

  PluginHandle& m_instance;
  struct poly_plugin
  {
    const clap_plugin_t* plugin{};
    bool activated{};
    bool processing{};
    operator const clap_plugin_t*() const noexcept { return plugin; }
  };

  std::vector<poly_plugin> m_poly;
  std::string m_plugin_id;
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
    const auto& in_channels = audio_in->data.get();
    auto& out_channels = audio_out->data.get();
    const auto poly_channels = audio_in->data.channels();
    if(poly_channels == 0)
      return;
    // FIXME should be in main thread
    while(m_poly.size() < poly_channels)
      add_poly_instance(e.sampleRate(), e.bufferSize());
    // FIXME constant mode of audio inputs
    if(std::all_of(
           audio_in->data.begin(), audio_in->data.end(),
           [](const auto& channel) { return channel.size() == 0; }))
      return;
    audio_out->data.set_channels(poly_channels);
    clap_audio_buffer_t in_buffer = dummy_audio_buffer();
    clap_audio_buffer_t out_buffer = dummy_audio_buffer();
    in_buffer.channel_count = 1;
    in_buffer.data32 = ins_pointer;
    out_buffer.channel_count = 1;
    out_buffer.data32 = outs_pointer;

    int current_channel = 0;
    for(auto& channel : in_channels)
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

      // 1. Copy input
      {
        const double* src = channel.data() + offset;
        float* dst = input_channel_storage.data();
        SCORE_ASSERT(channel.size() >= samples);
        SCORE_ASSERT(input_channel_storage.size() >= samples);
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
      do_process(cur, process, m_input_events, m_output_events);

      // 4. Copy double back to matching output channel
      {
        const float* src = output_channel_storage.data();
        out_channels[current_channel].resize(samples);
        double* dst = out_channels[current_channel].data() + offset;
        for(uint32_t s = 0; s < samples; ++s)
          dst[s] = static_cast<double>(src[s]);
      }

      current_channel++;
    }
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

    // Clear previous data
    prepare_input_events(samples);

    double* ins_pointer[1]{};
    double* outs_pointer[1]{};

    // Setup audio input buffers
    const auto audio_in = audio_ins[0];
    const auto audio_out = audio_outs[0];
    const auto poly_channels = audio_in->data.channels();
    if(poly_channels == 0)
      return;
    // FIXME should be in main thread
    while(m_poly.size() < poly_channels)
      add_poly_instance(e.sampleRate(), e.bufferSize());
    // FIXME constant mode of audio inputs
    if(std::all_of(
           audio_in->data.begin(), audio_in->data.end(),
           [](const auto& channel) { return channel.size() == 0; }))
      return;
    audio_out->data.set_channels(poly_channels);
    clap_audio_buffer_t in_buffer = dummy_audio_buffer();
    clap_audio_buffer_t out_buffer = dummy_audio_buffer();
    in_buffer.channel_count = 1;
    in_buffer.data64 = ins_pointer;
    out_buffer.channel_count = 1;
    out_buffer.data64 = outs_pointer;

    const auto& in_channels = audio_in->data.get();
    auto& out_channels = audio_out->data.get();
    int current_channel = 0;
    for(auto& channel : in_channels)
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

      // 1. Clear output
      auto in_chan = channel.data();
      ins_pointer[0] = const_cast<double*>(in_chan);
      out_channels[current_channel].resize(samples);
      auto out_chan = out_channels[current_channel].data();
      outs_pointer[0] = out_chan;
      std::fill_n(out_chan, samples, 0);

      // 2. Process this channel
      clap_process_t process{};
      process.frames_count = samples;
      process.audio_inputs = &in_buffer;
      process.audio_outputs = &out_buffer;
      process.audio_inputs_count = 1;
      process.audio_outputs_count = 1;
      do_process(cur, process, m_input_events, m_output_events);

      current_channel++;
    }
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

    for(auto& plug : clap->m_poly)
      if(plug.processing)
      {
        clap->stop_plugin(clap->m_instance.plugin);
        plug.processing = false;
      }
  }
  void pause() override { }
  void resume() override { }
};

Executor::Executor(Clap::Model& proc, const Execution::Context& ctx, QObject* parent)
    : Execution::ProcessComponent_T<Clap::Model, ossia::node_process>{
          proc, ctx, "ClapComponent", parent}
{
  auto h = proc.handle();
  if(!h)
    throw std::runtime_error("Plug-in unavailable");

  const bool monophonic
      = proc.audioInputs().size() == 1 && proc.audioInputs()[0].channel_count == 1
        && proc.audioOutputs().size() == 1 && proc.audioOutputs()[0].channel_count == 1;

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
        auto weak_self = std::weak_ptr{this->node};
        in_exec([inl, val = v, node = weak_self]() mutable {
          if(auto n = node.lock())
            inl->target<ossia::value_port>()->write_value(std::move(val), 0);
        });
      });
      control_idx++;
    }
  }
  /*
  connect(
      inlet, &Process::ControlInlet::valueChanged, this,
      [&params, i = index, this](const ossia::value& v) {
    if(this->m_executing)
      return;
    auto plugin = this->handle()->plugin;
    SCORE_ASSERT(this->parameterInputs().size() > i);
    auto& param_info = this->parameterInputs()[i];
    double val = ossia::convert<double>(v);
    
    clap_event_param_value_t param_event{};
    param_event.header.size = sizeof(clap_event_param_value_t);
    param_event.header.time = 0; // Beginning of buffer for now
    param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    param_event.header.type = CLAP_EVENT_PARAM_VALUE;
    param_event.header.flags = 0;
    param_event.param_id = param_info.id;
    param_event.cookie = param_info.cookie;
    param_event.note_id = -1;
    param_event.port_index = -1;
    param_event.channel = -1;
    param_event.key = -1;
    param_event.value = val;
    
    clap_input_events_t ip;
    ip.ctx = &param_event;
    ip.get = +[](const struct clap_input_events* list,
                 uint32_t index) -> const clap_event_header_t* {
      if(index == 0)
      {
        return (const clap_event_header_t*)&list->ctx;
      }
      return nullptr;
    };
    ip.size = +[](const struct clap_input_events* list) -> uint32_t { return 1; };
    
    clap_output_events_t op{
                            .ctx = nullptr,
                            .try_push = [](const struct clap_output_events* list,
                                           const clap_event_header_t* event) { return false; }};
    params.flush(plugin, &ip, &op);
  });
  */
  // Connect flush message
  connect(&proc, &Clap::Model::requestFlush, this, [&] {
    in_exec([plugin = proc.handle()->plugin] {
      auto params
          = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
      if(!params)
        return;
      if(!params->flush)
        return;
      clap_input_events_t ip;
      ip.ctx = nullptr;
      ip.get = +[](const struct clap_input_events* list,
                   uint32_t index) -> const clap_event_header_t* { return nullptr; };
      ip.size = +[](const struct clap_input_events* list) -> uint32_t { return 0; };

      clap_output_events_t op{
          .ctx = nullptr,
          .try_push = [](const struct clap_output_events* list,
                         const clap_event_header_t* event) { return false; }};

      params->flush(plugin, &ip, &op);
    });
  });

  // Connect the restart signal
  // FIXME threads are wrong
  // connect(
  //     &proc, &Process::ProcessModel::resetExecution, this,
  //     [this, weak_self = std::weak_ptr{clap}] {
  //   in_exec([weak_self]() mutable {
  //     if(auto n = weak_self.lock())
  //     {
  //       n->reset_execution();
  //     }
  //   });
  // });

  // Connect parameter value feedback from executor to GUI
  auto c = connect(
      &ctx.doc.coarseUpdateTimer, &QTimer::timeout, this,
      [weak_clap = std::weak_ptr{clap}, &proc] {
    if(auto node = weak_clap.lock())
    {
      const clap_plugin_t* plugin = nullptr;
      
      // Get the plugin instance depending on node type
      if(auto regular_node = std::dynamic_pointer_cast<clap_node>(node))
      {
        plugin = proc.handle()->plugin;
      }
      else if(auto mono_node = std::dynamic_pointer_cast<clap_node_mono>(node))
      {
        plugin = proc.handle()->plugin;
      }
      
      if(plugin)
      {
        // Get parameter extension to read current values
        auto params = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
        if(params)
        {
          std::size_t control_idx = 0;
          for(auto* inlet : proc.inlets())
          {
            if(auto* control = qobject_cast<Process::ControlInlet*>(inlet))
            {
              if(control_idx < node->m_param_ins.size())
              {
                const auto& param_info = node->m_param_ins[control_idx];
                double current_value = 0.0;
                
                // Read current parameter value from plugin
                if(params->get_value(plugin, param_info.id, &current_value))
                {
                  control->setExecutionValue(current_value);
                }
              }
              control_idx++;
            }
          }
        }
      }
    }
  });
}
}
