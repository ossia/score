#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <ossia/dataflow/fx_node.hpp>

namespace Media
{
namespace VST
{
template<bool UseDouble, bool IsSynth>
class VSTNode final : public ossia::audio_fx_node
{
  private:
    AEffect& fx;

    void dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      fx.dispatcher(&fx, opcode, index, value, ptr, opt);
    }
  public:
    VSTNode(AEffect& dat, int sampleRate):
      fx{dat}
    {
      if constexpr(IsSynth)
        m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
      else
        m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());

      for(int i = 0; i < fx.numParams; i++)
        m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());

      dispatch(effSetSampleRate, 0, 0, 0, sampleRate);
      dispatch(effSetBlockSize, 0, 0); // Generalize what's in pd
      dispatch(effSetProcessPrecision, 0, kVstProcessPrecision32);
      dispatch(effMainsChanged, 0, 1);
      dispatch(effStartProcess);
    }

    ~VSTNode()
    {
      dispatch(effStopProcess);
      dispatch(effMainsChanged, 0, 0);
    }

    void all_notes_off() override
    {
      // copy midi data
      auto& ip = m_inlets[0]->data.template target<ossia::midi_port>()->messages;
      VstEvents* events = (VstEvents*)alloca(sizeof(VstEvents));
      events->numEvents = 1;

      VstMidiEvent e;
      std::memset(&e, sizeof(VstMidiEvent), 0);

      e.type = kVstMidiType;
      e.byteSize = sizeof(VstMidiEvent);
      // TODO do it for all channel
      e.midiData[0] = (char)(uint8_t)176;
      e.midiData[1] = (char)(uint8_t)123;
      e.midiData[2] = 0;
      e.midiData[3] = 0;

      events->events[0] = reinterpret_cast<VstEvent*>(&e);

      dispatch(effProcessEvents, 0, 0, events, 0.f);

      if constexpr(UseDouble)
      {
        double f;
        double* c[2]{&f, &f};
        fx.processDoubleReplacing(&fx, c, c, 1);
      }
      else
      {
        float f{};
        float* c[2]{&f, &f};
        fx.processReplacing(&fx, c, c, 1);
      }
    }

    void setControls()
    {
      for(std::size_t i = 1; i < m_inlets.size(); i++)
      {
        auto& vec = m_inlets[i]->data.template target<ossia::value_port>()->get_data();
        if(vec.empty())
        {
          continue;
        }
        fx.setParameter(&fx, i-1, ossia::convert<float>(last(vec)));
      }
    }

    // Note: the function that does the actual tick is passed as an argument
    // since some plug-ins only store pointers to VstEvents struct,
    // which would go out of scope if this function was just called like this.
    template<typename Fun>
    void dispatchMidi(Fun&& f)
    {
      // copy midi data
      auto& ip = m_inlets[0]->data.template target<ossia::midi_port>()->messages;
      const auto n_mess = ip.size();
      // -2 since two are already available ?
      VstEvents* events = (VstEvents*)alloca(sizeof(VstEvents) + sizeof(void*) * n_mess);
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

        for(std::size_t k = 0; k < std::min(mess.data.size(), (std::size_t)4); k++)
          e.midiData[k] = mess.data[k];

        events->events[i] = reinterpret_cast<VstEvent*>(&e);
        i++;
      }
      dispatch(effProcessEvents, 0, 0, events, 0.f);
      f();
    }

    auto& prepareInput(std::size_t samples)
    {
      auto& ip = m_inlets[0]->data.template target<ossia::audio_port>()->samples;
      if(ip.size() < 2)
        ip.resize(2);
      if(ip[0].size() < samples)
      {
        ip[0].resize(samples);
      }
      if(ip[1].size() < samples)
      {
        ip[1].resize(samples);
      }
      return ip;
    }
    auto& prepareOutput(std::size_t samples)
    {
      auto& op = m_outlets[0]->data.template target<ossia::audio_port>()->samples;
      op.resize(2);
      for(auto& chan : op)
        chan.resize(samples);
      return op;
    }
    void run(ossia::token_request tk, ossia::execution_state&) override
    {
      if(tk.date > m_prev_date)
      {
        const std::size_t samples = tk.date - m_prev_date;
        setControls();

        if constexpr(UseDouble)
        {
          if constexpr(IsSynth)
          {
            dispatchMidi([=] {
              auto& op = prepareOutput(samples);
              double* output[2] = { op[0].data(), op[1].data() };

              fx.processDoubleReplacing(&fx, nullptr, output, samples);
            });
          }
          else
          {
            // copy audio data
            auto& ip = prepareInput(samples);
            double* input[2] = { ip[0].data(), ip[1].data() };

            auto& op = prepareOutput(samples);
            double* output[2] = { op[0].data(), op[1].data() };

            fx.processDoubleReplacing(&fx, input, output, samples);
          }
        }
        else
        {
          if constexpr(IsSynth)
          {
            dispatchMidi([=] {
              auto& op = m_outlets[0]->data.template target<ossia::audio_port>()->samples;
              for(auto& vec : float_v)
                vec.resize(samples);
              float* output[2] = { float_v[0].data(), float_v[1].data() };

              fx.processReplacing(&fx, output, output, samples);

              op.clear();
              op.emplace_back(float_v[0].begin(), float_v[0].end());
              op.emplace_back(float_v[1].begin(), float_v[1].end());
            });
          }
          else
          {
            // copy audio data

            auto& ip = m_inlets[0]->data.template target<ossia::audio_port>()->samples;
            auto& op = m_outlets[0]->data.template target<ossia::audio_port>()->samples;
            if(ip.size() < 2)
              ip.resize(2);

            float_v[0].assign(ip[0].begin(), ip[0].end());
            float_v[1].assign(ip[1].begin(), ip[1].end());
            float_v[0].resize(samples);
            float_v[1].resize(samples);
            float* output[2] = { float_v[0].data(), float_v[1].data() };

            fx.processReplacing(&fx, output, output, samples);

            op.clear();
            op.emplace_back(float_v[0].begin(), float_v[0].end());
            op.emplace_back(float_v[1].begin(), float_v[1].end());
          }
        }
      }
    }

    std::array<std::vector<float>, 2> float_v;
};
template<bool b1, bool b2, typename... Args>
auto make_vst_fx(Args&... args) {
  return std::make_shared<VSTNode<b1, b2>>(args...);
}

}
}
