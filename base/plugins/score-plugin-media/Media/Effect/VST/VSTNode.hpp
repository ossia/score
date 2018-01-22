#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/TimeSignature.hpp>
#include <ossia/dataflow/fx_node.hpp>

namespace Media
{
namespace VST
{
using time_signature = std::pair<uint16_t, uint16_t>;
template<bool UseDouble, bool IsSynth>
class vst_node final : public ossia::graph_node
{
  private:
    std::shared_ptr<AEffectWrapper> fx{};
    double m_tempo{120};
    time_signature m_sig{4, 4};

    void dispatch(int32_t opcode, int32_t index = 0, intptr_t value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      fx->dispatch(opcode, index, value, ptr, opt);
    }
  public:
    ossia::small_vector<std::pair<int, ossia::value_port*>, 10> ctrl_ptrs;

    vst_node(std::shared_ptr<AEffectWrapper> dat, int sampleRate):
      fx{std::move(dat)}
    {
      m_inlets.reserve(10);
      ctrl_ptrs.reserve(10);
      if constexpr(IsSynth)
        m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
      else
        m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());

      m_inlets.push_back(ossia::make_inlet<ossia::value_port>()); // tempo
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>()); // time signature

      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());

      dispatch(effSetSampleRate, 0, 0, 0, sampleRate);
      dispatch(effSetBlockSize, 0, 0, 0, 4096); // Generalize what's in pd
      dispatch(effSetProcessPrecision, 0, UseDouble ? kVstProcessPrecision64 : kVstProcessPrecision32);
      dispatch(effMainsChanged, 0, 1);
      dispatch(effStartProcess);

