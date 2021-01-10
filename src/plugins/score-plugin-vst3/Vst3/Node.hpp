#pragma once
#include <Vst3/EffectModel.hpp>
#include <Process/Dataflow/TimeSignature.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_signature.hpp>
#include <ossia/detail/logger.hpp>
#include <public.sdk/source/vst/hosting/eventlist.h>
namespace vst3
{

class vst_node_base : public ossia::graph_node
{
public:
  Plugin fx{};
  // Each element is the amount of channels in a given in/out port
  ossia::small_pod_vector<int, 2> m_audioInputChannels{};
  ossia::small_pod_vector<int, 2> m_audioOutputChannels{};
  int m_totalAudioIns{};
  int m_totalAudioOuts{};


protected:
  explicit vst_node_base(Plugin&& ptr) : fx{std::move(ptr)}
  {
    m_inlets.reserve(10);
    controls.reserve(10);

    struct vis {
      vst_node_base& self;
      void audioIn(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_inlets.push_back(new ossia::audio_inlet);
        self.m_audioInputChannels.push_back(bus.channelCount);
        self.m_totalAudioIns += bus.channelCount;
      }
      void eventIn(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_inlets.push_back(new ossia::midi_inlet);
      }
      void audioOut(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_outlets.push_back(new ossia::audio_outlet);
        self.m_audioOutputChannels.push_back(bus.channelCount);
        self.m_totalAudioOuts += bus.channelCount;
      }
      void eventOut(const Steinberg::Vst::BusInfo& bus, int idx)
      {
        self.m_outlets.push_back(new ossia::midi_outlet);
      }
    };

    forEachBus(vis{*this}, *fx.component);

    if(m_audioInputChannels.empty())
      m_audioInputChannels.push_back(2);
    if(m_audioOutputChannels.empty())
      m_audioOutputChannels.push_back(2);

    m_totalAudioIns = std::max(2, this->m_totalAudioIns);
    m_totalAudioOuts = std::max(2, this->m_totalAudioOuts);

    if (auto err = fx.processor->setProcessing(true);
        err != Steinberg::kResultOk && err != Steinberg::kNotImplemented) {
      ossia::logger().warn("Couldn't set VST3 processing: {}",  err);
    }
  }

  ~vst_node_base()
  {
    if (auto err = fx.processor->setProcessing(false);
        err != Steinberg::kResultOk && err != Steinberg::kNotImplemented) {
      ossia::logger().warn("Couldn't set VST3 processing: {}",  err);
    }
  }

  struct vst_control
  {
    int idx{};
    float value{};
    ossia::value_port* port{};
  };
/*
  inline void dispatch(
      int32_t opcode,
      int32_t index = 0,
      intptr_t value = 0,
      void* ptr = nullptr,
      float opt = 0.0f)
  {
    fx->dispatch(opcode, index, value, ptr, opt);
  }
*/
public:
  ossia::small_vector<vst_control, 16> controls;

  void setControls()
  {
    /*
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
    */
  }

  auto& prepareInput(std::size_t samples)
  {
    auto& ip = m_inlets[0]->template target<ossia::audio_port>()->samples;
    switch (ip.size())
    {
      case 0:
      {
        ip.resize(2);
        ip[0].resize(samples);
        ip[1].resize(samples);
        break;
      }
      case 1:
      {
        ip.resize(2);
        ip[0].resize(samples);
        ip[1].assign(ip[0].begin(), ip[0].end());
        break;
      }
      default:
      {
        for (auto& i : ip)
          i.resize(samples);
        break;
      }
    }

    return ip;
  }

  auto& prepareOutput(std::size_t samples)
  {
    auto& op = m_outlets[0]->template target<ossia::audio_port>()->samples;
    op.resize(2);
    for (auto& chan : op)
      chan.resize(samples);
    return op;
  }

