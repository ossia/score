// The parameter changes the VST3 node hands to the plug-in, and the ones it
// takes back from it, block after block.
//
// The node is made from a fake plug-in, with neither audio nor event buses,
// whose process() records the input parameter changes it was given and plays
// back the output parameter changes the test asks for.

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Vst3/Executor.hpp>
#include <Vst3/Node.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <catch2/catch_test_macros.hpp>

#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <ossia/detail/fmt.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace Steinberg;
using namespace Steinberg::Vst;

struct point
{
  ParamID id{};
  int32 offset{};
  ParamValue value{};
  bool operator==(const point&) const = default;
};

class fake_processor final
    : public IComponent
    , public IAudioProcessor
{
public:
  //! The input points of each process() call
  std::vector<std::vector<point>> received;
  //! The output points to write in the next process() call
  std::vector<point> to_send;
  //! The parameter count the output changes had in each process() call
  std::vector<int32> output_counts;
  //! The event input buses it declares
  int32 event_inputs{};
  //! The event output buses it declares, and the events to write to them in
  //! the next process() call
  int32 event_outputs{};
  std::vector<Event> events_to_send;
  //! Whether each process() call was given a list of input events
  std::vector<bool> had_events;
  //! The values each process() call set its parameters to, read as JUCE's
  //! wrapper reads them: the last point of every queue it is given
  std::vector<std::vector<point>> applied;
  //! The (bus, channel) of the note-offs of each process() call
  std::vector<std::vector<std::pair<int32, int32>>> note_offs;
  //! The project time, in samples, each process() call was given
  std::vector<int64> project_times;
  //! The numSamples of each process() call
  std::vector<int32> block_sizes;
  //! The flags of every note-off it was given
  std::vector<uint16> note_off_flags;
  //! The set-up calls, in order, with whether it was active for each
  std::vector<std::string> calls;
  bool active{};

  tresult PLUGIN_API queryInterface(const TUID, void** obj) override
  {
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API initialize(FUnknown*) override { return kResultOk; }
  tresult PLUGIN_API terminate() override { return kResultOk; }
  tresult PLUGIN_API getControllerClassId(TUID) override { return kNotImplemented; }
  tresult PLUGIN_API setIoMode(IoMode) override { return kResultOk; }
  int32 PLUGIN_API getBusCount(MediaType type, BusDirection dir) override
  {
    if(type != kEvent)
      return 0;
    return dir == kInput ? event_inputs : event_outputs;
  }
  tresult PLUGIN_API
  getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& info) override
  {
    if(type != kEvent || index < 0 || index >= getBusCount(type, dir))
      return kResultFalse;
    info = {};
    info.mediaType = kEvent;
    info.direction = dir;
    info.channelCount = 16;
    info.busType = kMain;
    info.flags = BusInfo::kDefaultActive;
    return kResultOk;
  }
  tresult PLUGIN_API getRoutingInfo(RoutingInfo&, RoutingInfo&) override
  {
    return kNotImplemented;
  }
  tresult PLUGIN_API
  activateBus(MediaType type, BusDirection dir, int32 index, TBool state) override
  {
    calls.push_back(
        fmt::format(
            "activateBus {} {} {} {}{}", type == kAudio ? "audio" : "event",
            dir == kInput ? "in" : "out", index, int(state),
            active ? " while active" : ""));
    return kResultOk;
  }
  tresult PLUGIN_API setActive(TBool state) override
  {
    calls.push_back(fmt::format("setActive {}", int(state)));
    active = state;
    return kResultOk;
  }
  tresult PLUGIN_API setState(IBStream*) override { return kResultOk; }
  tresult PLUGIN_API getState(IBStream*) override { return kResultOk; }

  tresult PLUGIN_API
  setBusArrangements(SpeakerArrangement*, int32, SpeakerArrangement*, int32) override
  {
    return kResultOk;
  }
  tresult PLUGIN_API getBusArrangement(BusDirection, int32, SpeakerArrangement&) override
  {
    return kResultFalse;
  }
  tresult PLUGIN_API canProcessSampleSize(int32) override { return kResultOk; }
  uint32 PLUGIN_API getLatencySamples() override { return 0; }
  tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override
  {
    calls.push_back(
        fmt::format(
            "setupProcessing {} {}{}", setup.sampleRate, setup.maxSamplesPerBlock,
            active ? " while active" : ""));
    return kResultOk;
  }
  tresult PLUGIN_API setProcessing(TBool) override { return kResultOk; }
  uint32 PLUGIN_API getTailSamples() override { return 0; }

  tresult PLUGIN_API process(ProcessData& data) override
  {
    block_sizes.push_back(data.numSamples);
    auto& block = received.emplace_back();
    auto& set = applied.emplace_back();
    had_events.push_back(data.inputEvents != nullptr);
    auto& offs = note_offs.emplace_back();
    project_times.push_back(data.processContext ? data.processContext->projectTimeSamples : -1);
    if(auto* ev = data.inputEvents)
    {
      for(int32 i = 0; i < ev->getEventCount(); i++)
      {
        Event e{};
        if(ev->getEvent(i, e) == kResultOk && e.type == Event::kNoteOffEvent)
        {
          offs.emplace_back(e.busIndex, e.noteOff.channel);
          note_off_flags.push_back(e.flags);
        }
      }
    }
    if(auto* in = data.inputParameterChanges)
    {
      for(int32 i = 0; i < in->getParameterCount(); i++)
      {
        if(auto* q = in->getParameterData(i))
        {
          point p{q->getParameterId()};
          if(q->getPoint(q->getPointCount() - 1, p.offset, p.value) == kResultTrue)
            set.push_back(p);
        }
      }

      for(int32 i = 0; i < in->getParameterCount(); i++)
      {
        auto* q = in->getParameterData(i);
        for(int32 k = 0; k < q->getPointCount(); k++)
        {
          point p{q->getParameterId()};
          q->getPoint(k, p.offset, p.value);
          block.push_back(p);
        }
      }
    }

    if(auto* out = data.outputParameterChanges)
    {
      for(const point& p : to_send)
      {
        int32 index{};
        if(auto* q = out->addParameterData(p.id, index))
        {
          int32 point_index{};
          q->addPoint(p.offset, p.value, point_index);
        }
      }
      output_counts.push_back(out->getParameterCount());
    }
    to_send.clear();
    if(auto* out = data.outputEvents)
      for(Event& e : events_to_send)
        out->addEvent(e);
    events_to_send.clear();
    return kResultOk;
  }
};

