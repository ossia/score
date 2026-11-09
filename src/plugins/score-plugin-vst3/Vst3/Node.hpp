#pragma once
#include <Process/Dataflow/TimeSignature.hpp>

#include <Vst3/EffectModel.hpp>
#include <Vst3/MidiEvents.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/editor/scenario/time_signature.hpp>

#include <libremidi/ump_events.hpp>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstmidicontrollers.h>

#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>

#include <algorithm>
#include <atomic>
#include <memory>
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

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  Steinberg::Vst::ParamID PLUGIN_API getParameterId() override { return id; }
  Steinberg::int32 PLUGIN_API getPointCount() override { return data.size(); }
  Steinberg::tresult PLUGIN_API getPoint(
      Steinberg::int32 index, Steinberg::int32& sampleOffset,
      Steinberg::Vst::ParamValue& value) override
  {
    if(!ossia::valid_index(index, data))
      return Steinberg::kInvalidArgument;
    std::tie(sampleOffset, value) = data[index];
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API addPoint(
      Steinberg::int32 sampleOffset, Steinberg::Vst::ParamValue value,
      Steinberg::int32& index) override
  {
    // The plug-in adds points from the audio thread, into room reserved up
    // front: once it is full, a point replaces the last one, which keeps the
    // value the parameter ends the block on.
    if(!data.empty() && data.size() == data.capacity())
    {
      index = data.size() - 1;
      data.back() = {sampleOffset, value};
      return Steinberg::kResultOk;
    }
    index = data.size();
    data.emplace_back(sampleOffset, value);
    return Steinberg::kResultOk;
  }
};

//! The parameter changes given to a plug-in in one process() call: a queue
//! per parameter the host may change, of which the plug-in only sees those
//! with points in the block. A plug-in reads the last point of each queue it
//! is given as the new value of its parameter, so a queue without one would
//! set the parameter back, every block, to whatever the host last sent.
class param_changes final : public Steinberg::Vst::IParameterChanges
{
public:
  std::vector<param_queue> queues;

  //! The queues the plug-in sees, by index in queues
  std::vector<std::size_t> changed;

  //! Its storage is made here, so that collect() does not allocate
  std::size_t add_queue(Steinberg::Vst::ParamID id)
  {
    queues.emplace_back(id);
    changed.reserve(queues.size());
    return queues.size() - 1;
  }

  //! Lists the queues with points, for the process() call to come
  void collect() noexcept
  {
    changed.clear();
    for(std::size_t i = 0; i < queues.size(); i++)
      if(!queues[i].data.empty())
        changed.push_back(i);
  }

  void clear() noexcept
  {
    for(auto& queue : queues)
      queue.data.clear();
    changed.clear();
  }

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  Steinberg::int32 PLUGIN_API getParameterCount() override { return changed.size(); }

  param_queue* PLUGIN_API getParameterData(Steinberg::int32 index) override
  {
    if(!ossia::valid_index(index, changed))
      return nullptr;
    return &queues[changed[index]];
  }

  //! The changes are the host's to write
  param_queue* PLUGIN_API addParameterData(
      const Steinberg::Vst::ParamID& id, Steinberg::int32& index /*out*/) override
  {
    index = -1;
    return nullptr;
  }
};

//! The parameter changes a plug-in gives back from one process() call. The
//! plug-in fills it from the audio thread, so all of its storage is made
//! beforehand, in reserve(), and a block only uses the queues it needs.
class output_param_changes final : public Steinberg::Vst::IParameterChanges
{
public:
  std::vector<param_queue> queues;
  std::size_t count{};

  //! Room for `parameters` parameters of `points` points each in a block
  void reserve(std::size_t parameters, std::size_t points)
  {
    queues.clear();
    queues.reserve(parameters);
    for(std::size_t i = 0; i < parameters; i++)
      queues.emplace_back(Steinberg::Vst::ParamID{}).data.reserve(points);
    count = 0;
  }

  void clear() noexcept
  {
    for(std::size_t i = 0; i < count; i++)
      queues[i].data.clear();
    count = 0;
  }

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  Steinberg::int32 PLUGIN_API getParameterCount() override { return count; }

  param_queue* PLUGIN_API getParameterData(Steinberg::int32 index) override
  {
    if(index < 0 || std::size_t(index) >= count)
      return nullptr;
    return &queues[index];
  }

  param_queue* PLUGIN_API addParameterData(
      const Steinberg::Vst::ParamID& id, Steinberg::int32& index /*out*/) override
  {
    for(std::size_t i = 0; i < count; i++)
    {
      if(queues[i].id == id)
      {
        index = i;
        return &queues[i];
      }
    }

    if(count == queues.size())
    {
      index = -1;
      return nullptr;
    }

    auto& queue = queues[count];
    queue.id = id;
    queue.data.clear();
    index = count++;
    return &queue;
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
      processor->release();
      component->release();
    }