  void setupTimeInfo(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    using namespace Steinberg::Vst;
    using F = ProcessContext;
    Steinberg::Vst::ProcessContext& time_info = this->m_context;
    time_info.sampleRate = st.sampleRate();
    time_info.projectTimeSamples = tk.date.impl;

    time_info.systemTime = st.currentDate() - st.startDate();
    time_info.continousTimeSamples = time_info.projectTimeSamples; // TODO

    time_info.projectTimeMusic = tk.musical_start_position;
    time_info.barPositionMusic = tk.musical_start_last_bar;
    time_info.cycleStartMusic = 0.;
    time_info.cycleEndMusic = 0.;

    time_info.tempo = tk.tempo;
    time_info.timeSigNumerator = tk.signature.upper;
    time_info.timeSigDenominator = tk.signature.lower;

    // time_info.chord = ....;

    time_info.smpteOffsetSubframes = 0;
    time_info.frameRate = {};
    time_info.samplesToNextClock = 0;
    time_info.state =
        F::kPlaying |
        F::kSystemTimeValid | F::kContTimeValid |
        F::kProjectTimeMusicValid | F::kBarPositionValid |
        F::kTempoValid | F::kTimeSigValid
    ;
  }


  Steinberg::Vst::ProcessData m_vstData;
  Steinberg::Vst::AudioBusBuffers m_vstInput;
  Steinberg::Vst::AudioBusBuffers m_vstOutput;

  Steinberg::Vst::ProcessContext m_context;
  ossia::small_vector<Steinberg::Vst::EventList*, 2> m_inputEvents;
  ossia::small_vector<Steinberg::Vst::EventList*, 2> m_outputEvents;
};

template <bool UseDouble>
class vst_node final : public vst_node_base
{
public:
  vst_node(Plugin dat, int sampleRate) : vst_node_base{std::move(dat)}
  {
    m_vstData.processMode = Steinberg::Vst::ProcessModes::kRealtime;
    if constexpr(UseDouble)
      m_vstData.symbolicSampleSize = Steinberg::Vst::kSample64;
    else
      m_vstData.symbolicSampleSize = Steinberg::Vst::kSample32;

    m_vstInput.numChannels = 2;
    m_vstOutput.numChannels = 2;

    m_vstData.numInputs = m_audioInputChannels.size();
    m_vstData.numOutputs = m_audioOutputChannels.size();

    m_vstData.inputs = &m_vstInput;
    m_vstData.outputs = &m_vstOutput;
    // m_vstData.inputEvents = m_inputEvents.data();
    // m_vstData.outputEvents = m_outputEvents.data();
    m_vstData.processContext = &m_context;

    /*

    dispatch(effSetSampleRate, 0, sampleRate, nullptr, sampleRate);
    dispatch(effSetBlockSize, 0, 4096, nullptr, 4096); // Generalize what's in pd
    dispatch(
        effSetProcessPrecision, 0, UseDouble ? kVstProcessPrecision64 : kVstProcessPrecision32);
    dispatch(effMainsChanged, 0, 1);
    dispatch(effStartProcess);

    fx->fx->resvd2 = reinterpret_cast<intptr_t>(this);
    */
  }

  ~vst_node()
  {
    /*
    fx->fx->resvd2 = 0;
    dispatch(effStopProcess);
    dispatch(effMainsChanged, 0, 0);
    */
  }

  std::string label() const noexcept override { return "VST3"; }

  void all_notes_off() noexcept override
  {
    /*
    if constexpr (IsSynth)
    {
      // copy midi data
      // should be 16 but some VSTs read a bit out of bounds apparently !
      constexpr auto sz = sizeof(VstEvents) + sizeof(void*) * 16 * 2;
      VstEvents* events = (VstEvents*)alloca(sz);
      events->numEvents = 16;

      VstMidiEvent ev[16] = {};
      std::memset(events, 0, sz);

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

      if constexpr(!UseDouble)
      {
        constexpr int samples = 64;
        float dummy[samples] = { 0.f };

        float** output = (float**)alloca(sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
        for (int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processReplacing(fx->fx, output, output, samples);
      }
      else
      {
        constexpr int samples = 64;
        double dummy[samples] = { 0.f };

        double** output = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
        for (int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processDoubleReplacing(fx->fx, output, output, samples);

      }
    }
    */
  }