//! What the node asks of the edit controller: the parameters there are
class fake_controller final : public IEditController
{
public:
  std::vector<ParamID> ids;

  tresult PLUGIN_API queryInterface(const TUID, void** obj) override
  {
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
  tresult PLUGIN_API initialize(FUnknown*) override { return kResultOk; }
  tresult PLUGIN_API terminate() override { return kResultOk; }

  tresult PLUGIN_API setComponentState(IBStream*) override { return kResultOk; }
  tresult PLUGIN_API setState(IBStream*) override { return kResultOk; }
  tresult PLUGIN_API getState(IBStream*) override { return kResultOk; }
  int32 PLUGIN_API getParameterCount() override { return int32(ids.size()); }
  tresult PLUGIN_API getParameterInfo(int32 index, ParameterInfo& info) override
  {
    if(index < 0 || index >= int32(ids.size()))
      return kInvalidArgument;
    info = {};
    info.id = ids[index];
    return kResultOk;
  }
  tresult PLUGIN_API getParamStringByValue(ParamID, ParamValue, String128) override
  {
    return kNotImplemented;
  }
  tresult PLUGIN_API getParamValueByString(ParamID, TChar*, ParamValue&) override
  {
    return kNotImplemented;
  }
  ParamValue PLUGIN_API normalizedParamToPlain(ParamID, ParamValue v) override { return v; }
  ParamValue PLUGIN_API plainParamToNormalized(ParamID, ParamValue v) override { return v; }
  ParamValue PLUGIN_API getParamNormalized(ParamID) override { return 0.; }
  tresult PLUGIN_API setParamNormalized(ParamID, ParamValue) override { return kResultOk; }
  tresult PLUGIN_API setComponentHandler(IComponentHandler*) override { return kResultOk; }
  IPlugView* PLUGIN_API createView(FIDString) override { return nullptr; }
};

struct fixture
{
  fake_processor fx;
  fake_controller controller;
  ossia::execution_state state;
  std::shared_ptr<vst3::vst_node<false>> node;

