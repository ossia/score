#pragma once
#include <LV2/Context.hpp>
#include <LV2/lv2_atom_helpers.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/fmt.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>

#include <libremidi/detail/conversion.hpp>

#include <QCoreApplication>

#include <lilv/lilv.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

namespace LV2
{

// One LilvInstance + per-voice scratch / atom buffers. Pinned (TimePort holds pointers).
struct voice
{
  SharedInstance instance_holder;
  LilvInstance* instance{}; // cached: instance_holder->instance

  ossia::float_vector fInControls;
  ossia::float_vector fOutControls;
  ossia::float_vector fOtherControls;
  std::vector<ossia::float_vector> fCVs;

  std::vector<ossia::float_vector> audio_in_scratch;
  std::vector<ossia::float_vector> audio_out_scratch;

  std::vector<AtomBuffer> midi_atom_ins;
  std::vector<AtomBuffer> midi_atom_outs;
  std::vector<libremidi::midi2_to_midi1> midi_2to1;
  std::vector<libremidi::midi1_to_midi2> midi_1to2;
  std::vector<ossia::small_vector<Message, 2>> message_for_midi_atom_ins;

  std::vector<AtomBuffer> atom_ins;
  std::vector<AtomBuffer> atom_outs;
  std::vector<ossia::small_vector<Message, 2>> message_for_atom_ins;

  struct TimePort
  {
    int port;
    AtomBuffer* buffer{};
  };
  std::vector<TimePort> atom_timePosition_midi;
  std::vector<AtomBuffer> atom_timePosition_owned_buffers;
  std::vector<TimePort> atom_timePosition_owned;

  const LV2_Worker_Interface* worker{};
  ossia::mpmc_queue<std::vector<char>> worker_datas;

  voice() = default;
  voice(const voice&) = delete;
  voice& operator=(const voice&) = delete;
  voice(voice&&) = delete;
  voice& operator=(voice&&) = delete;

  // Voice 0's instance survives via Model's shared ref and is re-activated next play
  ~voice() noexcept
  {
    if(instance_holder && instance && instance_holder->activated)
    {
      lilv_instance_deactivate(instance);
      instance_holder->activated = false;
    }
  }
};

enum class voice_routing : std::uint8_t
{
  single,
  // N voices, channel c -> voice c (mono plug-in on multichannel bus)
  per_channel
};

struct voice_strategy
{
  voice_routing routing{voice_routing::single};
  std::size_t voice_count{1};
};

// 1-in/1-out plug-ins get N replicated voices for multichannel host buses
inline voice_strategy
choose_voice_strategy(const LV2Data& data, std::size_t replicate_count = 8) noexcept
{
  const auto ains = data.audio_in_ports.size();
  const auto aouts = data.audio_out_ports.size();
  if(ains == 1 && aouts == 1 && replicate_count > 1)
    return {voice_routing::per_channel, replicate_count};
  return {voice_routing::single, 1};
}

template <typename OnExecStart, typename OnExecFinished>
struct lv2_node final : public ossia::graph_node
{
  LV2Data data;
  std::vector<std::unique_ptr<voice>> voices;
  voice_routing routing{voice_routing::single};

  ossia::float_vector fParamMin, fParamMax, fParamInit;
  std::unique_ptr<uint8_t[]> timePositionBuffer{};

  // Matches LV2_BUF_SIZE__maxBlockLength advertised in Context.cpp
  std::size_t max_block_size{4096};

  // Audio-thread high-water mark; monotonic; main-thread grower reads via in_exec
  std::atomic<std::size_t> requested_voices{0};

  OnExecStart on_start;
  OnExecFinished on_finished;

  // Avoids RT-unfriendly realloc in append_voice for common channel counts
  static constexpr std::size_t kVoicePoolReserve = 256;