  void dispatchMidi()
  {
    /*
    // copy midi data
    auto& ip = static_cast<ossia::midi_inlet*>(m_inlets[1])->data.messages;
    const auto n_mess = ip.size();
    if (n_mess == 0)
    {
      f();
      return;
    }

    // -2 since two are already available ?
    const auto sz = sizeof(VstEvents) + sizeof(void*) * n_mess * 2;
    VstEvents* events = (VstEvents*)alloca(sz);
    std::memset(events, 0, sz);
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

      std::memcpy(e.midiData, mess.bytes.data(), std::min(mess.bytes.size(), (std::size_t)4));
      // for (std::size_t k = 0, N = std::min(mess.bytes.size(),
      // (std::size_t)4); k < N; k++)
      //   e.midiData[k] = mess.bytes[k];

      events->events[i] = reinterpret_cast<VstEvent*>(&e);
      i++;
    }
    dispatch(effProcessEvents, 0, 0, events, 0.f);
    f();
    */
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {

    if (!muted() && tk.date > tk.prev_date)
    {
      const std::size_t samples = tk.physical_write_duration(st.modelToSamples());
      this->setControls();
      this->setupTimeInfo(tk, st);

      dispatchMidi();
      if constexpr (UseDouble)
      {
        processDouble(samples);
      }
      else
      {
        processFloat(samples);
      }

      // upmix mono VSTs to stereo
      if (this->m_audioOutputChannels[0] == 1)
      {
        auto& op = m_outlets[0]->template target<ossia::audio_port>()->samples;
        op[1].assign(op[0].begin(), op[0].end());
      }

    }
  }

  void processFloat(std::size_t samples)
  {
    if constexpr (!UseDouble)
    {
      // copy audio data
      auto& ip = m_inlets[0]->template cast<ossia::audio_port>().samples;
      auto& op = m_outlets[0]->template cast<ossia::audio_port>().samples;
      if (ip.size() < 2)
        ip.resize(2);

      float_v[0].assign(ip[0].begin(), ip[0].end());
      float_v[1].assign(ip[1].begin(), ip[1].end());
      float_v[0].resize(samples);
      float_v[1].resize(samples);

      float* dummy = (float*)alloca(sizeof(float) * samples);
      std::fill_n(dummy, samples, 0.f);

      const auto max_io = std::max(this->m_totalAudioIns, this->m_totalAudioOuts);
      float** output = (float**)alloca(sizeof(float*) * max_io);
      output[0] = float_v[0].data();
      output[1] = float_v[1].data();
      for (int i = 2; i < max_io; i++)
        output[i] = dummy;

      {
        m_vstData.numSamples = samples;

        m_vstInput.silenceFlags = 0;
        m_vstInput.channelBuffers32 = output;

        m_vstOutput.silenceFlags = 0;
        m_vstOutput.channelBuffers32 = output;

        fx.processor->process(m_vstData);
      }

      op.clear();
      op.emplace_back(float_v[0].begin(), float_v[0].end());
      op.emplace_back(float_v[1].begin(), float_v[1].end());
      float_v[0].clear();
      float_v[1].clear();
    }
  }

  void processDouble(std::size_t samples)
  {
    if constexpr (UseDouble)
    {
    // copy audio data
    double* dummy = (double*)alloca(sizeof(double) * samples);
    std::fill_n(dummy, samples, 0.);

    auto& ip = prepareInput(samples);
    double** input
        = (double**)alloca(sizeof(double*) * m_totalAudioIns);
    input[0] = ip[0].data();
    input[1] = ip[1].data();
    for (int i = 2; i < this->m_totalAudioIns; i++)
      input[i] = dummy;

    auto& op = prepareOutput(samples);
    double** output
        = (double**)alloca(sizeof(double*) * m_totalAudioOuts);
    output[0] = op[0].data();
    output[1] = op[1].data();

    for (int i = 2; i < this->m_totalAudioOuts; i++)
      output[i] = dummy;

    {
      m_vstData.numSamples = samples;

      m_vstInput.silenceFlags = 0;
      m_vstInput.channelBuffers64 = input;

      m_vstOutput.silenceFlags = 0;
      m_vstOutput.channelBuffers64 = output;

      fx.processor->process(m_vstData);
    }
    }
  }

  struct dummy_t { };
  std::conditional_t<!UseDouble, std::array<ossia::float_vector, 2>, dummy_t> float_v;
};

template <bool b1, typename... Args>
auto make_vst_fx(Args&... args)
{
  return std::make_shared<vst_node<b1>>(args...);
}
}
