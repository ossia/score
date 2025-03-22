#pragma once
#include <Process/Dataflow/TimeSignature.hpp>

#include <Vst3/EffectModel.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/editor/scenario/time_signature.hpp>

#include <libremidi/ump_events.hpp>
#include <pluginterfaces/vst/ivstmidicontrollers.h>

#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>
namespace vst3
{

class param_queue final : public Steinberg::Vst::IParamValueQueue
{
public:
  explicit param_queue(Steinberg::Vst::ParamID id)
      : id{id}
  {
  }
  ~param_queue() { }

  Steinberg::Vst::ParamID id{};
  ossia::small_vector<std::pair<int32_t, Steinberg::Vst::ParamValue>, 1> data;
  Steinberg::Vst::ParamValue lastValue{};

  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 addRef() override { return 1; }
  Steinberg::uint32 release() override { return 1; }

  Steinberg::Vst::ParamID getParameterId() override { return id; }
  Steinberg::int32 getPointCount() override { return data.size(); }
  Steinberg::tresult getPoint(
      Steinberg::int32 index, Steinberg::int32& sampleOffset,
      Steinberg::Vst::ParamValue& value) override
  {
    if(ossia::valid_index(index, data))
      std::tie(sampleOffset, value) = data[index];
    else if(index == -1)
    {
      sampleOffset = 0;
      value = lastValue;
    }

    return Steinberg::kResultOk;
  }

  Steinberg::tresult addPoint(
      Steinberg::int32 sampleOffset, Steinberg::Vst::ParamValue value,
      Steinberg::int32& index) override
  {
    index = data.size();
    data.emplace_back(sampleOffset, value);
    return Steinberg::kResultOk;
  }
};

class param_changes final : public Steinberg::Vst::IParameterChanges
{
public:
  std::vector<param_queue> queues;
  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 addRef() override { return 1; }
  Steinberg::uint32 release() override { return 1; }

  Steinberg::int32 getParameterCount() override { return queues.size(); }

  param_queue* getParameterData(Steinberg::int32 index) override
  {
    return &queues[index];
  }

  param_queue* addParameterData(
      const Steinberg::Vst::ParamID& id, Steinberg::int32& index /*out*/) override
  {
    index = queues.size();
    queues.emplace_back(id);
    return &queues.back();
  }
};

class vst_node_base : public ossia::graph_node
{
public:
  struct PluginHandle
  {
    explicit PluginHandle(const Plugin& p)
        : component{p.component}
        , processor{p.processor}
        , midi_controls{p.midiControls}
    {
      component->addRef();
      processor->addRef();
    }

    ~PluginHandle()
    {
      // qDebug() << processor->release();
      // qDebug() << component->release();
    }

    Steinberg::Vst::IComponent* component{};
    Steinberg::Vst::IAudioProcessor* processor{};
    MIDIControls midi_controls;
  };

  PluginHandle fx;
  // Each element is the amount of channels in a given in/out port
  ossia::small_pod_vector<int, 2> m_audioInputChannels{};
  ossia::small_pod_vector<int, 2> m_audioOutputChannels{};
  int m_totalAudioIns{};
  int m_totalAudioOuts{};
  int m_totalEventIns{};
  int m_totalEventOuts{};

protected:
  explicit vst_node_base(const Plugin& ptr)
      : fx{std::move(ptr)}
  {
    this->set_not_fp_safe();
    m_inlets.reserve(10);
    controls.reserve(10);

    struct vis
    {
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
        self.m_totalEventIns++;
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
        self.m_totalEventOuts++;
      }
    };

    forEachBus(vis{*this}, *fx.component);

    if(auto err = fx.processor->setProcessing(true);
       err != Steinberg::kResultOk && err != Steinberg::kNotImplemented)
    {
      ossia::logger().warn("Couldn't set VST3 processing: {}", err);
    }