  lv2_node(
      const LV2Data& dat, int sampleRate, voice_strategy strat, OnExecStart os,
      OnExecFinished of)
      : data{dat}
      , routing{strat.routing}
      , on_start{os}
      , on_finished{of}
  {
    this->set_not_fp_safe();

    if(strat.voice_count < 1)
      strat.voice_count = 1;

    const auto audio_in_size = data.audio_in_ports.size();
    const auto audio_out_size = data.audio_out_ports.size();
    const auto cv_size = data.cv_ports.size();
    const auto midi_in_size = data.midi_in_ports.size();
    const auto midi_out_size = data.midi_out_ports.size();
    const auto atom_in_size = data.atom_in_ports.size();
    const auto atom_out_size = data.atom_out_ports.size();
    const auto control_in_size = data.control_in_ports.size();
    const auto control_out_size = data.control_out_ports.size();

    if(audio_in_size > 0)
      m_inlets.push_back(new ossia::audio_inlet);
    if(audio_out_size > 0)
      m_outlets.push_back(new ossia::audio_outlet);
    for(std::size_t i = 0; i < cv_size; i++)
      m_inlets.push_back(new ossia::audio_inlet);
    for(std::size_t i = 0; i < midi_in_size; i++)
      m_inlets.push_back(new ossia::midi_inlet);
    for(std::size_t i = 0; i < midi_out_size; i++)
      m_outlets.push_back(new ossia::midi_outlet);
    for(std::size_t i = 0; i < atom_in_size; i++)
      m_inlets.push_back(new ossia::value_inlet);
    for(std::size_t i = 0; i < atom_out_size; i++)
      m_outlets.push_back(new ossia::value_outlet);
    for(std::size_t i = 0; i < control_in_size; i++)
      m_inlets.push_back(new ossia::value_inlet);
    for(std::size_t i = 0; i < control_out_size; i++)
      m_outlets.push_back(new ossia::value_outlet);

    const auto num_ports = data.effect.plugin.get_num_ports();
    fParamMin.resize(num_ports);
    fParamMax.resize(num_ports);
    fParamInit.resize(num_ports);
    data.effect.plugin.get_port_ranges_float(
        fParamMin.data(), fParamMax.data(), fParamInit.data());

    if(!data.effect.instance_holder)
      throw std::runtime_error("Error while creating a LV2 plug-in");

    voices.reserve(
        routing == voice_routing::per_channel ? kVoicePoolReserve : strat.voice_count);

    // Voice 0 reuses the Model's primary instance across play->stop->play
    voices.push_back(build_voice(data.effect.instance_holder));

    // Snapshot so extras inherit loaded samples/models; ports seeded separately below
    LilvState* primary_state = (strat.voice_count > 1) ? snapshot_state_raw() : nullptr;

    // lv2_node is constructed on the main thread; LV2EffectComponent grows past this
    for(std::size_t i = 1; i < strat.voice_count; ++i)
    {
      auto handle = instantiate_handle(sampleRate);
      if(!handle)
        break;
      if(primary_state)
        lilv_state_restore(
            primary_state, handle->instance, nullptr, nullptr,
            LV2_STATE_IS_PORTABLE, nullptr);
      voices.push_back(build_voice(std::move(handle)));
    }

    if(primary_state)
      lilv_state_free(primary_state);

    if(!data.time_Position_ports.empty())
      timePositionBuffer = std::make_unique<uint8_t[]>(256);

    requested_voices.store(voices.size(), std::memory_order_relaxed);
  }

  ~lv2_node() noexcept override
  {
    // May run on audio thread; voice cleanup is main-thread-only per LV2 spec
    if(voices.empty())
      return;
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [pending = std::move(voices)]() mutable { pending.clear(); });
  }

  // [main thread]
  std::unique_ptr<voice> build_voice(SharedInstance handle)
  {
    auto v = std::make_unique<voice>();
    v->instance_holder = std::move(handle);
    v->instance = v->instance_holder->instance;
    allocate_voice_buffers(*v);
    connect_voice_ports(*v);
    activate_voice(*v);
    return v;
  }

  // [main thread]
  SharedInstance instantiate_handle(int sampleRate)
  {
    auto* inst = lilv_plugin_instantiate(
        data.effect.plugin.me, sampleRate,
        data.host.global ? data.host.global->features() : data.host.features);
    if(!inst)
      return {};
    return std::make_shared<InstanceHandle>(inst);
  }

  // [main thread] null on failure; grower retries next tick
  std::unique_ptr<voice>
  make_voice_for_pool(int sampleRate, LilvState* state)
  {
    auto handle = instantiate_handle(sampleRate);
    if(!handle)
      return nullptr;

    if(state)
      lilv_state_restore(
          state, handle->instance, nullptr, nullptr, LV2_STATE_IS_PORTABLE,
          nullptr);

    return build_voice(std::move(handle));
  }