      fx->fx->resvd2 = reinterpret_cast<intptr_t>(this);
    }

    ~vst_node()
    {
      fx->fx->resvd2 = 0;
      dispatch(effStopProcess);
      dispatch(effMainsChanged, 0, 0);
    }

    std::string label() const override
    {
      return "VST";
    }

    void all_notes_off() override
    {
      // copy midi data
      VstEvents* events = (VstEvents*)alloca(sizeof(VstEvents));
      events->numEvents = 1;

      VstMidiEvent e;
      std::memset(&e, 0, sizeof(VstMidiEvent));

      e.type = kVstMidiType;
      e.byteSize = sizeof(VstMidiEvent);
      // TODO do it for all channel
      e.midiData[0] = (char)(uint8_t)176;
      e.midiData[1] = (char)(uint8_t)123;
      e.midiData[2] = 0;
      e.midiData[3] = 0;

      events->events[0] = reinterpret_cast<VstEvent*>(&e);

      dispatch(effProcessEvents, 0, 0, events, 0.f);
/*
      const auto nfloats = std::max(fx->fx->numInputs, fx->fx->numOutputs);
      if constexpr(UseDouble)
      {
        double f;
        double** c = (double**)alloca(sizeof(double*) * nfloats);
        for(int i = 0; i < nfloats; i++)
          c[i] = &f;
        fx->fx->processDoubleReplacing(fx, c, c, 1);
      }
      else
      {
        float f{};
        float** c = (float**)alloca(sizeof(float*) * nfloats);
        for(int i = 0; i < nfloats; i++)
          c[i] = &f;
        fx->fx->processReplacing(fx, c, c, 1);
      }*/
    }

    void setControls()
    {
      auto& tempo = inputs()[1]->data.template target<ossia::value_port>()->get_data();
      if(!tempo.empty())
      {
        m_tempo = ossia::convert<double>(tempo.rbegin()->value);
      }
      auto& ts = inputs()[2]->data.template target<ossia::value_port>()->get_data();
      if(!ts.empty())
      {
        auto str = ts.rbegin()->value.template target<std::string>();
        if(str)
        {
          if(auto sig = Control::get_time_signature(*str))
            m_sig = *sig;
        }
      }
      for(auto p : ctrl_ptrs)
      {
        auto& vec = p.second->get_data();
        if(vec.empty())
          continue;
        if(auto t = last(vec).template target<float>())
        {
          fx->fx->setParameter(fx->fx, p.first, *t);
        }
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

      ossia::small_vector<VstMidiEvent, 16> vec;
      vec.resize(n_mess);
      std::size_t i = 0;
      for(mm::MidiMessage& mess : ip)
      {
        VstMidiEvent& e = vec[i];
        std::memset(&e, 0, sizeof(VstMidiEvent));

        e.type = kVstMidiType;
        e.byteSize = sizeof(VstMidiEvent);
        e.deltaFrames = mess.timestamp;
        e.flags = kVstMidiEventIsRealtime;

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

    void setupTimeInfo(const ossia::token_request& tk, ossia::execution_state& st)
    {
      auto& time_info = fx->info;
      time_info.samplePos = tk.date.impl;
      time_info.sampleRate = st.sampleRate;
      time_info.nanoSeconds = st.cur_date - st.start_date;
      time_info.tempo = m_tempo;
      time_info.ppqPos = (tk.date.impl / st.sampleRate) * (60. / time_info.tempo);
      time_info.barStartPos = 0.;
      time_info.cycleStartPos = 0.;
      time_info.cycleEndPos = 0.;
      time_info.timeSigNumerator = m_sig.first;
      time_info.timeSigDenominator = m_sig.second;
      time_info.smpteOffset = 0;
      time_info.smpteFrameRate = 0;
      time_info.samplesToNextClock = 0;
      time_info.flags = kVstTransportPlaying & kVstNanosValid & kVstPpqPosValid & kVstTempoValid & kVstTimeSigValid & kVstClockValid;
    }
    void run(ossia::token_request tk, ossia::execution_state& st) override
    {
      if(tk.date > m_prev_date)
      {
        const std::size_t samples = tk.date - m_prev_date;
        setControls();
        setupTimeInfo(tk, st);

        if constexpr(UseDouble)
        {
          if constexpr(IsSynth)
          {
            dispatchMidi([=] {
              auto& op = prepareOutput(samples);
              double** output = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
              output[0] = op[0].data();
              output[1] = op[1].data();
              double* dummy = (double*)alloca(sizeof(double) * samples);
              for(int i = 2; i < this->fx->fx->numOutputs; i++)
                output[i] = dummy;

              fx->fx->processDoubleReplacing(fx->fx, output, output, samples);
            });
          }
          else
          {
            // copy audio data
            double* dummy = (double*)alloca(sizeof(double) * samples);

            auto& ip = prepareInput(samples);
            double** input = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numInputs));
            input[0] = ip[0].data();
            input[1] = ip[1].data();
            for(int i = 2; i < this->fx->fx->numInputs; i++)
              input[i] = dummy;

            auto& op = prepareOutput(samples);
            double** output = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
            output[0] = op[0].data();
            output[1] = op[1].data();

            for(int i = 2; i < this->fx->fx->numOutputs; i++)
              output[i] = dummy;

            fx->fx->processDoubleReplacing(fx->fx, input, output, samples);
          }
        }
        else
        {
          if constexpr(IsSynth)
          {
            dispatchMidi([=] {
              float* dummy = (float*)alloca(sizeof(float) * samples);
              auto& op = m_outlets[0]->data.template target<ossia::audio_port>()->samples;
              for(auto& vec : float_v)
                vec.resize(samples);

              float** output = (float**)alloca(sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
              output[0] = float_v[0].data();
              output[1] = float_v[1].data();
              for(int i = 2; i < this->fx->fx->numOutputs; i++)
                output[i] = dummy;

              fx->fx->processReplacing(fx->fx, output, output, samples);

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

            float* dummy = (float*)alloca(sizeof(float) * samples);

            float** output = (float**)alloca(sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
            output[0] = float_v[0].data();
            output[1] = float_v[1].data();
            for(int i = 2; i < this->fx->fx->numOutputs; i++)
              output[i] = dummy;

            fx->fx->processReplacing(fx->fx, output, output, samples);

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
  return std::make_shared<vst_node<b1, b2>>(args...);
}

}
}