    m_vstData.processMode = Steinberg::Vst::ProcessModes::kRealtime;
    m_vstData.numInputs = m_audioInputChannels.size();
    m_vstData.numOutputs = m_audioOutputChannels.size();
    m_vstInput.resize(m_audioInputChannels.size());
    m_vstOutput.resize(m_audioOutputChannels.size());
    for(std::size_t i = 0; i < m_audioInputChannels.size(); i++)
    {
      m_vstInput[i].numChannels = m_audioInputChannels[i];
    }
    for(std::size_t i = 0; i < m_audioOutputChannels.size(); i++)
    {
      m_vstOutput[i].numChannels = m_audioOutputChannels[i];
    }

    m_vstData.inputs = m_vstInput.data();
    m_vstData.outputs = m_vstOutput.data();
    m_vstData.inputParameterChanges = &m_inputChanges;
    m_vstData.outputParameterChanges = &m_outputChanges;
    m_vstData.inputEvents = &m_inputEvents;
    m_vstData.outputEvents = &m_outputEvents;
    m_vstData.processContext = &m_context;

    m_inputEvents.setMaxSize(17 * 128 * m_totalEventIns);
    m_outputEvents.setMaxSize(128 * m_totalEventOuts);
  }

  ~vst_node_base()
  {
    if(auto err = fx.processor->setProcessing(false);
       err != Steinberg::kResultOk && err != Steinberg::kNotImplemented)
    {
      ossia::logger().warn("Couldn't set VST3 processing: {}", err);
    }
  }

  struct vst_control
  {
    Steinberg::Vst::ParamID idx{};
    std::size_t queue_idx{};
    ossia::value_port* port{};
  };

  ossia::hash_map<Steinberg::Vst::ParamID, std::size_t> queue_map;

public:
  ossia::small_vector<vst_control, 16> controls;

  std::size_t add_control(ossia::value_inlet* inlet, Steinberg::Vst::ParamID id, float v)
  {
    (**inlet).domain = ossia::domain_base<float>{0.f, 1.f};
    (**inlet).type = ossia::val_type::FLOAT;

    // FIXME this allocates a lot :[
    auto queue_idx = this->m_inputChanges.queues.size();
    this->m_inputChanges.queues.emplace_back(id);
    this->m_inputChanges.queues.back().lastValue = v;

    queue_map[id] = queue_idx;
    controls.push_back({id, queue_idx, inlet->target<ossia::value_port>()});
    root_inputs().push_back(std::move(inlet));
    return queue_idx;
  }

  // Used when a control is changed from the ui.
  void set_control(std::size_t queue_idx, float value)
  {
    auto& queue = this->m_inputChanges.queues[queue_idx];
    queue.lastValue = value;
    queue.data.clear();
    queue.data.emplace_back(0, value);
  }

  void setControls()
  {
    for(vst_control& p : controls)
    {
      const auto& vec = p.port->get_data();
      if(vec.empty())
        continue;
      if(auto t = last(vec).target<float>())
      {
        double value = ossia::clamp<double>((double)*t, 0., 1.);
        auto& queue = m_inputChanges.queues[p.queue_idx];
        queue.data.clear();
        queue.data.emplace_back(0, value);
        queue.lastValue = value;
      }
    }
  }

  void dispatchMidi()
  {
    m_inputEvents.clear();
    m_outputEvents.clear();

    int k = 0;
    int audioBusCount = std::ssize(m_audioInputChannels);
    for(int i = audioBusCount; i < audioBusCount + m_totalEventIns; i++)
    {
      dispatchMidi(*m_inlets[i]->template target<ossia::midi_port>(), k++);
    }
    m_vstData.inputEvents
        = (m_inputEvents.getEventCount() > 0) ? &m_inputEvents : nullptr;
    m_vstData.outputEvents = &m_outputEvents;
  }

