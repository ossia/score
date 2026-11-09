// The parameter changes the VST3 node hands to the plug-in, and the ones it
// takes back from it, block after block.
//
// The node is made from a fake plug-in, with neither audio nor event buses,
// whose process() records the input parameter changes it was given and plays
// back the output parameter changes the test asks for.

#include <Vst3/Node.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <catch2/catch_test_macros.hpp>

#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <map>
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
  int32 PLUGIN_API getBusCount(MediaType, BusDirection) override { return 0; }
  tresult PLUGIN_API getBusInfo(MediaType, BusDirection, int32, BusInfo&) override
  {
    return kResultFalse;
  }
  tresult PLUGIN_API getRoutingInfo(RoutingInfo&, RoutingInfo&) override
  {
    return kNotImplemented;
  }
  tresult PLUGIN_API activateBus(MediaType, BusDirection, int32, TBool) override
  {
    return kResultOk;
  }
  tresult PLUGIN_API setActive(TBool) override { return kResultOk; }
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
  tresult PLUGIN_API setupProcessing(ProcessSetup&) override { return kResultOk; }
  tresult PLUGIN_API setProcessing(TBool) override { return kResultOk; }
  uint32 PLUGIN_API getTailSamples() override { return 0; }

  tresult PLUGIN_API process(ProcessData& data) override
  {
    auto& block = received.emplace_back();
    if(auto* in = data.inputParameterChanges)
    {
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

  explicit fixture(std::vector<ParamID> ids = {})
  {
    controller.ids = std::move(ids);
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