  explicit fixture(
      std::vector<ParamID> ids = {}, int32 event_inputs = 0, int32 event_outputs = 0)
  {
    controller.ids = std::move(ids);
    fx.event_inputs = event_inputs;
    fx.event_outputs = event_outputs;
    state.bufferSize = 64;
    state.sampleRate = 44100;
    state.modelToSamplesRatio = 1.;
    state.samplesToModelRatio = 1.;

    vst3::Plugin p;
    p.component = &fx;
    p.processor = &fx;
    p.controller = &controller;
    node = std::make_shared<vst3::vst_node<false>>(p, 44100);
    // Plugin's members are not owned by it: nothing to undo
    p.component = nullptr;
    p.processor = nullptr;
    p.controller = nullptr;
  }

  //! One block, the way the graph runs it: the port data is that of this
  //! tick only
  const std::vector<point>& tick(int frames = 64)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{frames};
    tk.start_sample = 0;
    tk.length_sample = frames;

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node->run(tk, ossia::exec_state_facade{&state});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    for(auto* in : node->root_inputs())
      if(auto* v = in->target<ossia::value_port>())
        v->clear();

    REQUIRE(!fx.received.empty());
    return fx.received.back();
  }
};
}

TEST_CASE(
    "VST3: a control changed from the UI reaches the plug-in in one block only",
    "[vst3][parameters]")
{
  fixture f;
  const auto queue = f.node->add_control(new ossia::value_inlet, 7, 0.5f);

  // Nothing changed yet
  CHECK(f.tick().empty());

  f.node->set_control(queue, 0.25f);
  CHECK(f.tick() == std::vector<point>{{7, 0, 0.25}});
  CHECK(f.tick().empty());
  CHECK(f.tick().empty());

  // A later change is sent once too
  f.node->set_control(queue, 0.75f);
  CHECK(f.tick() == std::vector<point>{{7, 0, 0.75}});
  CHECK(f.tick().empty());
}

TEST_CASE(
    "VST3: a value on a control's port reaches the plug-in in that block only",
    "[vst3][parameters]")
{
  fixture f;
  auto* inlet = new ossia::value_inlet;
  f.node->add_control(inlet, 3, 0.f);

  (*inlet)->write_value(0.5f, 0);
  CHECK(f.tick() == std::vector<point>{{3, 0, 0.5}});
  CHECK(f.tick().empty());

  // Automation writes every tick: every tick carries its value
  for(float v : {0.1f, 0.2f, 0.3f})
  {
    (*inlet)->write_value(v, 0);
    const auto& got = f.tick();
    REQUIRE(got.size() == 1);
    CHECK(got[0].id == 3);
    CHECK(float(got[0].value) == v);
  }
  CHECK(f.tick().empty());
}

TEST_CASE(
    "VST3: a change made in a block that covers no sample waits for the next one",
    "[vst3][parameters]")
{
  fixture f;
  const auto queue = f.node->add_control(new ossia::value_inlet, 1, 0.f);
  f.node->set_control(queue, 0.5f);

  // No process() call for an empty block
  f.tick();
  const auto calls = f.fx.received.size();
  {
    ossia::token_request tk;
    tk.start_sample = 0;
    tk.length_sample = 0;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    f.node->run(tk, ossia::exec_state_facade{&f.state});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  }
  CHECK(f.fx.received.size() == calls);
  CHECK(f.fx.received.back() == std::vector<point>{{1, 0, 0.5}});

  // Set between the last block and an empty one: delivered after the empty one
  f.node->set_control(queue, 0.25f);
  {
    ossia::token_request tk;
    tk.start_sample = 0;
    tk.length_sample = 0;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    f.node->run(tk, ossia::exec_state_facade{&f.state});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  }
  CHECK(f.tick() == std::vector<point>{{1, 0, 0.25}});
  CHECK(f.tick().empty());
}

TEST_CASE(
    "VST3: the output parameter changes of a block do not carry to the next",
    "[vst3][parameters]")
{
  fixture f{{1, 2}};

  f.fx.to_send = {{1, 0, 0.3}};
  f.tick();
  CHECK(f.fx.output_counts.back() == 1);

  // The plug-in starts each block from an empty list
  f.tick();
  CHECK(f.fx.output_counts.back() == 0);

  // A plug-in that reports two parameters every block, as a meter does
  for(int i = 0; i < 1000; i++)
  {
    f.fx.to_send = {{1, 0, i / 1000.}, {2, 3, 1. - i / 1000.}};
    f.tick();
    REQUIRE(f.fx.output_counts.back() == 2);
  }
  CHECK(f.node->m_outputChanges.getParameterCount() == 0);
}

