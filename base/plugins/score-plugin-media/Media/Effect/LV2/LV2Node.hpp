#pragma once
#include <ossia/dataflow/fx_node.hpp>
#include <Media/Effect/LV2/LV2Context.hpp>
#include <Media/Effect/LV2/lv2_atom_helpers.hpp>
#include <unordered_map>

namespace Media
{
namespace LV2
{
class lv2_node final : public ossia::graph_node
{
  protected:
    LV2Data data;
    std::vector<float> fInControls, fOutControls, fParamMin, fParamMax, fParamInit, fOtherControls;
    std::unordered_map<std::string, int> fLabelsMap;
    std::vector<std::vector<float>> fCVs;
    std::vector<AtomBuffer> fMidiIns, fMidiOuts;

    LilvInstance* fInstance{};

  public:
    lv2_node(LV2Data dat, int sampleRate):
      data{dat}
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

      if(audio_in_size > 0)
      {
        m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
      }
      if(audio_out_size > 0)
      {
        m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
      }

      for(std::size_t i = 0; i < cv_size; i++)
      {
        m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
      }

      for(std::size_t i = 0; i < midi_in_size; i++)
      {
        m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
      }
      for(std::size_t i = 0; i < midi_out_size; i++)
      {
        m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
      }

      for(std::size_t i = 0; i < control_in_size; i++)
      {
        m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
      }
      for(std::size_t i = 0; i < control_out_size; i++)
      {
        m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
      }



      qDebug() << "in\t" << control_in_size << "\n"
               << "out\t" << control_out_size << "\n"
               << "min\t" << midi_in_size << "\n"
               << "mout\t" << midi_out_size << "\n"
               << "np\t" << num_ports;

      fInControls.resize(control_in_size);
      fOutControls.resize(control_out_size);
      fOtherControls.resize(other_size);
      fCVs.resize(cv_size);
      for(std::size_t i = 0; i < cv_size; i++)
      {
        fCVs[i].resize(4096);
      }

      fParamMin.resize(num_ports);
      fParamMax.resize(num_ports);
      fParamInit.resize(num_ports);

      data.effect.plugin.get_port_ranges_float(fParamMin.data(), fParamMax.data(), fParamInit.data());

      for(std::size_t i = 0; i < control_in_size; i++)
      {
        Lilv::Port p{data.effect.plugin.get_port_by_index(data.control_in_ports[i])};
        Lilv::Node n = p.get_name();
        fLabelsMap.emplace(n.as_string(), i);
      }

      fInstance = lilv_plugin_instantiate(
                    data.effect.plugin.me,
                    data.host.global->sampleRate,
                    data.host.features);
      data.effect.instance = fInstance;

      if(!fInstance)
        throw std::runtime_error("Error while creating a LV2 plug-in");

      data.effect.data.data_access = lilv_instance_get_descriptor(fInstance)->extension_data;

      // MIDI
      fMidiIns.reserve(midi_in_size);
      for(std::size_t i = 0; i < midi_in_size; i++)
      {
        fMidiIns.emplace_back(2048, data.host.atom_chunk_id, data.host.midi_event_id, true);
      }

      fMidiOuts.reserve(midi_out_size);
      for(std::size_t i = 0; i < midi_out_size; i++)
      {
        fMidiOuts.emplace_back(2048, data.host.atom_chunk_id, data.host.midi_event_id, false);
      }

      // Worker
      if(lilv_plugin_has_feature(data.effect.plugin.me, data.host.work_schedule)
         && lilv_plugin_has_extension_data(data.effect.plugin.me, data.host.work_interface))
      {
        data.effect.worker =
            static_cast<const LV2_Worker_Interface*>(
              lilv_instance_get_extension_data(fInstance, LV2_WORKER__interface));
      }

      for(std::size_t i = 0; i < control_in_size; i++)
      {
        auto port_i = data.control_in_ports[i];
        std::cerr << port_i << ": " << fParamMin[port_i] << " " << fParamMax[port_i] << std::endl;
        fInControls[i] = fParamInit[port_i];
        lilv_instance_connect_port(fInstance, port_i, &fInControls[i]);
      }

      for(std::size_t i = 0; i < control_out_size; i++)
      {
        lilv_instance_connect_port(fInstance, data.control_out_ports[i], &fOutControls[i]);
      }

      for(std::size_t i = 0; i < cv_size; i++)
      {
        lilv_instance_connect_port(fInstance, data.cv_ports[i], fCVs[i].data());
      }

      for(std::size_t i = 0; i < other_size; i++)
      {
        lilv_instance_connect_port(fInstance, data.control_other_ports[i], &fOtherControls[i]);
      }

      for(std::size_t i = 0; i < midi_in_size; i++)
      {
        lilv_instance_connect_port(fInstance, data.midi_in_ports[i], &fMidiIns[i].buf->atoms);
      }

      for(std::size_t i = 0; i < midi_out_size; i++)
      {
        lilv_instance_connect_port(fInstance, data.midi_out_ports[i], &fMidiOuts[i].buf->atoms);
      }

      lilv_instance_activate(fInstance);
    }

    void GetControlParamImpl(long param, char* label, float* min, float* max, float* init, std::vector<int>& v)
    {
      if(param >= 0 && param < v.size())
      {
        auto port_i = v[param];
        Lilv::Port p = data.effect.plugin.get_port_by_index(port_i);
        Lilv::Node n = p.get_name();
        strcpy(label, n.as_string());
        *min = fParamMin[port_i];
        *max = fParamMax[port_i];
        *init = fParamInit[port_i];
      }
    }