  void dispatchMidi(ossia::midi_port& port, int index)
  {
    // copy midi data
    auto& ip = port.messages;
    if(ip.empty())
      return;

    using VstEvent = Steinberg::Vst::Event;
    VstEvent e;
    e.busIndex = index;
    e.sampleOffset = 0;
    e.ppqPosition = 0; // FIXME
    for(const libremidi::ump& mess : ip)
    {
      if(mess.get_type() != libremidi::midi2::message_type::MIDI_2_CHANNEL)
        continue;
      e.sampleOffset = mess.timestamp;
      switch(libremidi::message_type(mess.get_status_code()))
      {
        case libremidi::message_type::NOTE_ON: {
          auto [channel, note, value] = libremidi::as_01::note_off(mess);

          if(value > 0)
          {
            e.type = VstEvent::kNoteOnEvent;
            e.noteOn.channel = channel; // FIXME 0 or 1-based?
            e.noteOn.pitch = note;
            e.noteOn.velocity = value;
            e.noteOn.noteId = -1;
            e.noteOn.tuning = 0.f;
            m_inputEvents.addEvent(e);
          }
          else
          {
            e.type = VstEvent::kNoteOffEvent;
            e.noteOff.channel = mess.get_channel();
            e.noteOff.pitch = note;
            e.noteOff.velocity = 0;
            e.noteOff.noteId = -1;
            e.noteOff.tuning = 0.f;
            m_inputEvents.addEvent(e);
          }
          break;
        }
        case libremidi::message_type::NOTE_OFF: {
          auto [channel, note, value] = libremidi::as_01::note_off(mess);
          e.type = VstEvent::kNoteOffEvent;
          e.noteOff.channel = channel;
          e.noteOff.pitch = note;
          e.noteOff.velocity = value;
          e.noteOff.noteId = -1;
          e.noteOff.tuning = 0.f;
          m_inputEvents.addEvent(e);
          break;
        }
        case libremidi::message_type::POLY_PRESSURE: {
          auto [channel, note, value] = libremidi::as_01::poly_pressure(mess);
          e.type = VstEvent::kPolyPressureEvent;
          e.polyPressure.channel = channel;
          e.polyPressure.pitch = note;
          e.polyPressure.pressure = value;
          e.polyPressure.noteId = -1;
          m_inputEvents.addEvent(e);
          break;
        }

        case libremidi::message_type::PITCH_BEND: {
          if(auto it = this->fx.midi_controls.find({index, Steinberg::Vst::kPitchBend});
             it != this->fx.midi_controls.end())
          {
            auto [channel, value] = libremidi::as_01::pitch_bend(mess);
            Steinberg::Vst::ParamID pid = it->second;
            if(auto queue_it = this->queue_map.find(pid);
               queue_it != this->queue_map.end())
            {
              auto& queue = this->m_inputChanges.queues[queue_it->second];
              queue.data.push_back({e.sampleOffset, value});
              queue.lastValue = value;
            }
          }
        }

        case libremidi::message_type::AFTERTOUCH: {
          if(auto it = this->fx.midi_controls.find({index, Steinberg::Vst::kAfterTouch});
             it != this->fx.midi_controls.end())
          {
            auto [channel, value] = libremidi::as_01::aftertouch(mess);
            Steinberg::Vst::ParamID pid = it->second;
            if(auto queue_it = this->queue_map.find(pid);
               queue_it != this->queue_map.end())
            {
              auto& queue = this->m_inputChanges.queues[queue_it->second];
              queue.data.push_back({e.sampleOffset, value});
              queue.lastValue = value;
            }
          }
        }
        default:
          break;
      }
    }
  }