TEST_CASE(
    "VST3: the output parameter changes use the storage made with the node",
    "[vst3][parameters]")
{
  fixture f{{1, 2, 3}};
  auto& out = f.node->m_outputChanges;
  const auto queues = out.queues.size();
  const auto queues_data = out.queues.data();
  std::vector<std::size_t> capacities;
  for(auto& q : out.queues)
    capacities.push_back(q.data.capacity());

  // More points in a block than there is room for, and parameters the
  // controller does not list
  for(int i = 0; i < 100; i++)
  {
    for(int k = 0; k < 50; k++)
      f.fx.to_send.push_back({ParamID(1 + k % 3), k, k / 50.});
    for(ParamID unlisted = 100; unlisted < 120; unlisted++)
      f.fx.to_send.push_back({unlisted, 0, 0.5});
    f.tick();
  }

  CHECK(out.queues.size() == queues);
  CHECK(out.queues.data() == queues_data);
  for(std::size_t i = 0; i < out.queues.size(); i++)
    CHECK(out.queues[i].data.capacity() == capacities[i]);
}

TEST_CASE(
    "VST3: the UI gets the last value the plug-in reported for a parameter, once",
    "[vst3][parameters]")
{
  fixture f{{1, 2}};
  std::map<ParamID, ParamValue> got;
  const auto consume = [&] {
    got.clear();
    f.node->consumeOutputValues([&](ParamID id, ParamValue v) { got[id] = v; });
    return got;
  };

  CHECK(consume().empty());

  f.fx.to_send = {{1, 0, 0.1}, {1, 10, 0.2}};
  f.tick();
  f.fx.to_send = {{2, 0, 0.7}};
  f.tick();
  f.fx.to_send = {{1, 5, 0.4}, {42, 0, 1.}};
  f.tick();

  CHECK(consume() == std::map<ParamID, ParamValue>{{1, 0.4}, {2, 0.7}});
  CHECK(consume().empty());
}

TEST_CASE(
    "VST3: a control changed on the UI reaches the node through the execution "
    "queue",
    "[vst3][parameters][threads]")
{
  // The UI thread only enqueues; the node is touched by what the audio
  // thread dequeues, between two ticks, as the engine does before a tick.
  fixture f;
  const auto queue_idx = f.node->add_control(new ossia::value_inlet, 5, 0.f);

  Execution::ExecutionCommandQueue queue{64};
  QObject context;
  Process::ControlInlet ctrl{"Control", Id<Process::Port>{0}, &context};
  vst3::connectControl(ctrl, &context, queue, f.node, queue_idx);

  const auto run_queue = [&] {
    int n = 0;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    Execution::ExecutionCommand cmd;
    while(queue.try_dequeue(cmd))
    {
      cmd();
      n++;
    }
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    return n;
  };

  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  ctrl.setValue(0.25f);

  // Nothing reached the node from the UI thread
  CHECK(f.node->m_inputChanges.queues[queue_idx].data.empty());
  CHECK(f.tick().empty());

  CHECK(run_queue() == 1);
  CHECK(f.tick() == std::vector<point>{{5, 0, 0.25}});
  CHECK(f.tick().empty());

  // A value the processor reported is shown on the control: the document
  // keeps the control's value, and nothing goes back to the processor
  ossia::value shown;
  QObject::connect(
      &ctrl, &Process::ControlInlet::executionValueChanged, &context,
      [&](const ossia::value& v) { shown = v; });
  ctrl.setExecutionValue(0.5f);
  CHECK(shown == ossia::value{0.5f});
  CHECK(ctrl.value() == ossia::value{0.25f});
  CHECK(run_queue() == 0);
  CHECK(f.tick().empty());
}

TEST_CASE(
    "VST3: a plug-in with an event input is given a list of events in every block",
    "[vst3][midi]")
{
  // Plug-ins such as Geonkick read the list without checking it is there,
  // including in blocks with no events
  fixture f{{}, 1};
  for(int i = 0; i < 4; i++)
    f.tick();
  REQUIRE(f.fx.had_events.size() == 4);
  for(bool had : f.fx.had_events)
    CHECK(had);
}

