#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>

namespace Media
{
namespace VST
{
class VSTAudioEffect : public ossia::graph_node
{
  private:
    AEffect& fx;

    void dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      fx.dispatcher(&fx, opcode, index, value, ptr, opt);
    }
  public:
    VSTAudioEffect(AEffect& dat, int sampleRate):
      fx{dat}
    {
      if(fx.flags & effFlagsIsSynth)
        m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
      else
        m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());

      for(std::size_t i = 0; i < fx.numParams; i++)
        m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());

      dispatch(effSetSampleRate, 0, 0, 0, sampleRate);
      dispatch(effSetBlockSize, 0, 64); // Generalize what's in pd
      dispatch(effSetProcessPrecision, 0, kVstProcessPrecision32);
      dispatch(effMainsChanged, 0, 1);
      dispatch(effStartProcess);
    }

    ~VSTAudioEffect()
    {
      dispatch(effStopProcess);
      dispatch(effMainsChanged, 0, 0);
    }

    void dispatchMidi()
    {
      // copy midi data
      auto& ip = m_inlets[0]->data.target<ossia::midi_port>()->messages;
      const auto n_mess = ip.size();
      VstEvents* events = (VstEvents*)alloca(sizeof(VstEvents) + sizeof(void*) * n_mess); // -2 since two are already available ?
      events->numEvents = n_mess;

      chobo::small_vector<VstMidiEvent, 16> vec;
      vec.resize(n_mess);
      std::size_t i = 0;
      for(mm::MidiMessage& mess : ip)
      {
        VstMidiEvent& e = vec[i];
        std::memset(&e, sizeof(VstMidiEvent), 0);

        e.type = kVstMidiType;
        e.byteSize = sizeof(VstMidiEvent);
        e.deltaFrames = mess.timestamp;
        e.midiData[0] = mess.data[0];
        e.midiData[1] = mess.data[1];
        e.midiData[2] = mess.data[2];
        e.midiData[3] = mess.data[3];

        events->events[i] = reinterpret_cast<VstEvent*>(&e);
        i++;
      }
      dispatch(effProcessEvents, 0, 0, events, 0.f);
    }

    auto& prepareInput()
    {
      auto& ip = m_inlets[0]->data.target<ossia::audio_port>()->samples;
      if(ip.size() < 2)
        ip.resize(2);
      if(ip[0].size() < 64)
      {
        ip[0].resize(64);
      }
      if(ip[1].size() < 64)
      {
        ip[1].resize(64);
      }
      return ip;
    }
    auto& prepareOutput()
    {
      auto& op = m_outlets[0]->data.target<ossia::audio_port>()->samples;
      op.resize(2);
      for(auto& chan : op)
        chan.resize(64);
      return op;
    }
    void run(ossia::token_request tk, ossia::execution_state&) override
    {
      if(tk.date > m_prev_date)
      {
        const std::size_t samples = tk.date - m_prev_date;

        if(fx.flags & effFlagsCanDoubleReplacing)
        {
          if(fx.flags & effFlagsIsSynth)
          {
            dispatchMidi();

            auto& op = prepareOutput();
            double* output[2] = { op[0].data(), op[1].data() };

            fx.processDoubleReplacing(&fx, nullptr, output, samples);
          }
          else
          {
            // copy audio data
            auto& ip = prepareInput();
            double* input[2] = { ip[0].data(), ip[1].data() };

            auto& op = prepareOutput();
            double* output[2] = { op[0].data(), op[1].data() };

            fx.processDoubleReplacing(&fx, input, output, samples);
          }
        }
        else
        {
          if(fx.flags & effFlagsIsSynth)
          {
            dispatchMidi();

            auto& op = prepareOutput();

            std::vector<std::vector<float>> v(2);
            for(auto& vec : v)
              vec.resize(samples);
            float* output[2] = { v[0].data(), v[1].data() };

            fx.processReplacing(&fx, nullptr, output, samples);

            op.clear();
            op.emplace_back(v[0].begin(), v[0].end());
            op.emplace_back(v[1].begin(), v[1].end());
          }
          else
          {
            // copy audio data

            auto& ip = m_inlets[0]->data.target<ossia::audio_port>()->samples;
            auto& op = m_outlets[0]->data.target<ossia::audio_port>()->samples;
            if(ip.size() < 2)
              ip.resize(2);

            std::vector<std::vector<float>> in_v;
            in_v.emplace_back(ip[0].begin(), ip[0].end());
            in_v.emplace_back(ip[1].begin(), ip[1].end());
            in_v[0].resize(samples);
            in_v[1].resize(samples);
            float* input[2] = { in_v[0].data(), in_v[1].data() };

            std::vector<std::vector<float>> out_v(2);
            for(auto& vec : out_v)
              vec.resize(samples);
            float* output[2] = { out_v[0].data(), out_v[1].data() };

            fx.processReplacing(&fx, input, output, samples);

            op.clear();
            op.emplace_back(out_v[0].begin(), out_v[0].end());
            op.emplace_back(out_v[1].begin(), out_v[1].end());
          }
        }
      }
    }
};


}
}
