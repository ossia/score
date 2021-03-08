#pragma once
#include <LV2/Context.hpp>
#include <LV2/lv2_atom_helpers.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace LV2
{
template <typename OnExecFinished>
struct lv2_node final : public ossia::graph_node
{
  LV2Data data;
  ossia::float_vector fInControls, fOutControls, fParamMin, fParamMax, fParamInit, fOtherControls;
  std::vector<ossia::float_vector> fCVs;
  std::vector<AtomBuffer> fMidiIns, fMidiOuts;

  LilvInstance* fInstance{};

  OnExecFinished onFinished;
  lv2_node(LV2Data dat, int sampleRate, OnExecFinished of) : data{dat}, onFinished{of}
  {
    data.host.global->sampleRate = sampleRate;
    const std::size_t audio_in_size = data.audio_in_ports.size();
    const std::size_t audio_out_size = data.audio_out_ports.size();
    const std::size_t control_in_size = data.control_in_ports.size();
    const std::size_t control_out_size = data.control_out_ports.size();
    const std::size_t midi_in_size = data.midi_in_ports.size();
    const std::size_t midi_out_size = data.midi_out_ports.size();
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
    fMidiIns.reserve(midi_in_size);
    for (std::size_t i = 0; i < midi_in_size; i++)
    {
      fMidiIns.emplace_back(2048, data.host.atom_chunk_id, data.host.midi_event_id, true);
    }

    fMidiOuts.reserve(midi_out_size);
    for (std::size_t i = 0; i < midi_out_size; i++)
    {
      fMidiOuts.emplace_back(2048, data.host.atom_chunk_id, data.host.midi_event_id, false);
    }

    // Worker
    if (lilv_plugin_has_feature(data.effect.plugin.me, data.host.work_schedule)
        && lilv_plugin_has_extension_data(data.effect.plugin.me, data.host.work_interface))
    {
      data.effect.worker = static_cast<const LV2_Worker_Interface*>(
          lilv_instance_get_extension_data(fInstance, LV2_WORKER__interface));
    }

    for (std::size_t i = 0; i < control_in_size; i++)
    {
      auto port_i = data.control_in_ports[i];
      fInControls[i] = fParamInit[port_i];
      lilv_instance_connect_port(fInstance, port_i, &fInControls[i]);
    }

    for (std::size_t i = 0; i < control_out_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.control_out_ports[i], &fOutControls[i]);
    }

    for (std::size_t i = 0; i < cv_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.cv_ports[i], fCVs[i].data());
    }

    for (std::size_t i = 0; i < other_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.control_other_ports[i], &fOtherControls[i]);
    }

    for (std::size_t i = 0; i < midi_in_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.midi_in_ports[i], &fMidiIns[i].buf->atoms);
    }

    for (std::size_t i = 0; i < midi_out_size; i++)
    {
      lilv_instance_connect_port(fInstance, data.midi_out_ports[i], &fMidiOuts[i].buf->atoms);
    }

    lilv_instance_activate(fInstance);
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

    // Copy midi
    int first_midi_idx = (audio_in_size > 0 ? 1 : 0) + cv_size;
    for (std::size_t i = 0; i < fMidiIns.size(); i++)
    {
      ossia::midi_port& ossia_port
          = this->m_inlets[i + first_midi_idx]->template cast<ossia::midi_port>();
      auto& lv2_port = fMidiIns[i];
      Iterator it{lv2_port.buf};

      for (const libremidi::message& msg : ossia_port.messages)
      {
        it.write(msg.timestamp, 0, data.host.midi_event_id, msg.bytes.size(), msg.bytes.data());
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

  void postProcess(int64_t offset)
  {
    if (data.effect.worker && data.effect.worker_response && data.effect.worker->work_response)
    {
      data.effect.worker->work_response(
          data.effect.instance->lv2_handle,
          data.effect.worker_data.size(),
          data.effect.worker_data.data());
      data.effect.worker_response = false;
    }

    if (data.effect.worker && data.effect.worker->end_run)
    {
      data.effect.worker->end_run(data.effect.instance->lv2_handle);
    }

    const std::size_t audio_out_size = data.audio_out_ports.size();
    const std::size_t midi_out_size = data.midi_out_ports.size();
    const std::size_t control_out_size = data.control_out_ports.size();

    // Copy midi
    int first_midi_idx = (audio_out_size > 0 ? 1 : 0);
    for (std::size_t i = 0; i < fMidiOuts.size(); i++)
    {
      ossia::midi_port& ossia_port
          = this->m_outlets[i + first_midi_idx]->template cast<ossia::midi_port>();
      auto& lv2_port = fMidiOuts[i];
      Iterator it{lv2_port.buf};

      while (it.is_valid())
      {
        uint8_t* bytes;
        auto ev = it.get(&bytes);

        if (ev->body.type == data.host.midi_event_id)
        {
          libremidi::message msg;
          msg.timestamp = ev->time.frames;
          msg.bytes.resize(ev->body.size);
          for (std::size_t i = 0; i < ev->body.size; i++)
            msg.bytes[i] = bytes[i];
          ossia_port.messages.push_back(std::move(msg));
        }
        it.increment();
      }
    }

    // Copy controls
    auto control_start = (audio_out_size > 0 ? 1 : 0) + midi_out_size;
    for (std::size_t i = control_start; i < control_out_size; i++)
    {
      auto& out = m_outlets[i]->template cast<ossia::value_port>();

      out.write_value(fOutControls[i - control_start], offset);
    }

    onFinished();

    for (AtomBuffer& port : fMidiIns)
    {
      port.buf->reset(true);
    }
    for (AtomBuffer& port : fMidiOuts)
    {
      port.buf->reset(false);
    }
  }

  ~lv2_node() override { lilv_instance_deactivate(fInstance); }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if (tk.date > tk.prev_date)
    {
      data.host.current = &data.effect;
      preProcess();

      const std::size_t samples = tk.physical_write_duration(st.modelToSamples());
      const auto audio_ins = data.audio_in_ports.size();
      const auto audio_outs = data.audio_out_ports.size();
      ossia::small_vector<ossia::float_vector, 2> in_vec;
      in_vec.resize(audio_ins);
      ossia::small_vector<ossia::float_vector, 2> out_vec;
      out_vec.resize(audio_outs);

      if (audio_ins > 0)
      {
        const auto& audio_in = m_inlets[0]->template cast<ossia::audio_port>();
        for (std::size_t i = 0; i < audio_ins; i++)
        {
          in_vec[i].resize(samples);
          if (audio_in.samples.size() > i)
          {
            for (std::size_t j = 0; j < std::min(samples, audio_in.samples[i].size()); j++)
            {
              in_vec[i][j] = (float)audio_in.samples[i][j];
            }
          }
          lilv_instance_connect_port(fInstance, data.audio_in_ports[i], in_vec[i].data());
        }
      }

      if (audio_outs > 0)
      {
        for (std::size_t i = 0; i < audio_outs; i++)
        {
          out_vec[i].resize(samples);
          lilv_instance_connect_port(fInstance, data.audio_out_ports[i], out_vec[i].data());
        }
      }

      lilv_instance_run(fInstance, samples);

      if (audio_outs > 0)
      {
        auto& audio_out = static_cast<ossia::audio_outlet*>(m_outlets[0])->data;
        audio_out.samples.resize(audio_outs);
        for (std::size_t i = 0; i < audio_outs; i++)
        {
          audio_out.samples[i].clear();
          audio_out.samples[i].reserve(samples);
          for (std::size_t j = 0; j < samples; j++)
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