  // [main thread] Caller frees with lilv_state_free
  LilvState* snapshot_state_raw() const noexcept
  {
    if(voices.empty() || !voices[0]->instance || !data.host.global)
      return nullptr;
    return lilv_state_new_from_instance(
        data.effect.plugin.me, voices[0]->instance, &data.host.global->map,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        LV2_STATE_IS_PORTABLE, nullptr);
  }

  // [audio thread] from main-thread grower via in_exec
  void append_voice(std::unique_ptr<voice> v) noexcept
  {
    voices.push_back(std::move(v));
  }

  // [audio thread] Monotonic high-water mark
  void request_voice_count(std::size_t n) noexcept
  {
    auto prev = requested_voices.load(std::memory_order_relaxed);
    while(n > prev
          && !requested_voices.compare_exchange_weak(
              prev, n, std::memory_order_release, std::memory_order_relaxed))
      ;
  }

  void allocate_voice_buffers(voice& v)
  {
    const auto control_in_size = data.control_in_ports.size();
    const auto control_out_size = data.control_out_ports.size();
    const auto cv_size = data.cv_ports.size();
    const auto other_size = data.control_other_ports.size();
    const auto midi_in_size = data.midi_in_ports.size();
    const auto midi_out_size = data.midi_out_ports.size();
    const auto atom_in_size = data.atom_in_ports.size();
    const auto atom_out_size = data.atom_out_ports.size();
    const auto audio_in_size = data.audio_in_ports.size();
    const auto audio_out_size = data.audio_out_ports.size();

    v.fInControls.resize(control_in_size);
    v.fOutControls.resize(control_out_size);
    v.fOtherControls.resize(other_size);

    v.fCVs.resize(cv_size);
    for(auto& cv : v.fCVs)
      cv.resize(max_block_size);

    v.audio_in_scratch.resize(audio_in_size);
    for(auto& a : v.audio_in_scratch)
      a.resize(max_block_size);

    v.audio_out_scratch.resize(audio_out_size);
    for(auto& a : v.audio_out_scratch)
      a.resize(max_block_size);

    v.midi_atom_ins.reserve(midi_in_size);
    v.midi_2to1.resize(midi_in_size);
    v.message_for_midi_atom_ins.resize(midi_in_size);
    for(std::size_t i = 0; i < midi_in_size; i++)
      v.midi_atom_ins.emplace_back(
          2048, data.host.atom_chunk_id, data.host.midi_event_id, true);

    v.midi_atom_outs.reserve(midi_out_size);
    v.midi_1to2.resize(midi_out_size);
    for(std::size_t i = 0; i < midi_out_size; i++)
      v.midi_atom_outs.emplace_back(
          2048, data.host.atom_chunk_id, data.host.midi_event_id, false);

    v.atom_ins.reserve(atom_in_size);
    v.message_for_atom_ins.resize(atom_in_size);
    for(std::size_t i = 0; i < atom_in_size; i++)
      v.atom_ins.emplace_back(
          2048, data.host.atom_chunk_id, data.host.atom_object_id, true);

    v.atom_outs.reserve(atom_out_size);
    for(std::size_t i = 0; i < atom_out_size; i++)
      v.atom_outs.emplace_back(
          2048, data.host.atom_chunk_id, data.host.atom_object_id, false);

    // time:Position may share a MIDI port (atom:supports midi+time) or have its own atom port
    v.atom_timePosition_owned_buffers.reserve(data.time_Position_ports.size());
    for(int port_index : data.time_Position_ports)
    {
      bool shared = false;
      for(std::size_t k = 0; k < data.midi_in_ports.size(); ++k)
      {
        if(data.midi_in_ports[k] == port_index)
        {
          v.atom_timePosition_midi.push_back({port_index, &v.midi_atom_ins[k]});
          shared = true;
          break;
        }
      }
      if(!shared)
      {
        v.atom_timePosition_owned_buffers.emplace_back(
            256, data.host.atom_chunk_id, data.host.time_Position_id, true);
        v.atom_timePosition_owned.push_back(
            {port_index, &v.atom_timePosition_owned_buffers.back()});
      }
    }

    if(lilv_plugin_has_feature(data.effect.plugin.me, data.host.work_schedule)
       && lilv_plugin_has_extension_data(
           data.effect.plugin.me, data.host.work_interface))
    {
      v.worker = static_cast<const LV2_Worker_Interface*>(
          lilv_instance_get_extension_data(v.instance, LV2_WORKER__interface));
    }

    for(std::size_t i = 0; i < control_in_size; i++)
      v.fInControls[i] = fParamInit[data.control_in_ports[i]];

    (void)control_out_size;
    (void)audio_in_size;
    (void)audio_out_size;
  }