    PluginHandle(const PluginHandle&) = delete;
    PluginHandle(PluginHandle&&) = delete;
    PluginHandle& operator=(const PluginHandle&) = delete;
    PluginHandle& operator=(PluginHandle&&) = delete;

    Steinberg::Vst::IComponent* component{};
    Steinberg::Vst::IAudioProcessor* processor{};
    MIDIControls midi_controls;
  };

  PluginHandle fx;
  //! The largest block the plug-in was set up for, if known
  int m_maxBlock{};
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
      , m_maxBlock{ptr.max_block}
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
    m_sysexStreams.resize(m_totalEventIns);

    // Every parameter the controller knows gets a slot through which the
    // value the processor reports for it reaches the UI, and a queue in which
    // the processor can report it. A few more queues are there for parameters
    // the controller does not list, whose values go nowhere.
    if(auto* ctl = ptr.controller)
    {
      const int N = std::max(0, int(ctl->getParameterCount()));
      m_outputValues = std::make_unique<output_value[]>(N);
      m_outputValueCount = N;
      for(int i = 0; i < N; i++)
      {
        Steinberg::Vst::ParameterInfo info{};
        if(ctl->getParameterInfo(i, info) == Steinberg::kResultOk)
        {
          m_outputValues[i].id = info.id;
          m_outputValueIndex[info.id] = i;
        }
      }
    }
    m_outputChanges.reserve(m_outputValueCount + 8, 4);

    // Give every MIDI-mapped parameter a queue up front.
    //
    // VST3 delivers CC, pitch bend and aftertouch only as parameter changes,
    // and the parameters a plug-in publishes through IMidiMapping are as a rule
    // not the ones exposed as score controls - a JUCE plug-in such as Surge XT
    // answers getMidiControllerAssignment with ids that are not in its
    // parameter list at all. Queues were only ever created by add_control, for
    // exposed controls, so these ids had nowhere to be written and every
    // controller was dropped at the last step even once it had been decoded.
    //
    // Done here rather than on demand because adding a queue allocates, and
    // dispatchMidi runs on the audio thread.
    for(const auto& [key, pid] : fx.midi_controls)
    {
      if(queue_map.find(pid) != queue_map.end())
        continue;
      queue_map[pid] = m_inputChanges.add_queue(pid);
    }
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

  //! `unsent`: the processor was not given v yet, and gets it in the next
  //! block. Its value only ever reaches it through process(): one set on the
  //! edit controller while nothing ran is not known to it.
  std::size_t add_control(
      ossia::value_inlet* inlet, Steinberg::Vst::ParamID id, float v,
      bool unsent = false)
  {
    (**inlet).domain = ossia::domain_base<float>{0.f, 1.f};
    (**inlet).type = ossia::val_type::FLOAT;

    // The parameter may already own a queue because it is MIDI-mapped too.
    // Reuse it, so a controller and the automation port for the same id do not
    // write to two queues the plug-in then sees as conflicting.
    std::size_t queue_idx{};
    if(auto it = queue_map.find(id); it != queue_map.end())
    {
      queue_idx = it->second;
    }
    else
    {
      // FIXME this allocates a lot :[
      queue_idx = this->m_inputChanges.add_queue(id);
      queue_map[id] = queue_idx;
    }

    auto& queue = this->m_inputChanges.queues[queue_idx];
    queue.lastValue = v;
    if(unsent)
    {
      queue.data.clear();
      queue.data.emplace_back(0, v);
    }

    controls.push_back({id, queue_idx, inlet->target<ossia::value_port>()});
    root_inputs().push_back(std::move(inlet));
    return queue_idx;
  }

  //! A control changed from the UI, applied from the execution queue: it
  //! reaches the plug-in in the next block that runs.
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

  void dispatchMidi(int64_t tick_start, int64_t samples)
  {
    m_inputEvents.clear();
    m_sysexArena.clear();
    m_outputEvents.clear();

    int k = 0;
    int audioBusCount = std::ssize(m_audioInputChannels);
    for(int i = audioBusCount; i < audioBusCount + m_totalEventIns; i++)
    {
      dispatchMidi(
          *m_inlets[i]->template target<ossia::midi_port>(), k++, tick_start, samples);
    }
    m_deferred.clear();
    // Empty or not: plug-ins such as Geonkick read the list of a block
    // without checking it is there
    m_vstData.inputEvents = &m_inputEvents;
    m_vstData.outputEvents = &m_outputEvents;
  }

  //! Keep this tick's events for the next block instead of dropping them. The
  //! span covers no whole sample, so every date in the tick maps to the sample
  //! the span starts on, which is offset 0 of the next block that does cover
  //! one: deferring is exact here, not an approximation.
  void stashMidi()
  {
    const int audioBusCount = std::ssize(m_audioInputChannels);
    int k = 0;
    for(int i = audioBusCount; i < audioBusCount + m_totalEventIns; i++, k++)
    {
      for(const libremidi::ump& mess :
          m_inlets[i]->template target<ossia::midi_port>()->messages)
      {
        if(m_deferred.size() >= 1024)
          return;
        m_deferred.push_back({k, mess});
      }
    }
  }

  void dispatchMidi(ossia::midi_port& port, int index, int64_t tick_start, int64_t samples)
  {
    // copy midi data
    auto& ip = port.messages;

    // Port timestamps are relative to the buffer; the plug-in only sees
    // [tick_start; tick_start + samples[ of it and indexes from 0. Anything
    // held back from an empty tick goes first, at offset 0.
    ossia::small_vector<std::pair<const libremidi::ump*, int64_t>, 16> to_send;
    for(const auto& [bus, mess] : m_deferred)
      if(bus == index)
        to_send.push_back({&mess, 0});
    for(const libremidi::ump& mess : ip)
      to_send.push_back(
          {&mess, std::clamp<int64_t>(
               mess.timestamp - tick_start, 0, samples > 0 ? samples - 1 : 0)});

    if(to_send.empty())
      return;

    using VstEvent = Steinberg::Vst::Event;

    // VST3 has no MIDI CC event: controllers reach the plug-in only as
    // parameter changes, for whichever controllers it published through
    // IMidiMapping (collected into fx.midi_controls). Everything that is not a
    // note goes through here.
    const auto push_midi_control = [this, index](
                                       int channel, int controller, double value,
                                       Steinberg::int32 offset) {
      auto it = this->fx.midi_controls.find({index, channel, controller});
      if(it == this->fx.midi_controls.end())
      {
        // A plug-in that maps a controller globally answers for channel 0
        // only; fall back to it so single-channel plug-ins keep working when
        // the sender uses another channel.
        it = this->fx.midi_controls.find({index, 0, controller});
        if(it == this->fx.midi_controls.end())
          return;
      }
      const auto queue_it = this->queue_map.find(it->second);
      if(queue_it == this->queue_map.end())
        return;
      auto& queue = this->m_inputChanges.queues[queue_it->second];
      queue.data.push_back({offset, value});
      queue.lastValue = value;
    };

    // Every field the plug-in may read has a value: the flags, a note's
    // length, which a plug-in may take as when to end it
    VstEvent e{};
    e.busIndex = index;
    e.sampleOffset = 0;
    e.ppqPosition = 0; // FIXME
    for(const auto& [mess_ptr, sample_offset] : to_send)
    {
      const libremidi::ump& mess = *mess_ptr;
      if(Process::MidiSysexArena::isSysex(mess))
      {
        if(auto sysex = toSysexEvent(
               mess, index, Steinberg::int32(sample_offset), m_sysexArena,
               m_sysexStreams[index]))
          m_inputEvents.addEvent(*sysex);
        continue;
      }
      if(mess.get_type() != libremidi::midi2::message_type::MIDI_2_CHANNEL)
        continue;

      e.sampleOffset = sample_offset;
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

        // Control changes were never handled at all, which is what kept every
        // CC out of the plug-ins.
        case libremidi::message_type::CONTROL_CHANGE: {
          const auto cc = libremidi::as_01::control_change(mess);
          push_midi_control(cc.channel, cc.control, cc.value, e.sampleOffset);
          break;
        }

        case libremidi::message_type::PITCH_BEND: {
          const auto pb = libremidi::as_01::pitch_bend(mess);
          push_midi_control(
              pb.channel, Steinberg::Vst::kPitchBend, pb.value, e.sampleOffset);
          break;
        }

        case libremidi::message_type::AFTERTOUCH: {
          const auto at = libremidi::as_01::aftertouch(mess);
          push_midi_control(
              at.channel, Steinberg::Vst::kAfterTouch, at.value, e.sampleOffset);
          break;
        }
        default:
          break;
      }
    }
  }

  //! A parameter change is delivered in the process() call that follows it,
  //! and in that one only: the plug-in must not see it again in the next
  //! blocks. A DPF plug-in turns a change of its program parameter into a
  //! MIDI program change, so a change it kept seeing would reload the program
  //! every block, undoing any program picked in its own editor.
  void clearInputChanges() noexcept { m_inputChanges.clear(); }

  //! Hands the values the plug-in reported for its parameters over to the UI,
  //! the last of each block, and empties the output changes for the next
  //! process() call.
  void readOutputChanges() noexcept
  {
    for(std::size_t i = 0; i < m_outputChanges.count; i++)
    {
      const param_queue& queue = m_outputChanges.queues[i];
      if(queue.data.empty())
        continue;
      auto it = m_outputValueIndex.find(queue.id);
      if(it == m_outputValueIndex.end())
        continue;
      output_value& slot = m_outputValues[it->second];
      slot.value.store(queue.data.back().second, std::memory_order_relaxed);
      slot.changed.store(true, std::memory_order_release);
    }
    m_outputChanges.clear();
  }

  //! Calls f(id, value) on the UI thread for each parameter the plug-in
  //! reported a value for since the previous call, with the latest value.
  template <typename F>
  void consumeOutputValues(F&& f)
  {
    for(std::size_t i = 0; i < m_outputValueCount; i++)
    {
      output_value& slot = m_outputValues[i];
      if(slot.changed.exchange(false, std::memory_order_acquire))
        f(slot.id, slot.value.load(std::memory_order_relaxed));
    }
  }

  void readbackMidi(int64_t tick_start)
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

      // A plug-in may say any bus
      const int bus = e.busIndex;
      if(bus < 0 || bus >= m_totalEventOuts)
        continue;
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
              e.polyPressure.channel, e.polyPressure.pitch, e.polyPressure.pressure);
          break;
        }
        // How a plug-in sends controllers, pitch bend and the like out, with
        // their MIDI 1 values
        case VstEvent::kLegacyMIDICCOutEvent: {
          const auto& cc = e.midiCCOut;
          const uint8_t channel = cc.channel & 0xf;
          const uint8_t value = cc.value & 0x7f;
          const uint8_t value2 = cc.value2 & 0x7f;
          switch(cc.controlNumber)
          {
            case Steinberg::Vst::kAfterTouch:
              mess = libremidi::from_midi1::aftertouch(channel, value);
              break;
            case Steinberg::Vst::kPitchBend:
              mess = libremidi::from_midi1::pitch_bend(channel, value | (value2 << 7));
              break;
            case Steinberg::Vst::kCtrlProgramChange:
              mess = libremidi::from_midi1::program_change(channel, value);
              break;
            case Steinberg::Vst::kCtrlPolyPressure:
              mess = libremidi::from_midi1::poly_pressure(channel, value, value2);
              break;
            default:
              if(cc.controlNumber >= 128)
                continue;
              mess = libremidi::from_midi1::control_change(channel, cc.controlNumber, value);
              break;
          }
          break;
        }
        // Nothing a MIDI port carries
        default:
          continue;
      }

      mess.timestamp = tick_start + e.sampleOffset;
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

    // The playhead at the first sample of this block. m_transport_frames is
    // only updated once a node has run: it is that of the previous tick.
    time_info.projectTimeSamples
        = tk.speed != 0. ? tk.start_date_to_physical(st.modelToSamples())
                         : this->m_transport_frames;

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
  output_param_changes m_outputChanges;

  //! The latest value the plug-in reported for a parameter, from the audio
  //! thread to the UI one.
  struct output_value
  {
    Steinberg::Vst::ParamID id{};
    std::atomic<Steinberg::Vst::ParamValue> value{};
    std::atomic_bool changed{};
  };
  std::unique_ptr<output_value[]> m_outputValues;
  std::size_t m_outputValueCount{};
  ossia::hash_map<Steinberg::Vst::ParamID, std::size_t> m_outputValueIndex;
  Steinberg::Vst::EventList m_inputEvents;
  Steinberg::Vst::EventList m_outputEvents;

  //! Events from ticks that covered no whole sample, with the bus they came in
  //! on, waiting for the next block that does cover one.
  ossia::small_vector<std::pair<int, libremidi::ump>, 8> m_deferred;

  //! The bytes of the system-exclusive messages m_inputEvents points at, kept
  //! until the next block; one stream per event input bus.
  Process::MidiSysexArena m_sysexArena;
  std::vector<Process::MidiSysexArena::stream> m_sysexStreams;
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
    // Panic goes to channel 0's mapping: it is a global request, and a
    // per-channel sweep would write the same parameter sixteen times.
    if(auto it
       = this->fx.midi_controls.find({bus, 0, Steinberg::Vst::kCtrlAllNotesOff});
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

    if(auto it
       = this->fx.midi_controls.find({bus, 0, Steinberg::Vst::kCtrlAllSoundsOff});
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
      for(int k = 0; k < 16; k++)
        for(int i = 0; i <= 127; i++)
        {
          using VstEvent = Steinberg::Vst::Event;
          VstEvent e{};
          e.busIndex = bus;
          e.sampleOffset = 0;
          e.ppqPosition = 0; // FIXME
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
    for(int i = 0; i < m_totalEventIns; i++)
      all_notes_off(i);

    // Run a process cycle, on silence, in the sample size and within the
    // largest block the plug-in was set up for
    {
      using sample_t = std::conditional_t<UseDouble, double, float>;
      const int samples = m_maxBlock > 0 ? std::min(m_maxBlock, 64) : 64;
      Steinberg::Vst::ProcessData dat;
      memcpy(&dat, &m_vstData, sizeof(m_vstData));
      dat.inputEvents = &m_inputEvents;
      dat.numSamples = samples;

      const int channels = std::max(m_totalAudioIns, m_totalAudioOuts);
      auto** buffers = (sample_t**)alloca(sizeof(sample_t*) * std::max(1, channels));
      for(int k = 0; k < channels; k++)
      {
        buffers[k] = (sample_t*)alloca(sizeof(sample_t) * samples);
        std::fill_n(buffers[k], samples, sample_t{});
      }

      const auto set_buffers = [](Steinberg::Vst::AudioBusBuffers& bus, sample_t** b) {
        if constexpr(UseDouble)
          bus.channelBuffers64 = b;
        else
          bus.channelBuffers32 = b;
        bus.silenceFlags = ~0ULL;
      };
      for(std::size_t i = 0, k = 0; i < m_audioInputChannels.size(); i++)
      {
        set_buffers(dat.inputs[i], buffers + k);
        k += m_audioInputChannels[i];
      }
      for(std::size_t i = 0, k = 0; i < m_audioOutputChannels.size(); i++)
      {
        set_buffers(dat.outputs[i], buffers + k);
        k += m_audioOutputChannels[i];
      }

      m_inputChanges.collect();
      fx.processor->process(dat);
      this->clearInputChanges();
      this->readOutputChanges();
    }
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if(!muted() && !tk.paused())
    {
      const auto [tick_start, samples] = st.timings(tk);
      if(samples <= 0)
      {
        // Never call a plug-in with an empty block, but do not lose what the
        // tick carried: it is delivered at the top of the next real one.
        this->stashMidi();
        return;
      }

      this->setControls();
      this->setupTimeInfo(tk, st);

      this->dispatchMidi(tick_start, samples);
      m_inputChanges.collect();

      if constexpr(UseDouble)
      {
        processDouble(tick_start, samples, st.bufferSize());
      }
      else
      {
        processFloat(tick_start, samples, st.bufferSize());
      }

      this->clearInputChanges();
      this->readOutputChanges();
      this->readbackMidi(tick_start);
    }
  }

  void processFloat(int64_t tick_start, std::size_t samples, int bufferSize)
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
            auto& ip = preparePort(port, numChannels, bufferSize);

            Steinberg::Vst::AudioBusBuffers& vst_in = m_vstInput[i];
            vst_in.channelBuffers32 = input + channel_k;
            vst_in.silenceFlags = 0;

            for(int k = 0; k < numChannels; k++)
            {
              const std::size_t avail = ip[k].size() > std::size_t(tick_start)
                                            ? ip[k].size() - std::size_t(tick_start)
                                            : 0u;
              std::copy_n(
                  ip[k].data() + tick_start, std::min(samples, avail),
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
            preparePort(port, numChannels, bufferSize);

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
            std::copy_n(
                float_v[float_k].data(), samples, audio_out.data() + tick_start);
            float_k++;
          }
        }
      }
    }
  }

  void processDouble(int64_t tick_start, std::size_t samples, int bufferSize)
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
          auto& ip = preparePort(port, numChannels, bufferSize);

          Steinberg::Vst::AudioBusBuffers& vst_in = m_vstInput[i];
          vst_in.channelBuffers64 = input + channel_k;
          vst_in.silenceFlags = 0;
          for(int k = 0; k < numChannels; k++)
          {
            input[channel_k++] = ip[k].data() + tick_start;
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
          auto& op = preparePort(port, numChannels, bufferSize);

          Steinberg::Vst::AudioBusBuffers& vst_out = m_vstOutput[i];
          vst_out.channelBuffers64 = output + channel_k;
          vst_out.silenceFlags = 0;
          for(int k = 0; k < numChannels; k++)
          {
            output[channel_k++] = op[k].data() + tick_start;
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