TEST_CASE(
    "VST3: a parameter the host does not change is not set again", "[vst3][parameters]")
{
  fixture f;
  const auto queue = f.node->add_control(new ossia::value_inlet, 7, 0.5f);
  f.node->add_control(new ossia::value_inlet, 8, 0.5f);

  f.tick();
  CHECK(f.fx.applied.back().empty());

  f.node->set_control(queue, 0.25f);
  f.tick();
  CHECK(f.fx.applied.back() == std::vector<point>{{7, 0, 0.25}});

  // The plug-in may have changed it since, from its own editor or its
  // processor: the value the host sent last must not undo that
  f.tick();
  CHECK(f.fx.applied.back().empty());
}

TEST_CASE(
    "VST3: a stop sends notes off on every event input, on valid channels",
    "[vst3][midi]")
{
  fixture f{{}, 3};
  f.node->all_notes_off();
  REQUIRE(f.fx.note_offs.size() == 1);
  std::set<int32> buses;
  for(auto [bus, channel] : f.fx.note_offs.back())
  {
    buses.insert(bus);
    CHECK(channel >= 0);
    CHECK(channel < 16);
  }
  CHECK(buses == std::set<int32>{0, 1, 2});
  // Nothing but a note-off: no stray flags
  REQUIRE(!f.fx.note_off_flags.empty());
  for(auto flags : f.fx.note_off_flags)
    CHECK(flags == 0);
}

TEST_CASE(
    "VST3: a controller a plug-in sends out reaches its MIDI output", "[vst3][midi]")
{
  fixture f{{}, 0, 1};
  Event cc{};
  cc.busIndex = 0;
  cc.sampleOffset = 3;
  cc.type = Event::kLegacyMIDICCOutEvent;
  cc.midiCCOut.controlNumber = 7;
  cc.midiCCOut.channel = 2;
  cc.midiCCOut.value = 100;
  Event bend = cc;
  bend.midiCCOut.controlNumber = kPitchBend;
  bend.midiCCOut.value = 0;
  bend.midiCCOut.value2 = 0x40;
  // Nothing a MIDI port carries
  Event data{};
  data.type = Event::kDataEvent;
  f.fx.events_to_send = {cc, bend, data};
  f.tick();

  auto& out = f.node->root_outputs()[0]->cast<ossia::midi_port>();
  REQUIRE(out.messages.size() == 2);
  const auto& m = out.messages[0];
  CHECK(m.get_type() == libremidi::midi2::message_type::MIDI_2_CHANNEL);
  CHECK(libremidi::message_type(m.get_status_code()) == libremidi::message_type::CONTROL_CHANGE);
  CHECK(m.get_channel_0_15() == 2);
  CHECK(libremidi::as_midi1::control_change(m).control == 7);
  CHECK(libremidi::as_midi1::control_change(m).value == 100);
  CHECK(m.timestamp == 3);
  CHECK(libremidi::message_type(out.messages[1].get_status_code()) == libremidi::message_type::PITCH_BEND);
  CHECK(libremidi::as_midi1::pitch_bend(out.messages[1]).value == 0x2000);
}

TEST_CASE(
    "VST3: an event a plug-in sends to a bus it does not have goes nowhere",
    "[vst3][midi]")
{
  fixture f{{}, 0, 1};
  Event e{};
  e.busIndex = 3;
  e.type = Event::kNoteOnEvent;
  e.noteOn.pitch = 60;
  e.noteOn.velocity = 1.f;
  f.fx.events_to_send = {e};
  f.tick();
  CHECK(f.node->root_outputs()[0]->cast<ossia::midi_port>().messages.empty());
}

TEST_CASE(
    "VST3: a block is told the position of its own first sample", "[vst3][transport]")
{
  fixture f;
  // As the graph runs a node: the time it covered is accounted for after it ran
  for(int64_t start : {0, 64, 128, 1000})
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{start};
    tk.date = ossia::time_value{start + 64};
    tk.start_sample = 0;
    tk.length_sample = 64;
    tk.speed = 1.;
    f.node->run(tk, ossia::exec_state_facade{&f.state});
    f.node->process_time(tk, f.state);
  }
  CHECK(f.fx.project_times == std::vector<int64>{0, 64, 128, 1000});
}