  void connect_voice_ports(voice& v) noexcept
  {
    auto* inst = v.instance;
    for(std::size_t i = 0; i < data.control_in_ports.size(); i++)
      lilv_instance_connect_port(inst, data.control_in_ports[i], &v.fInControls[i]);
    for(std::size_t i = 0; i < data.control_out_ports.size(); i++)
      lilv_instance_connect_port(inst, data.control_out_ports[i], &v.fOutControls[i]);
    for(std::size_t i = 0; i < data.cv_ports.size(); i++)
      lilv_instance_connect_port(inst, data.cv_ports[i], v.fCVs[i].data());
    for(std::size_t i = 0; i < data.control_other_ports.size(); i++)
      lilv_instance_connect_port(
          inst, data.control_other_ports[i], &v.fOtherControls[i]);
    for(std::size_t i = 0; i < v.midi_atom_ins.size(); i++)
      lilv_instance_connect_port(
          inst, data.midi_in_ports[i], &v.midi_atom_ins[i].buf->atoms);
    for(std::size_t i = 0; i < v.midi_atom_outs.size(); i++)
      lilv_instance_connect_port(
          inst, data.midi_out_ports[i], &v.midi_atom_outs[i].buf->atoms);
    for(std::size_t i = 0; i < v.atom_ins.size(); i++)
      lilv_instance_connect_port(
          inst, data.atom_in_ports[i], &v.atom_ins[i].buf->atoms);
    for(std::size_t i = 0; i < v.atom_outs.size(); i++)
      lilv_instance_connect_port(
          inst, data.atom_out_ports[i], &v.atom_outs[i].buf->atoms);
    for(auto& [port, buf] : v.atom_timePosition_owned)
      lilv_instance_connect_port(inst, port, &buf->buf->atoms);
  }

  void activate_voice(voice& v) noexcept
  {
    if(v.instance_holder && v.instance && !v.instance_holder->activated)
    {
      lilv_instance_activate(v.instance);
      v.instance_holder->activated = true;
    }
  }

  void all_notes_off() noexcept override
  {
    // CC#120 (All Sound Off) + CC#123 (All Notes Off) on every channel of every voice
    if(data.midi_in_ports.empty())
      return;
    const int midi_port_index = data.midi_in_ports[0];
    for(auto& v : voices)
    {
      if(v->message_for_midi_atom_ins.empty())
        continue;
      auto& msg_queue = v->message_for_midi_atom_ins[0];
      for(uint8_t ch = 0; ch < 16; ++ch)
      {
        for(uint8_t cc : {uint8_t(120), uint8_t(123)})
        {
          Message m;
          m.index = uint32_t(midi_port_index);
          m.protocol = data.host.atom_eventTransfer;
          m.body.resize(sizeof(LV2_Atom) + 3);
          auto* atom = reinterpret_cast<LV2_Atom*>(m.body.data());
          atom->type = data.host.midi_event_id;
          atom->size = 3;
          uint8_t* d = m.body.data() + sizeof(LV2_Atom);
          d[0] = uint8_t(0xB0 | ch);
          d[1] = cc;
          d[2] = 0;
          msg_queue.push_back(std::move(m));
        }
      }
    }
  }

  [[nodiscard]] std::string label() const noexcept override
  {
    return fmt::format("lv2 ({})", data.effect.plugin.get_name().as_string());
  }

  void updateTime(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    LV2::HostContext& host = data.host;
    auto& forge = host.forge;
    uint8_t* buffer = timePositionBuffer.get();
    lv2_atom_forge_set_buffer(&forge, buffer, 256);
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&forge, &frame, 0, host.time_Position_id);