    std::string GetName()
    {
      return data.effect.plugin.get_name().as_string();
    }


    void SetControlValue(long param, float value)
    {
      if(param >= 0 && param < (int64_t)data.control_in_ports.size())
      {
        auto param_i = data.control_in_ports[param];
        fInControls[param] = ossia::clamp(value, fParamMin[param_i], fParamMax[param_i]);
      }
    }

    void SetControlValue(const char* label, float value)
    {
      auto it = fLabelsMap.find(label);
      if(it != fLabelsMap.end())
        SetControlValue(it->second, value);
    }

    float GetControlValue(long param)
    {
      if(param >= 0 && param < (int64_t)data.control_in_ports.size())
        return fInControls[param];
      return {};
    }
    float GetControlValue(const char* label)
    {
      auto it = fLabelsMap.find(label);
      if(it != fLabelsMap.end())
        return GetControlValue(it->second);
      return {};
    }

    float GetControlOutValue(long param)
    {
      if(param >= 0 && param < (int64_t)data.control_out_ports.size())
        return fOutControls[param];
      return {};
    }

    void all_notes_off() override
    {
      /*
          if(fMidiSource)
          {
            for(AtomBuffer& port : fMidiIns)
            {
              static float stopbuf_[1];
              static float* stopbuf[2] = { +stopbuf_, +stopbuf_ };
              Iterator it{port.buf};
              for(int i = 0; i < 127; i++)
              {
                auto off = mm::MakeNoteOff(fMidiSource->process().channel(), i, 0);
                it.write(0, 0, data.host.midi_event_id, 3, off.data.data());
              }

              Process(+stopbuf, +stopbuf, 0);
            }
          }*/
    }

    void preProcess()
    {/*
          if(fMidiSource)
          {
            for(AtomBuffer& port : fMidiIns)
            {
              Iterator it{port.buf};
              for(auto& note : fMidiSource->timedState.currentAudioStart)
              {
                auto on = mm::MakeNoteOn(fMidiSource->process().channel(), note.first.pitch(), note.first.velocity());
                it.write(note.second, 0, data.host.midi_event_id, 3, on.data.data());
              }

              for(auto& note : fMidiSource->timedState.currentAudioStop)
              {
                auto off = mm::MakeNoteOff(fMidiSource->process().channel(), note.first.pitch(), note.first.velocity());
                it.write(note.second, 0, data.host.midi_event_id, 3, off.data.data());
              }
            }
            fMidiSource->timedState.currentAudioStart.clear();
            fMidiSource->timedState.currentAudioStop.clear();
          }*/
    }

    void postProcess()
    {
      if(data.effect.worker && data.effect.worker_response && data.effect.worker->work_response )
      {
        data.effect.worker->work_response(
              data.effect.instance->lv2_handle,
              data.effect.worker_data.size(),
              data.effect.worker_data.data());
        data.effect.worker_response = false;
      }

      if(data.effect.on_outControlsChanged && !fOutControls.empty())
      {
        data.effect.on_outControlsChanged();
      }

      if(data.effect.worker && data.effect.worker->end_run)
      {
        data.effect.worker->end_run(data.effect.instance->lv2_handle);
      }


      for(AtomBuffer& port : fMidiIns)
      {
        port.buf->reset(true);
      }
      for(AtomBuffer& port : fMidiOuts)
      {
        port.buf->reset(false);
      }
    }

    ~lv2_node()
    {
      lilv_instance_deactivate(fInstance);
      //lilv_instance_free(fInstance);
    }

    void run(ossia::token_request tk, ossia::execution_state&) override
    {
      if(tk.date > m_prev_date)
      {
        data.host.current = &data.effect;
        preProcess();

        const std::size_t samples = tk.date - m_prev_date;
        const auto audio_ins = data.audio_in_ports.size();
        const auto audio_outs = data.audio_out_ports.size();
        std::vector<std::vector<float>> in_vec;
        in_vec.resize(audio_ins);
        std::vector<std::vector<float>> out_vec;
        out_vec.resize(audio_outs);

        if(audio_ins > 0)
        {
          const auto& audio_in = *m_inlets[0]->data.target<ossia::audio_port>();
          for(std::size_t i = 0; i < audio_ins; i++)
          {
            in_vec[i].resize(samples);
            if(audio_in.samples.size() > i)
            {
              for(std::size_t j = 0; j < std::min(samples, audio_in.samples[i].size()); j++)
              {
                in_vec[i][j] = (float)audio_in.samples[i][j];
              }
            }
            lilv_instance_connect_port(fInstance, data.audio_in_ports[i], in_vec[i].data());
          }
        }

        if(audio_outs > 0)
        {
          for(std::size_t i = 0; i < audio_outs; i++)
          {
            out_vec[i].resize(samples);
            lilv_instance_connect_port(fInstance, data.audio_out_ports[i], out_vec[i].data());
          }
        }

        lilv_instance_run(fInstance, samples);

        if(audio_outs > 0)
        {
          auto& audio_out = *m_outlets[0]->data.target<ossia::audio_port>();
          audio_out.samples.resize(audio_outs);
          for(std::size_t i = 0; i < audio_outs; i++)
          {
            audio_out.samples[i].clear();
            audio_out.samples[i].reserve(samples);
            for(std::size_t j = 0; j < samples; j++)
            {
              audio_out.samples[i].push_back((double)out_vec[i][j]);
            }
          }
        }

        postProcess();
      }
    }
};

}
}
