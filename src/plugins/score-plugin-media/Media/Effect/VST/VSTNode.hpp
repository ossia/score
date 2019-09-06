#pragma once
#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Process/Dataflow/TimeSignature.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_signature.hpp>
namespace Media
{
namespace VST
{

class vst_node_base : public ossia::graph_node
{
protected:
  explicit vst_node_base(std::shared_ptr<AEffectWrapper>&& ptr)
    : fx{std::move(ptr)}
  {
    m_inlets.reserve(10);
    controls.reserve(10);
  }

  std::shared_ptr<AEffectWrapper> fx{};
  struct vst_control
  {
    int idx{};
    float value{};
    ossia::value_port* port{};
  };

  inline void dispatch(
      int32_t opcode,
      int32_t index = 0,
      intptr_t value = 0,
      void* ptr = nullptr,
      float opt = 0.0f)
  {
    fx->dispatch(opcode, index, value, ptr, opt);
  }

public:
  ossia::small_vector<vst_control, 16> controls;

  void setControls()
  {
    for (vst_control& p : controls)
    {
      const auto& vec = p.port->get_data();
      if (vec.empty())
        continue;
      if (auto t = last(vec).template target<float>())
      {
        p.value = ossia::clamp<float>(*t, 0.f, 1.f);
        fx->fx->setParameter(fx->fx, p.idx, p.value);
      }
    }
  }

  auto& prepareInput(std::size_t samples)
  {
    auto& ip = m_inlets[0]->data.template target<ossia::audio_port>()->samples;
    if (ip.size() < 2)
      ip.resize(2);
    if (ip[0].size() < samples)
    {
      ip[0].resize(samples);
    }
    if (ip[1].size() < samples)
    {
      ip[1].resize(samples);
    }
    return ip;
  }

  auto& prepareOutput(std::size_t samples)
  {
    auto& op
        = m_outlets[0]->data.template target<ossia::audio_port>()->samples;
    op.resize(2);
    for (auto& chan : op)
      chan.resize(samples);
    return op;
  }

  void setupTimeInfo(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    auto& time_info = fx->info;
    time_info.samplePos = tk.date.impl;
    time_info.sampleRate = st.sampleRate();
    time_info.nanoSeconds = st.currentDate() - st.startDate();
    time_info.tempo = tk.tempo;
    time_info.ppqPos
        = (tk.date.impl / st.sampleRate()) * (60. / time_info.tempo);
    time_info.barStartPos = 0.;
    time_info.cycleStartPos = 0.;
    time_info.cycleEndPos = 0.;
    time_info.timeSigNumerator = tk.signature.upper;
    time_info.timeSigDenominator = tk.signature.lower;
    time_info.smpteOffset = 0;
    time_info.smpteFrameRate = 0;
    time_info.samplesToNextClock = 0;
    time_info.flags = kVstTransportPlaying | kVstNanosValid | kVstPpqPosValid
                      | kVstTempoValid | kVstTimeSigValid | kVstClockValid;
  }
};

template <bool UseDouble, bool IsSynth>
class vst_node final : public vst_node_base
{
public:
  vst_node(std::shared_ptr<AEffectWrapper> dat, int sampleRate)
      : vst_node_base{std::move(dat)}
  {
    // Midi or audio input
    if constexpr (IsSynth)
      m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
    else
      m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());

    // tempo
    m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    // time signature
    m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

    // audio output
    m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());

    dispatch(effSetSampleRate, 0, sampleRate, nullptr, sampleRate);
    dispatch(effSetBlockSize, 0, 4096, nullptr, 4096); // Generalize what's in pd
    dispatch(
        effSetProcessPrecision,
        0,
        UseDouble ? kVstProcessPrecision64 : kVstProcessPrecision32);
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

  std::string label() const noexcept override { return "VST"; }