  void readbackMidi()
  {
    using VstEvent = Steinberg::Vst::Event;

    const int audioBusCount = std::ssize(m_audioOutputChannels);
    const int N = m_outputEvents.getEventCount();

    for(int i = 0; i < N; i++)
    {
      auto event_p = m_outputEvents.getEventByIndex(i);
      if(!event_p)
        continue;
      VstEvent& e = *event_p;

      int bus = e.busIndex;
      auto& port = *m_outlets[bus + audioBusCount]->template target<ossia::midi_port>();

      libremidi::ump mess;

      switch(e.type)
      {
        case VstEvent::kNoteOnEvent: {
          if(e.noteOn.velocity > 0.f)
            mess = libremidi::from_01::note_on(
                e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity);
          else
            mess = libremidi::from_01::note_off(e.noteOn.channel, e.noteOn.pitch, 0.);
          break;
        }
        case VstEvent::kNoteOffEvent: {
          mess = libremidi::from_01::note_off(
              e.noteOff.channel, e.noteOff.pitch, e.noteOff.velocity);
          break;
        }
        case VstEvent::kPolyPressureEvent: {
          mess = libremidi::from_01::poly_pressure(
              e.noteOff.channel, e.polyPressure.pitch, e.polyPressure.pressure);
          break;
        }
        default:
          break;
      }

      mess.timestamp = e.sampleOffset;
      port.messages.push_back(std::move(mess));
    }
  }

  auto& preparePort(ossia::audio_port& port, int numChannels, std::size_t samples)
  {
    port.set_channels(numChannels);

    for(auto& i : port)
      i.resize(samples);
    return port.get();
  }

  void setupTimeInfo(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    using namespace Steinberg::Vst;
    using F = ProcessContext;
    Steinberg::Vst::ProcessContext& time_info = this->m_context;
    time_info.sampleRate = st.sampleRate();

    time_info.projectTimeSamples = this->m_processed_frames;

    time_info.systemTime = st.currentDate() - st.startDate();
    time_info.continousTimeSamples = this->m_processed_frames; // TODO

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
    time_info.state = F::kPlaying | F::kSystemTimeValid | F::kContTimeValid
                      | F::kProjectTimeMusicValid | F::kBarPositionValid | F::kTempoValid
                      | F::kTimeSigValid;
  }

  Steinberg::Vst::ProcessData m_vstData;
  ossia::small_vector<Steinberg::Vst::AudioBusBuffers, 1> m_vstInput;
  ossia::small_vector<Steinberg::Vst::AudioBusBuffers, 1> m_vstOutput;

  Steinberg::Vst::ProcessContext m_context;
  param_changes m_inputChanges;
  param_changes m_outputChanges;
  Steinberg::Vst::EventList m_inputEvents;
  Steinberg::Vst::EventList m_outputEvents;
};

template <bool UseDouble>
class vst_node final : public vst_node_base
{
public:
  vst_node(Plugin dat, int sampleRate)
      : vst_node_base{std::move(dat)}
  {
    if constexpr(UseDouble)
      m_vstData.symbolicSampleSize = Steinberg::Vst::kSample64;
    else
      m_vstData.symbolicSampleSize = Steinberg::Vst::kSample32;
  }

  ~vst_node() { }

  std::string label() const noexcept override { return "VST3"; }

  void all_notes_off(int bus) noexcept
  {
    bool ok = false;
    if(auto it = this->fx.midi_controls.find({bus, Steinberg::Vst::kCtrlAllNotesOff});
       it != this->fx.midi_controls.end())
    {
      Steinberg::Vst::ParamID pid = it->second;
      if(auto queue_it = this->queue_map.find(pid); queue_it != this->queue_map.end())
      {
        auto& queue = this->m_inputChanges.queues[queue_it->second];
        queue.data.push_back({0, 1.});
        queue.lastValue = 1.;
        ok = true;
      }
    }

    if(auto it = this->fx.midi_controls.find({bus, Steinberg::Vst::kCtrlAllSoundsOff});
       it != this->fx.midi_controls.end())
    {
      Steinberg::Vst::ParamID pid = it->second;
      if(auto queue_it = this->queue_map.find(pid); queue_it != this->queue_map.end())
      {
        auto& queue = this->m_inputChanges.queues[queue_it->second];
        queue.data.push_back({0, 1.});
        queue.lastValue = 1.;
        ok = true;
      }
    }

    if(!ok)
    {
      // Send manual note off events
      for(int k = 0; k <= 16; k++)
        for(int i = 0; i <= 127; i++)
        {
          using VstEvent = Steinberg::Vst::Event;
          VstEvent e;
          e.busIndex = bus;
          e.sampleOffset = 0;
          e.ppqPosition = 0; // FIXME
          e.sampleOffset = 0;
          e.type = VstEvent::kNoteOffEvent;
          e.noteOff.channel = k;
          e.noteOff.pitch = i;
          e.noteOff.velocity = 0;
          e.noteOff.noteId = -1;
          e.noteOff.tuning = 0.f;
          m_inputEvents.addEvent(e);
        }
    }
  }