    lv2_atom_forge_key(&forge, host.time_frame_id);
    lv2_atom_forge_long(&forge, this->m_processed_frames);

    lv2_atom_forge_key(&forge, host.time_framesPerSecond_id);
    lv2_atom_forge_long(&forge, st.sampleRate());

    lv2_atom_forge_key(&forge, host.time_speed_id);
    lv2_atom_forge_float(&forge, tk.speed);

    lv2_atom_forge_key(&forge, host.time_bar_id);
    lv2_atom_forge_long(&forge, tk.musical_start_last_bar / 4.);

    lv2_atom_forge_key(&forge, host.time_beat_id);
    lv2_atom_forge_double(&forge, tk.musical_start_position);

    auto barBeat = float(tk.musical_start_position - tk.musical_start_last_bar);
    lv2_atom_forge_key(&forge, host.time_barBeat_id);
    lv2_atom_forge_float(&forge, barBeat);

    lv2_atom_forge_key(&forge, host.time_beatUnit_id);
    lv2_atom_forge_int(&forge, 4);

    lv2_atom_forge_key(&forge, host.time_beatsPerBar_id);
    lv2_atom_forge_float(
        &forge, 4 * double(tk.signature.upper) / double(tk.signature.lower));

    lv2_atom_forge_key(&forge, host.time_beatsPerMinute_id);
    lv2_atom_forge_float(&forge, tk.tempo);

