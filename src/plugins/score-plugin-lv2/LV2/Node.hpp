#pragma once
#include <LV2/Context.hpp>
#include <LV2/lv2_atom_helpers.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace LV2
{
template <typename OnExecStart, typename OnExecFinished>
struct lv2_node final : public ossia::graph_node
{
  LV2Data data;
  ossia::float_vector fInControls, fOutControls, fParamMin, fParamMax,
      fParamInit, fOtherControls;
  std::vector<ossia::float_vector> fCVs;
  std::vector<AtomBuffer> m_atom_ins, m_atom_outs;
  std::vector<ossia::small_vector<Message, 2>> m_message_for_atom_ins;

  LilvInstance* fInstance{};
  std::unique_ptr<uint8_t> timePositionBuffer{};
  struct MatchedPort {
    int port;
    AtomBuffer* buffer{};
  };
  std::vector<MatchedPort> m_atom_timePosition_midi;
  std::vector<MatchedPort> m_atom_timePosition_owned;

  OnExecStart on_start;
  OnExecFinished on_finished;
  lv2_node(LV2Data dat, int sampleRate, OnExecStart os, OnExecFinished of)
      : data{dat}
      , on_start{os}
      , on_finished{of}
  {
    data.host.global->sampleRate = sampleRate;
    const std::size_t audio_in_size = data.audio_in_ports.size();
    const std::size_t audio_out_size = data.audio_out_ports.size();
    const std::size_t control_in_size = data.control_in_ports.size();
    const std::size_t control_out_size = data.control_out_ports.size();
    const std::size_t midi_in_size = data.midi_in_ports.size();
    const std::size_t midi_out_size = data.midi_out_ports.size();
    const std::size_t time_in_size = data.time_Position_ports.size();
    const std::size_t cv_size = data.cv_ports.size();
    const std::size_t other_size = data.control_other_ports.size();
    const std::size_t num_ports = data.effect.plugin.get_num_ports();

    if (audio_in_size > 0)
    {
      m_inlets.push_back(new ossia::audio_inlet);
    }
    if (audio_out_size > 0)
    {
      m_outlets.push_back(new ossia::audio_outlet);
    }

    for (std::size_t i = 0; i < cv_size; i++)
    {
      m_inlets.push_back(new ossia::audio_inlet);
    }

    for (std::size_t i = 0; i < midi_in_size; i++)
    {
      m_inlets.push_back(new ossia::midi_inlet);
    }
    for (std::size_t i = 0; i < midi_out_size; i++)
    {
      m_outlets.push_back(new ossia::midi_outlet);
    }

    for (std::size_t i = 0; i < control_in_size; i++)
    {
      m_inlets.push_back(new ossia::value_inlet);
    }
    for (std::size_t i = 0; i < control_out_size; i++)
    {
      m_outlets.push_back(new ossia::value_outlet);
    }

    fInControls.resize(control_in_size);
    fOutControls.resize(control_out_size);
    fOtherControls.resize(other_size);
    fCVs.resize(cv_size);
    for (std::size_t i = 0; i < cv_size; i++)
    {
      fCVs[i].resize(4096);
    }

    fParamMin.resize(num_ports);
    fParamMax.resize(num_ports);
    fParamInit.resize(num_ports);

    data.effect.plugin.get_port_ranges_float(
        fParamMin.data(), fParamMax.data(), fParamInit.data());

    fInstance = data.effect.instance;
    data.effect.instance = fInstance;

    if (!fInstance)
      throw std::runtime_error("Error while creating a LV2 plug-in");

    // MIDI
    m_atom_ins.reserve(midi_in_size);
    m_message_for_atom_ins.resize(midi_in_size);
    for (std::size_t i = 0; i < midi_in_size; i++)
    {
      m_atom_ins.emplace_back(
          2048, data.host.atom_chunk_id, data.host.midi_event_id, true);
    }

    m_atom_outs.reserve(midi_out_size);
    for (std::size_t i = 0; i < midi_out_size; i++)
    {
      m_atom_outs.emplace_back(
          2048, data.host.atom_chunk_id, data.host.midi_event_id, false);
    }

    // Timing
    // Note: some plug-ins have the timing port shared with the midi port, others have it separate
    for (std::size_t i = 0; i < time_in_size; i++)
    {
      auto port_index = data.time_Position_ports[i];

      bool is_midi = false;
      for(std::size_t midi_port_k = 0; midi_port_k < data.midi_in_ports.size(); midi_port_k++) {
        int midi_port_index = data.midi_in_ports[midi_port_k];
        if(midi_port_index == port_index) {
          m_atom_timePosition_midi.push_back(MatchedPort{port_index, &m_atom_ins[midi_port_k]});
          is_midi = true;
          break;
        }
      }

      if(!is_midi) {
        // Allocate a new port
        auto abuf = new AtomBuffer(256, data.host.atom_chunk_id, data.host.time_Position_id, true);
        m_atom_timePosition_owned.push_back(MatchedPort{port_index, abuf});
      }
    }

    // Worker
    if (lilv_plugin_has_feature(data.effect.plugin.me, data.host.work_schedule)
        && lilv_plugin_has_extension_data(
            data.effect.plugin.me, data.host.work_interface))
    {
      data.effect.worker = static_cast<const LV2_Worker_Interface*>(
          lilv_instance_get_extension_data(fInstance, LV2_WORKER__interface));
    }

    for (std::size_t i = 0; i < control_in_size; i++)
    {
      auto port_i = data.control_in_ports[i];
      fInControls[i] = fParamInit[port_i];
    }

    if(!m_atom_timePosition_midi.empty() || !m_atom_timePosition_owned.empty())
    {
      // inspired from QTractor code, (c) RNCBC
      timePositionBuffer.reset(new uint8_t[256]);
    }

    lilv_instance_activate(fInstance);
  }

  void connect_all_ports()
  {
    const std::size_t control_in_size = data.control_in_ports.size();
    const std::size_t control_out_size = data.control_out_ports.size();
    const std::size_t midi_in_size = data.midi_in_ports.size();
    const std::size_t midi_out_size = data.midi_out_ports.size();
    const std::size_t cv_size = data.cv_ports.size();
    const std::size_t other_size = data.control_other_ports.size();

    for (std::size_t i = 0; i < control_in_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.control_in_ports[i], &fInControls[i]);
    }

    for (std::size_t i = 0; i < control_out_size; i++)
    {
      lilv_instance_connect_port(
          fInstance, data.control_out_ports[i], &fOutControls[i]);
    }

    for (std::size_t i = 0; i < cv_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.cv_ports[i], fCVs[i].data());
    }

    for (std::size_t i = 0; i < other_size; i++)
    {
      lilv_instance_connect_port(
          fInstance, data.control_other_ports[i], &fOtherControls[i]);
    }

    for (std::size_t i = 0; i < midi_in_size; i++)
    {
      lilv_instance_connect_port(
          fInstance, data.midi_in_ports[i], &m_atom_ins[i].buf->atoms);
    }

    for (std::size_t i = 0; i < midi_out_size; i++)
    {
      auto& out_p = data.midi_out_ports[i];
      auto atoms = &m_atom_outs[i].buf->atoms;
      lilv_instance_connect_port(fInstance, out_p, atoms);
    }

    for(auto& [index, port] : m_atom_timePosition_owned)
    {
      lilv_instance_connect_port(
          fInstance, index, &port->buf->atoms);
    }

  }

  void all_notes_off() noexcept override
  {
    // TODO
  }

  void preProcess()
  {
    const std::size_t audio_in_size = data.audio_in_ports.size();
    const std::size_t cv_size = data.cv_ports.size();
    const std::size_t midi_in_size = data.midi_in_ports.size();
    const std::size_t control_in_size = data.control_in_ports.size();

    // Callback from UI
    on_start();

    // Copy midi
    int first_midi_idx = (audio_in_size > 0 ? 1 : 0) + cv_size;
    for (std::size_t i = 0; i < m_atom_ins.size(); i++)
    {
      ossia::midi_port& ossia_port = this->m_inlets[i + first_midi_idx]
                                         ->template cast<ossia::midi_port>();
      auto& lv2_port = m_atom_ins[i];
      Iterator it{lv2_port.buf};

      // Message from the UI
      for (const Message& msg : this->m_message_for_atom_ins[i]) {
        auto atom = (LV2_Atom*) msg.body.data();
        auto atom_data = (const uint8_t*) LV2_ATOM_BODY(atom);
        it.write(0, 0, atom->type, atom->size, atom_data);
      }

      // MIDI input
      for (const libremidi::message& msg : ossia_port.messages)
      {
        it.write(
            msg.timestamp,
            0,
            data.host.midi_event_id,
            msg.bytes.size(),
            msg.bytes.data());
      }

      // Copy timing for MIDI ports
      if(this->m_atom_timePosition_midi.size() != 0)
      {
        const LV2_Atom *atom = (const LV2_Atom *) timePositionBuffer.get();

        // Time position
        it.write(
            0,
            0,
            atom->type,
            atom->size,
            (const uint8_t *) LV2_ATOM_BODY(atom));
      }
    }

    // Copy timing for timing-only ports
    for(auto& [port, atoms] : m_atom_timePosition_owned)
    {
      auto& lv2_port = atoms;
      Iterator it{lv2_port->buf};
      {
        const LV2_Atom *atom = (const LV2_Atom *) timePositionBuffer.get();

        // Time position
        it.write(
            0,
            0,
            atom->type,
            atom->size,
            (const uint8_t *) LV2_ATOM_BODY(atom));
      }
    }


    // Copy controls
    auto control_start = (audio_in_size > 0 ? 1 : 0) + midi_in_size + cv_size;
    for (std::size_t i = control_start; i < control_in_size; i++)
    {
      auto& in = m_inlets[i]->template cast<ossia::value_port>().get_data();

      if (!in.empty())
      {
        if (auto f = in.back().value.template target<float>())
        {
          fInControls[i - control_start] = *f;
        }
      }
    }
  }

  void updateTime(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    LV2::HostContext& host = this->data.host;
    auto& forge = host.forge;
    uint8_t *buffer = timePositionBuffer.get();
    lv2_atom_forge_set_buffer(&forge, buffer, 256);
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&forge, &frame, 0, host.time_Position_id);

    lv2_atom_forge_key(&forge, host.time_frame_id);
    // FIXME see note in vst_node_base::setupTimeInfo
    lv2_atom_forge_long(&forge, tk.start_date_to_physical(st.modelToSamples()));

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
    lv2_atom_forge_float(&forge, 4 * double(tk.signature.upper) / double(tk.signature.lower));

    lv2_atom_forge_key(&forge, host.time_beatsPerMinute_id);
    lv2_atom_forge_float(&forge, tk.tempo);

    lv2_atom_forge_pop(&forge, &frame);
  }

  void postProcess(int64_t offset)
  {
    if (data.effect.worker && data.effect.worker->work_response)
    {
      std::vector<char> vec;
      while(data.effect.worker_datas.try_dequeue(vec)) {
        data.effect.worker->work_response(
            data.effect.instance->lv2_handle,
            vec.size(),
            vec.data());
      }
    }

    if (data.effect.worker && data.effect.worker->end_run)
    {
      data.effect.worker->end_run(data.effect.instance->lv2_handle);
    }

    const std::size_t audio_out_size = data.audio_out_ports.size();
    const std::size_t midi_out_size = data.midi_out_ports.size();
    const std::size_t control_out_size = data.control_out_ports.size();

    // Copy midi
    // FIXME the line below definitely does not look right
    int first_midi_idx = (audio_out_size > 0 ? 1 : 0);
    for (std::size_t i = 0; i < m_atom_outs.size(); i++)
    {
      ossia::midi_port& ossia_port = this->m_outlets[i + first_midi_idx]
                                         ->template cast<ossia::midi_port>();
      AtomBuffer& lv2_port = m_atom_outs[i];

      const LV2::HostContext& host = this->data.host;
      LV2_ATOM_SEQUENCE_FOREACH(&lv2_port.buf->atoms, ev)
      {
        if(ev->body.type == host.midi_event_id)
        {
          libremidi::message msg;
          msg.timestamp = ev->time.frames;
          msg.bytes.resize(ev->body.size);

          auto bytes = (uint8_t*) LV2_ATOM_BODY(&ev->body);
          for (std::size_t i = 0; i < ev->body.size; i++)
          {
            msg.bytes[i] = bytes[i];
          }
          ossia_port.messages.push_back(std::move(msg));
        }
        else
        {
        }
      }
    }

    // Copy controls
    auto control_start = (audio_out_size > 0 ? 1 : 0) + midi_out_size;
    for (std::size_t i = control_start; i < control_out_size; i++)
    {
      auto& out = m_outlets[i]->template cast<ossia::value_port>();

      out.write_value(fOutControls[i - control_start], offset);
    }

    // Callback to UI
    on_finished();

    for (AtomBuffer& port : m_atom_ins)
    {
      port.buf->reset(true);
    }
    for (auto& [port, atoms] : m_atom_timePosition_owned)
    {
      atoms->buf->reset(true);
    }
    for (AtomBuffer& port : m_atom_outs)
    {
      port.buf->reset(false);
    }

    for(auto& mqueue : m_message_for_atom_ins)
      mqueue.clear();
  }

  ~lv2_node() override {
    lilv_instance_deactivate(fInstance);
    if(!m_atom_timePosition_owned.empty())
      for(auto [port, atoms] : m_atom_timePosition_owned)
        delete atoms;
  }

  void
  run(const ossia::token_request& tk,
      ossia::exec_state_facade st) noexcept override
  {
    if (tk.date > tk.prev_date)
    {
      data.host.current = &data.effect;
      if(!data.time_Position_ports.empty())
        updateTime(tk, st);

      preProcess();

      const auto [tick_start, samples] = st.timings(tk);
      const auto audio_ins = data.audio_in_ports.size();
      const auto audio_outs = data.audio_out_ports.size();
      ossia::small_vector<ossia::float_vector, 2> in_vec;
      in_vec.resize(audio_ins);
      ossia::small_vector<ossia::float_vector, 2> out_vec;
      out_vec.resize(audio_outs);

      connect_all_ports();
      if (audio_ins > 0)
      {
        const auto& audio_in = m_inlets[0]->template cast<ossia::audio_port>();
        for (std::size_t i = 0; i < audio_ins; i++)
        {
          in_vec[i].resize(samples);
          if (audio_in.samples.size() > i)
          {
            for (std::size_t j = 0;
                 j < std::min(std::size_t(samples), audio_in.samples[i].size());
                 j++)
            {
              in_vec[i][j] = (float)audio_in.samples[i][j];
            }
          }
          lilv_instance_connect_port(
              fInstance, data.audio_in_ports[i], in_vec[i].data());
        }
      }

      if (audio_outs > 0)
      {
        for (std::size_t i = 0; i < audio_outs; i++)
        {
          out_vec[i].resize(samples);
          lilv_instance_connect_port(
              fInstance, data.audio_out_ports[i], out_vec[i].data());
        }
      }

      lilv_instance_run(fInstance, samples);

      if (audio_outs > 0)
      {
        auto& audio_out
            = static_cast<ossia::audio_outlet*>(m_outlets[0])->data;
        audio_out.set_channels(audio_outs);
        for (std::size_t i = 0; i < audio_outs; i++)
        {
          audio_out.samples[i].clear();
          audio_out.samples[i].reserve(samples);
          for (int64_t j = 0; j < samples; j++)
          {
            audio_out.samples[i].push_back((double)out_vec[i][j]);
          }
        }
      }

      postProcess(tk.physical_start(st.modelToSamples()));
    }
  }
};
}