  void all_notes_off() noexcept override
  {
    if(m_totalEventIns == 0)
      return;

    m_inputEvents.clear();

    // Put messages into each MIDI in's event queues
    {
      for(int i = 0; i < m_totalEventIns; i++)
      {
        all_notes_off(i++);
      }
    }

    // Run a process cycle
    {
      constexpr int samples = 64;
      Steinberg::Vst::ProcessData dat;
      memcpy(&dat, &m_vstData, sizeof(m_vstData));
      dat.inputEvents = &m_inputEvents;
      dat.numSamples = samples;

      {
        double** input{};
        double** output{};

        // Copy inputs
        if(m_totalAudioIns > 0)
        {
          input = (double**)alloca(sizeof(double*) * m_totalAudioIns);

          for(int k = 0; k < m_totalAudioIns; k++)
          {
            input[k] = (double*)alloca(sizeof(double) * samples);
            memset(input[k], 0, sizeof(double) * samples);
          }

          for(std::size_t i = 0; i < m_audioInputChannels.size(); i++)
          {
            Steinberg::Vst::AudioBusBuffers& vst_in = dat.inputs[i];
            vst_in.channelBuffers64 = input;
            vst_in.silenceFlags = ~0ULL;
          }
        }

        // Prepare outputs
        if(m_totalAudioOuts > 0)
        {
          output = (double**)alloca(sizeof(double*) * m_totalAudioOuts);
          for(int k = 0; k < m_totalAudioOuts; k++)
          {
            output[k] = (double*)alloca(sizeof(double) * samples);
            memset(output[k], 0, sizeof(double) * samples);
          }

          for(std::size_t i = 0; i < m_audioOutputChannels.size(); i++)
          {
            Steinberg::Vst::AudioBusBuffers& vst_out = dat.outputs[i];
            vst_out.channelBuffers64 = output;
            vst_out.silenceFlags = ~0ULL;
          }
        }

        fx.processor->process(dat);
      }
    }
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if(!muted() && tk.date > tk.prev_date)
    {
      const auto [tick_start, samples] = st.timings(tk);
      this->setControls();
      this->setupTimeInfo(tk, st);

      this->dispatchMidi();

      if constexpr(UseDouble)
      {
        processDouble(samples);
      }
      else
      {
        processFloat(samples);
      }

      this->readbackMidi();
    }
  }