    lv2_atom_forge_pop(&forge, &frame);
  }

  // Vec slices by voice (clamped); scalars broadcast. Mirrors faust_mono_fx::set_control.
  static std::optional<float>
  voice_control_value(const ossia::value& val, std::size_t voice_idx) noexcept
  {
    return ossia::apply_nonnull(
        [voice_idx](const auto& v) -> std::optional<float> {
      using T = std::decay_t<decltype(v)>;
      if constexpr(std::is_same_v<T, std::vector<ossia::value>>)
      {
        if(v.empty())
          return std::nullopt;
        const auto& elem = (voice_idx < v.size()) ? v[voice_idx] : v.back();
        return ossia::convert<float>(elem);
      }
      else if constexpr(std::is_same_v<T, ossia::vec2f>)
        return v[std::min<std::size_t>(voice_idx, 1)];
      else if constexpr(std::is_same_v<T, ossia::vec3f>)
        return v[std::min<std::size_t>(voice_idx, 2)];
      else if constexpr(std::is_same_v<T, ossia::vec4f>)
        return v[std::min<std::size_t>(voice_idx, 3)];
      else if constexpr(
          std::is_same_v<T, float> || std::is_same_v<T, int>
          || std::is_same_v<T, bool>)
        return float(v);
      else
        return std::nullopt;
    },
        val.v);
  }

  void preProcessVoice(voice& v, std::size_t voice_idx)
  {
    const std::size_t audio_in_size = data.audio_in_ports.size();
    const std::size_t cv_size = data.cv_ports.size();
    const std::size_t midi_in_size = data.midi_in_ports.size();
    const std::size_t atom_in_size = data.atom_in_ports.size();
    const std::size_t control_in_size = data.control_in_ports.size();

    int first_midi_idx = (audio_in_size > 0 ? 1 : 0) + cv_size;
    for(std::size_t i = 0; i < v.midi_atom_ins.size(); i++)
    {
      auto& ossia_port
          = this->m_inlets[i + first_midi_idx]->template cast<ossia::midi_port>();
      auto& lv2_port = v.midi_atom_ins[i];
      Iterator it{lv2_port.buf};

      for(const Message& msg : v.message_for_midi_atom_ins[i])
      {
        auto* atom = (LV2_Atom*)msg.body.data();
        auto* atom_data = (const uint8_t*)LV2_ATOM_BODY(atom);
        it.write(0, 0, atom->type, atom->size, atom_data);
      }

      auto& conv = v.midi_2to1[i];
      for(const libremidi::ump& msg : ossia_port.messages)
      {
        conv.convert(
            msg.data, msg.size(), msg.timestamp,
            [&](unsigned char* midi1, int bytes, int64_t) {
          if(bytes > 0)
            it.write(msg.timestamp, 0, data.host.midi_event_id, bytes, midi1);
          return stdx::error{};
        });
      }

      if(!v.atom_timePosition_midi.empty())
      {
        const LV2_Atom* atom = (const LV2_Atom*)timePositionBuffer.get();
        it.write(0, 0, atom->type, atom->size, (const uint8_t*)LV2_ATOM_BODY(atom));
      }
    }

    for(auto& [_, buf] : v.atom_timePosition_owned)
    {
      Iterator it{buf->buf};
      const LV2_Atom* atom = (const LV2_Atom*)timePositionBuffer.get();
      it.write(0, 0, atom->type, atom->size, (const uint8_t*)LV2_ATOM_BODY(atom));
    }

    const auto control_start
        = (audio_in_size > 0 ? 1 : 0) + midi_in_size + atom_in_size + cv_size;
    for(std::size_t i = 0; i < control_in_size; i++)
    {
      auto& in = m_inlets[control_start + i]
                     ->template cast<ossia::value_port>()
                     .get_data();
      if(in.empty())
        continue;
      if(auto new_val = voice_control_value(in.back().value, voice_idx))
        v.fInControls[i] = *new_val;
    }

    (void)atom_in_size;
  }

  void postProcessVoice(voice& v) noexcept
  {
    if(v.worker && v.worker->work_response)
    {
      std::vector<char> vec;
      while(v.worker_datas.try_dequeue(vec))
        v.worker->work_response(v.instance->lv2_handle, vec.size(), vec.data());
    }
    if(v.worker && v.worker->end_run)
      v.worker->end_run(v.instance->lv2_handle);

    for(auto& port : v.midi_atom_ins)
      port.buf->reset(true);
    for(auto& port : v.atom_ins)
      port.buf->reset(true);
    for(auto& [_, buf] : v.atom_timePosition_owned)
      buf->buf->reset(true);
    for(auto& port : v.midi_atom_outs)
      port.buf->reset(false);
    for(auto& port : v.atom_outs)
      port.buf->reset(false);
    for(auto& mqueue : v.message_for_midi_atom_ins)
      mqueue.clear();
  }

  // per_channel: inputs broadcast -> voice 0 outputs are canonical
  void surfaceVoiceZeroOutputs(int64_t offset)
  {
    auto& v0 = *voices[0];
    const auto audio_out_size = data.audio_out_ports.size();
    const auto midi_out_size = data.midi_out_ports.size();
    const auto control_out_size = data.control_out_ports.size();

    int first_midi_idx = (audio_out_size > 0 ? 1 : 0);
    for(std::size_t i = 0; i < v0.midi_atom_outs.size(); i++)
    {
      auto& ossia_port
          = this->m_outlets[i + first_midi_idx]->template cast<ossia::midi_port>();
      auto& lv2_port = v0.midi_atom_outs[i];
      auto& conv = v0.midi_1to2[i];

      LV2_ATOM_SEQUENCE_FOREACH(&lv2_port.buf->atoms, ev)
      {
        if(ev->body.type == data.host.midi_event_id)
        {
          auto bytes = (uint8_t*)LV2_ATOM_BODY_CONST(&ev->body);
          conv.convert(
              bytes, ev->body.size, ev->time.frames,
              [&](const uint32_t* ump, int count, int64_t ts) {
            libremidi::ump u;
            std::copy_n(ump, std::min(count, 4), u.data);
            u.timestamp = ts;
            ossia_port.messages.push_back(u);
            return stdx::error{};
          });
        }
        else
        {
          // FIXME forward non-MIDI atom outputs (patch:Set etc.) to a value outlet
          qDebug() << "Unhandled LV2 event type: " << ev->body.type;
        }
      }
    }

    const auto control_start = (audio_out_size > 0 ? 1 : 0) + midi_out_size;
    for(std::size_t i = 0; i < control_out_size; i++)
    {
      auto& out
          = m_outlets[control_start + i]->template cast<ossia::value_port>();
      out.write_value(v0.fOutControls[i], offset);
    }
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if(tk.date <= tk.prev_date)
      return;

    data.host.current = &data.effect;
    if(!data.time_Position_ports.empty())
      updateTime(tk, st);

    on_start();
    for(std::size_t c = 0; c < voices.size(); ++c)
      preProcessVoice(*voices[c], c);

    const auto [tick_start, samples] = st.timings(tk);
    const bool samples_valid
        = samples > 0 && std::size_t(samples) <= max_block_size;

    if(samples_valid)
    {
      switch(routing)
      {
        case voice_routing::single:
          runSingle(samples);
          break;
        case voice_routing::per_channel:
          runPerChannel(samples);
          break;
      }
    }

    surfaceVoiceZeroOutputs(tk.physical_start(st.modelToSamples()));
    on_finished();

    for(auto& v : voices)
      postProcessVoice(*v);
  }

  struct worker_routing_scope
  {
    LV2::HostContext& host;
    worker_routing_scope(LV2::HostContext& h, voice& v) noexcept
        : host{h}
    {
      host.current_worker_datas = &v.worker_datas;
    }
    ~worker_routing_scope() noexcept { host.current_worker_datas = nullptr; }
  };

  void runSingle(int samples) noexcept
  {
    auto& v = *voices[0];
    const auto audio_ins = data.audio_in_ports.size();
    const auto audio_outs = data.audio_out_ports.size();

    if(audio_ins > 0)
    {
      const auto& audio_in = m_inlets[0]->template cast<ossia::audio_port>();
      const auto in_channels = audio_in.channels();
      for(std::size_t i = 0; i < audio_ins; i++)
      {
        auto& scratch = v.audio_in_scratch[i];
        std::fill_n(scratch.data(), samples, 0.0f);
        if(in_channels > i)
        {
          const auto& ch = audio_in.channel(i);
          const auto n = std::min<std::size_t>(samples, ch.size());
          for(std::size_t j = 0; j < n; j++)
            scratch[j] = float(ch[j]);
        }
        lilv_instance_connect_port(
            v.instance, data.audio_in_ports[i], scratch.data());
      }
    }

    for(std::size_t i = 0; i < audio_outs; i++)
      lilv_instance_connect_port(
          v.instance, data.audio_out_ports[i], v.audio_out_scratch[i].data());

    {
      worker_routing_scope ws{data.host, v};
      lilv_instance_run(v.instance, samples);
    }

    if(audio_outs > 0)
    {
      auto& out = static_cast<ossia::audio_outlet*>(m_outlets[0])->data;
      out.set_channels(audio_outs);
      for(std::size_t i = 0; i < audio_outs; i++)
      {
        auto& ch = out.channel(i);
        ch.assign(
            v.audio_out_scratch[i].begin(),
            v.audio_out_scratch[i].begin() + samples);
      }
    }
  }

  // voice c <-> channel c; MIDI/controls broadcast in preProcessVoice
  void runPerChannel(int samples) noexcept
  {
    if(voices.empty())
      return;

    const auto* audio_in_inlet
        = m_inlets.empty() ? nullptr : m_inlets[0]->template target<ossia::audio_port>();
    const auto in_channels = audio_in_inlet ? audio_in_inlet->channels() : 0;

    // Pure synths drive all voices; otherwise follow input channel count
    const auto requested
        = (audio_in_inlet ? std::max<std::size_t>(in_channels, 1) : voices.size());
    request_voice_count(requested);

    const auto active = std::min<std::size_t>(voices.size(), requested);

    auto& out = static_cast<ossia::audio_outlet*>(m_outlets[0])->data;
    out.set_channels(active);

    for(std::size_t c = 0; c < active; ++c)
    {
      auto& v = *voices[c];

      if(audio_in_inlet)
      {
        auto& in_scratch = v.audio_in_scratch[0];
        std::fill_n(in_scratch.data(), samples, 0.0f);
        if(in_channels > c)
        {
          const auto& src = audio_in_inlet->channel(c);
          const auto n = std::min<std::size_t>(samples, src.size());
          for(std::size_t j = 0; j < n; j++)
            in_scratch[j] = float(src[j]);
        }
        lilv_instance_connect_port(
            v.instance, data.audio_in_ports[0], in_scratch.data());
      }

      auto& out_scratch = v.audio_out_scratch[0];
      lilv_instance_connect_port(
          v.instance, data.audio_out_ports[0], out_scratch.data());

      {
        worker_routing_scope ws{data.host, v};
        lilv_instance_run(v.instance, samples);
      }

      auto& dst = out.channel(c);
      dst.assign(out_scratch.begin(), out_scratch.begin() + samples);
    }
  }
};

} // namespace LV2