TEST_CASE(
    "VST3: a value the processor was not given reaches it in the first block only",
    "[vst3][parameters]")
{
  // Set on the edit controller while nothing ran: from score's own control,
  // or from the plug-in's editor
  fixture f;
  f.node->add_control(new ossia::value_inlet, 7, 0.5f, true);
  f.node->add_control(new ossia::value_inlet, 8, 0.25f);

  CHECK(f.tick() == std::vector<point>{{7, 0, 0.5}});
  CHECK(f.fx.applied.back() == std::vector<point>{{7, 0, 0.5}});
  CHECK(f.tick().empty());
  CHECK(f.tick().empty());
}

TEST_CASE(
    "VST3: a MIDI-mapped parameter the host does not change is not set again",
    "[vst3][parameters]")
{
  // Uhhyou's synths map controllers to parameters of their own: a value they
  // were given for those every block, 0, left them silent
  fixture f;
  vst3::Plugin p;
  p.component = &f.fx;
  p.processor = &f.fx;
  p.controller = &f.controller;
  p.midiControls[{0, 0, 1}] = 100;
  p.midiControls[{0, 0, Steinberg::Vst::kPitchBend}] = 101;
  f.fx.event_inputs = 1;
  auto node = std::make_shared<vst3::vst_node<false>>(p, 44100);
  p.component = nullptr;
  p.processor = nullptr;
  p.controller = nullptr;
  f.node = node;
  f.node->add_control(new ossia::value_inlet, 7, 0.5f);

  for(int i = 0; i < 3; i++)
  {
    CHECK(f.tick().empty());
    CHECK(f.fx.applied.back().empty());
  }
}

namespace
{
//! A plug-in as Plugin::load leaves it, before start()
struct loaded_plugin
{
  fake_processor fx;
  vst3::Plugin p;
  explicit loaded_plugin(int32 event_inputs = 0, int32 event_outputs = 0)
  {
    fx.event_inputs = event_inputs;
    fx.event_outputs = event_outputs;
    p.component = &fx;
    p.processor = &fx;
  }
  ~loaded_plugin()
  {
    p.component = nullptr;
    p.processor = nullptr;
  }
};
}

TEST_CASE(
    "VST3: a plug-in is set up, then its buses switched on, then it is activated",
    "[vst3][setup]")
{
  loaded_plugin l{1, 1};
  l.p.start(44100, 512);
  CHECK(
      l.fx.calls
      == std::vector<std::string>{
          "setupProcessing 44100 512", "activateBus event in 0 1",
          "activateBus event out 0 1", "setActive 1"});
  CHECK(l.p.sample_rate == 44100);
  CHECK(l.p.max_block == 512);
}

TEST_CASE(
    "VST3: a plug-in is set up again, inactive, only for another rate or block size",
    "[vst3][setup]")
{
  loaded_plugin l;
  l.p.start(44100, 512);
  l.fx.calls.clear();

  // What it was set up for: nothing to do, activating can be costly
  l.p.restart(44100, 512);
  CHECK(l.fx.calls.empty());

  // setupProcessing() only while inactive
  l.p.restart(48000, 256);
  CHECK(
      l.fx.calls
      == std::vector<std::string>{
          "setActive 0", "setupProcessing 48000 256", "setActive 1"});
  CHECK(l.fx.active);
  l.fx.calls.clear();

  l.p.restart(48000, 256);
  CHECK(l.fx.calls.empty());

  l.p.restart(48000, 1024);
  CHECK(
      l.fx.calls
      == std::vector<std::string>{
          "setActive 0", "setupProcessing 48000 1024", "setActive 1"});
  l.fx.calls.clear();

  // Nothing to set it up for
  l.p.restart(0, 512);
  l.p.restart(48000, 0);
  CHECK(l.fx.calls.empty());
}

TEST_CASE(
    "VST3: the notes off of a stop fit in the largest block the plug-in was set "
    "up for",
    "[vst3][midi][setup]")
{
  for(int max_block : {16, 32, 64, 512})
  {
    fake_processor fx;
    fx.event_inputs = 1;
    vst3::Plugin p;
    p.component = &fx;
    p.processor = &fx;
    p.start(44100, max_block);
    {
      vst3::vst_node<false> node{p, 44100};
      node.all_notes_off();
    }
    p.component = nullptr;
    p.processor = nullptr;
    REQUIRE(fx.block_sizes.size() == 1);
    CHECK(fx.block_sizes[0] > 0);
    CHECK(fx.block_sizes[0] <= max_block);
  }
}