  void processFloat(std::size_t samples)
  {
    // In the float case we have temporary buffers for conversion
    if constexpr(!UseDouble)
    {
      // Prepare buffers
      if(m_totalAudioIns > 0 || m_totalAudioOuts > 0)
      {
        float_v.resize(std::max(m_totalAudioIns, m_totalAudioOuts));
        for(auto& v : float_v)
          v.resize(samples);

        float** input{};
        float** output{};

        // Copy inputs
        if(m_totalAudioIns > 0)
        {
          input = (float**)alloca(sizeof(float*) * m_totalAudioIns);
          int channel_k = 0;
          int float_k = 0;

          for(std::size_t i = 0; i < m_audioInputChannels.size(); i++)
          {
            const int numChannels = m_audioInputChannels[i];
            auto& port = *m_inlets[i]->template target<ossia::audio_port>();
            auto& ip = preparePort(port, numChannels, samples);

            Steinberg::Vst::AudioBusBuffers& vst_in = m_vstInput[i];
            vst_in.channelBuffers32 = input + channel_k;
            vst_in.silenceFlags = 0;

            for(int k = 0; k < numChannels; k++)
            {
              std::copy_n(
                  ip[k].data(), std::min(samples, ip[k].size()),
                  float_v[float_k].data());
              input[channel_k] = float_v[float_k].data();
              channel_k++;
              float_k++;
            }
          }
        }

        // Prepare outputs
        if(m_totalAudioOuts > 0)
        {
          // copy audio data
          output = (float**)alloca(sizeof(float*) * m_totalAudioOuts);

          int channel_k = 0;
          int float_k = 0;
          for(std::size_t i = 0; i < m_audioOutputChannels.size(); i++)
          {
            const int numChannels = m_audioOutputChannels[i];
            auto& port = *m_outlets[i]->template target<ossia::audio_port>();
            preparePort(port, numChannels, samples);

            Steinberg::Vst::AudioBusBuffers& vst_out = m_vstOutput[i];
            vst_out.channelBuffers32 = output + channel_k;
            vst_out.silenceFlags = 0;
            for(int k = 0; k < numChannels; k++)
            {
              output[channel_k] = float_v[float_k].data();
              channel_k++;
              float_k++;
            }
          }
        }
      }

      // Run the process
      {
        m_vstData.numSamples = samples;

        fx.processor->process(m_vstData);
      }

      // Copy the float outputs to the audio outlet buffer
      if(m_totalAudioOuts > 0)
      {
        int float_k = 0;
        for(std::size_t i = 0; i < m_audioOutputChannels.size(); i++)
        {
          const int numChannels = m_audioOutputChannels[i];
          ossia::audio_port& port = *m_outlets[i]->template target<ossia::audio_port>();
          for(int k = 0; k < numChannels; k++)
          {
            auto& audio_out = port.channel(k);
            std::copy_n(float_v[float_k].data(), samples, audio_out.data());
          }
        }
      }
    }
  }

  void processDouble(std::size_t samples)
  {
    // In the double case we use directly the buffers that are part of the
    // input & output ports
    if constexpr(UseDouble)
    {
      double** input{};
      double** output{};

      // Copy inputs
      if(m_totalAudioIns > 0)
      {
        input = (double**)alloca(sizeof(double*) * m_totalAudioIns);

        int channel_k = 0;
        for(std::size_t i = 0; i < m_audioInputChannels.size(); i++)
        {
          const int numChannels = m_audioInputChannels[i];
          auto& port = *m_inlets[i]->template target<ossia::audio_port>();
          auto& ip = preparePort(port, numChannels, samples);

          Steinberg::Vst::AudioBusBuffers& vst_in = m_vstInput[i];
          vst_in.channelBuffers64 = input + channel_k;
          vst_in.silenceFlags = 0;
          for(int k = 0; k < numChannels; k++)
          {
            input[channel_k++] = ip[k].data();
          }
        }
      }

      // Prepare outputs
      if(m_totalAudioOuts > 0)
      {
        output = (double**)alloca(sizeof(double*) * m_totalAudioOuts);
        int channel_k = 0;
        for(std::size_t i = 0; i < m_audioOutputChannels.size(); i++)
        {
          const int numChannels = m_audioOutputChannels[i];
          auto& port = *m_outlets[i]->template target<ossia::audio_port>();
          auto& op = preparePort(port, numChannels, samples);

          Steinberg::Vst::AudioBusBuffers& vst_out = m_vstOutput[i];
          vst_out.channelBuffers64 = output + channel_k;
          vst_out.silenceFlags = 0;
          for(int k = 0; k < numChannels; k++)
          {
            output[channel_k++] = op[k].data();
          }
        }
      }

      // Run process
      {
        m_vstData.numSamples = samples;

        fx.processor->process(m_vstData);
      }
    }
  }

  struct dummy_t
  {
  };
  std::conditional_t<!UseDouble, std::vector<ossia::float_vector>, dummy_t> float_v;
};

template <bool b1, typename... Args>
auto make_vst_fx(Args&... args)
{
  return ossia::make_node<vst_node<b1>>(args...);
}
}