  void all_notes_off() noexcept override
  {
    if constexpr (IsSynth)
    {
      // copy midi data
      VstEvents* events
          = (VstEvents*)alloca(sizeof(VstEvents) + sizeof(void*) * 16);
      events->numEvents = 16;

      VstMidiEvent ev[16] = {};
      // std::memset(&e, 0, sizeof(VstMidiEvent));

      for (int i = 0; i < 16; i++)
      {
        auto& e = ev[i];
        e.type = kVstMidiType;
        e.byteSize = sizeof(VstMidiEvent);

        e.midiData[0] = (char)(uint8_t)176;
        e.midiData[1] = (char)(uint8_t)123;
        e.midiData[2] = 0;
        e.midiData[3] = 0;

        events->events[i] = reinterpret_cast<VstEvent*>(&e);
      }

      dispatch(effProcessEvents, 0, 0, events, 0.f);

      constexpr int samples = 64;
      float dummy[samples];

      for (auto& vec : float_v)
        vec.resize(samples);

      float** output = (float**)alloca(
          sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
      output[0] = float_v[0].data();
      output[1] = float_v[1].data();
      for (int i = 2; i < this->fx->fx->numOutputs; i++)
        output[i] = dummy;

      fx->fx->processReplacing(fx->fx, output, output, samples);
    }
  }

  // Note: the function that does the actual tick is passed as an argument
  // since some plug-ins only store pointers to VstEvents struct,
  // which would go out of scope if this function was just called like this.
  template <typename Fun>
  void dispatchMidi(Fun&& f)
  {
    // copy midi data
    auto& ip = m_inlets[0]->data.template target<ossia::midi_port>()->messages;
    const auto n_mess = ip.size();
    if (n_mess == 0)
    {
      f();
      return;
    }

    // -2 since two are already available ?
    VstEvents* events
        = (VstEvents*)alloca(sizeof(VstEvents) + sizeof(void*) * n_mess);
    events->numEvents = n_mess;

    ossia::small_vector<VstMidiEvent, 16> vec;
    vec.resize(n_mess);
    std::size_t i = 0;
    for (rtmidi::message& mess : ip)
    {
      VstMidiEvent& e = vec[i];
      std::memset(&e, 0, sizeof(VstMidiEvent));

      e.type = kVstMidiType;
      e.byteSize = sizeof(VstMidiEvent);
      e.deltaFrames = mess.timestamp;
      e.flags = kVstMidiEventIsRealtime;

      for (std::size_t k = 0; k < std::min(mess.bytes.size(), (std::size_t)4);
           k++)
        e.midiData[k] = mess.bytes[k];

      events->events[i] = reinterpret_cast<VstEvent*>(&e);
      i++;
    }
    dispatch(effProcessEvents, 0, 0, events, 0.f);
    f();
  }


  void
  run(ossia::token_request tk, ossia::exec_state_facade st) noexcept override
  {
    if (!muted() && tk.date > tk.prev_date)
    {
      const std::size_t samples = tk.date - tk.prev_date;
      this->setControls();
      this->setupTimeInfo(tk, st);

      if constexpr (UseDouble)
      {
        if constexpr (IsSynth)
        {
          dispatchMidi([=] {
            auto& op = prepareOutput(samples);
            const auto max_io
                = std::max(this->fx->fx->numOutputs, this->fx->fx->numInputs);
            double** output
                = (double**)alloca(sizeof(double*) * std::max(2, max_io));
            output[0] = op[0].data();
            output[1] = op[1].data();
            double* dummy = (double*)alloca(sizeof(double) * samples);
            for (int i = 2; i < max_io; i++)
              output[i] = dummy;

            fx->fx->processDoubleReplacing(fx->fx, output, output, samples);
          });
        }
        else
        {
          // copy audio data
          double* dummy = (double*)alloca(sizeof(double) * samples);

          auto& ip = prepareInput(samples);
          double** input = (double**)alloca(
              sizeof(double*) * std::max(2, this->fx->fx->numInputs));
          input[0] = ip[0].data();
          input[1] = ip[1].data();
          for (int i = 2; i < this->fx->fx->numInputs; i++)
            input[i] = dummy;

          auto& op = prepareOutput(samples);
          double** output = (double**)alloca(
              sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
          output[0] = op[0].data();
          output[1] = op[1].data();

          for (int i = 2; i < this->fx->fx->numOutputs; i++)
            output[i] = dummy;

          fx->fx->processDoubleReplacing(fx->fx, input, output, samples);
        }
      }
      else
      {
        if constexpr (IsSynth)
        {
          dispatchMidi([=] {
            float* dummy = (float*)alloca(sizeof(float) * samples);
            auto& op = m_outlets[0]
                           ->data.template target<ossia::audio_port>()
                           ->samples;
            for (auto& vec : float_v)
              vec.resize(samples);

            const auto max_io
                = std::max(this->fx->fx->numOutputs, this->fx->fx->numInputs);
            float** output
                = (float**)alloca(sizeof(float*) * std::max(2, max_io));
            output[0] = float_v[0].data();
            output[1] = float_v[1].data();
            for (int i = 2; i < max_io; i++)
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

          auto& ip = m_inlets[0]
                         ->data.template target<ossia::audio_port>()
                         ->samples;
          auto& op = m_outlets[0]
                         ->data.template target<ossia::audio_port>()
                         ->samples;
          if (ip.size() < 2)
            ip.resize(2);

          float_v[0].assign(ip[0].begin(), ip[0].end());
          float_v[1].assign(ip[1].begin(), ip[1].end());
          float_v[0].resize(samples);
          float_v[1].resize(samples);

          float* dummy = (float*)alloca(sizeof(float) * samples);

          const auto max_io
              = std::max(this->fx->fx->numOutputs, this->fx->fx->numInputs);
          float** output
              = (float**)alloca(sizeof(float*) * std::max(2, max_io));
          output[0] = float_v[0].data();
          output[1] = float_v[1].data();
          for (int i = 2; i < max_io; i++)
            output[i] = dummy;

          fx->fx->processReplacing(fx->fx, output, output, samples);

          op.clear();
          op.emplace_back(float_v[0].begin(), float_v[0].end());
          op.emplace_back(float_v[1].begin(), float_v[1].end());
          float_v[0].clear();
          float_v[1].clear();
        }
      }
    }
  }

  std::array<ossia::float_vector, 2> float_v;
};
template <bool b1, bool b2, typename... Args>
auto make_vst_fx(Args&... args)
{
  return std::make_shared<vst_node<b1, b2>>(args...);
}
}
}
#endif
