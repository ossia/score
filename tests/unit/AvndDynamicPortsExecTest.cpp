// Dynamic ports of avendish processes, on the execution side.
//
// When the model adds or removes ports, the exec node (oscr::safe_node) is not
// replaced: Executor::recompute_ports() rebuilds its port list in place,
// through a transaction on the execution queue. What has to hold afterwards:
//  * the exec node has exactly the model's ports, in the model's order,
//    including the ports that do not come from a field of the object;
//  * every model port is registered against its exec port in the SetupContext,
//    and the ports that went away are not in there anymore;
//  * the cables of the process are re-made against the new exec ports;
//  * a value set on a control that appeared reaches the object.
//
// The execution queue is drained by hand (runAllCommands) so that the tests
// are deterministic: nothing plays here.

#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/detail/thread.hpp>

#include <boost/container/small_vector.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Scenario/Commands/SetControllerControlValue.hpp>
#include <catch2/catch_test_macros.hpp>
#include <examples/Advanced/Utilities/Demux.hpp>
#include <examples/Advanced/Utilities/Mux.hpp>
#include <examples/Advanced/Utilities/ValueMixer.hpp>
#include <halp/messages.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString mux_uuid = QStringLiteral("deb6c556-afda-47c2-b545-d74c8e3958e4");
const QString value_mixer_uuid = QStringLiteral("0fd108dd-daa8-4667-869d-f409da70e823");
// ao::Demux: inlets = [Output count, Current index, Input], outlets = [Output 0..N-1]
const QString demux_uuid = QStringLiteral("37ba8700-a924-4364-b761-a14cc036cef3");
// ao::AudioSplitter: inlets = [Input (audio), Channels], outlets = [Channel 0..N-1] (audio)
const QString audio_splitter_uuid = QStringLiteral("42db08e5-625e-40d5-8f90-aed74cc34661");

Process::ControlInlet* controller(Process::ProcessModel& p)
{
  auto* c = qobject_cast<Process::ControlInlet*>(p.inlets()[0]);
  REQUIRE(c);
  return c;
}

Process::ControlInlet* control(Process::ProcessModel& p, int i)
{
  auto* c = qobject_cast<Process::ControlInlet*>(p.inlets()[i]);
  REQUIRE(c);
  return c;
}

void setController(score::Document& doc, Process::ProcessModel& p, int value)
{
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::SetControllerControlValue>(*controller(p), value, doc.context());
}

Process::Cable* makeCable(
    score::Document& doc, int id, const Process::Port& source, const Process::Port& sink)
{
  auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
  CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
      model, Id<Process::Cable>{id}, Process::CableType::ImmediateGlutton, source, sink);
  auto it = model.cables.find(Id<Process::Cable>{id});
  REQUIRE(it != model.cables.end());
  return &*it;
}

//! Runs what the exec thread would run. The commands check that they run in
//! an audio thread, so the main thread poses as one for the duration.
void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

void spin(int ms = 0)
{
  QApplication::processEvents();
  if(ms > 0)
  {
    QElapsedTimer t;
    t.start();
    while(t.elapsed() < ms)
      QApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  QApplication::processEvents();
}

Execution::DocumentPlugin& load_execution(score::Document& doc)
{
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  run_exec(plug);
  return plug;
}

std::shared_ptr<Execution::ProcessComponent>
component(Execution::DocumentPlugin& plug, const Process::ProcessModel& proc)
{
  REQUIRE(plug.baseScenario());
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(proc.id());
  REQUIRE(it != procs.end());
  REQUIRE(it->second);
  REQUIRE(it->second->node);
  return it->second;
}

//! The exec node has the model's ports, one for one, and the setup context
//! maps every model port to that exec port.
void check_registered(
    Execution::SetupContext& setup, const Process::ProcessModel& proc,
    const ossia::graph_node& node)
{
  REQUIRE(node.root_inputs().size() == proc.inlets().size());
  REQUIRE(node.root_outputs().size() == proc.outlets().size());

  for(std::size_t i = 0; i < proc.inlets().size(); i++)
  {
    auto* inlet = proc.inlets()[i];
    auto it = setup.inlets.find(inlet);
    REQUIRE(it != setup.inlets.end());
    CHECK(it->second.first.get() == &node);
    CHECK(it->second.second == node.root_inputs()[i]);
    CHECK(node.root_inputs()[i] != nullptr);
  }
  for(std::size_t i = 0; i < proc.outlets().size(); i++)
  {
    auto* outlet = proc.outlets()[i];
    auto it = setup.outlets.find(outlet);
    REQUIRE(it != setup.outlets.end());
    CHECK(it->second.first.get() == &node);
    CHECK(it->second.second == node.root_outputs()[i]);
    CHECK(node.root_outputs()[i] != nullptr);
  }

  // No two model ports share an exec port
  std::set<const void*> seen;
  for(auto* p : node.root_inputs())
    CHECK(seen.insert(p).second);
  for(auto* p : node.root_outputs())
    CHECK(seen.insert(p).second);

  auto pm = setup.proc_map.find(&node);
  REQUIRE(pm != setup.proc_map.end());
  CHECK(pm->second == &proc);
}

//! A test object with a message inlet: its exec node has a port that does
//! not come from a field of the object, which must stay first through resizes.
struct DynWithMessage
{
  halp_meta(name, "Dynamic ports with message")
  halp_meta(c_name, "score_test_dyn_with_message")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "Dynamic ports next to a message inlet")
  halp_meta(uuid, "3f1d7a52-6c1e-4d0b-9c58-7b2c1e4f0a91")

  struct ins
  {
    struct : halp::spinbox_i32<"Count", halp::range{0, 16, 1}>
    {
      static std::function<void(DynWithMessage&, int)> on_controller_interaction()
      {
        return [](DynWithMessage& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    halp::dynamic_port<halp::knob_f32<"In {}">> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Out", float> out;
  } outputs;

  void bang() { bangs++; }
  struct messages
  {
    using parent_type = DynWithMessage;
    halp::func_ref<"Bang", &DynWithMessage::bang> m;
  };

  int bangs{};
  void operator()() { }
};

//! A per-sample (cv) processor with dynamic ports: its first model port is the
//! sample input, which is not a field of the object. The ports of a group must
//! be added and removed at the right place after it.
struct CvDyn
{
  halp_meta(name, "CV with dynamic ports")
  halp_meta(c_name, "score_test_cv_dyn")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "Dynamic ports on a cv processor")
  halp_meta(uuid, "5b7e2c91-3a4d-4e8f-b0c1-9d2e3f4a5b6c")
  halp_flag(cv);

  struct ins
  {
    struct : halp::spinbox_i32<"Count", halp::range{0, 16, 1}>
    {
      static std::function<void(CvDyn&, int)> on_controller_interaction()
      {
        return [](CvDyn& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    halp::dynamic_port<halp::knob_f32<"In {}">> in_i;
  } inputs;

  float operator()(float v) noexcept
  {
    for(auto& p : inputs.in_i.ports)
      v += p.value;
    return v;
  }
};

//! A control with an on_controller_interaction hook that is not a spinbox or
//! a line edit: it links two controls, the way puara's Smoother does. The
//! model keeps an instance to run the hook, but the port is a plain control,
//! and there is no port factory to register for it.
struct LinkedControls
{
  halp_meta(name, "Linked controls")
  halp_meta(c_name, "score_test_linked_controls")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "A controller that is a toggle")
  halp_meta(uuid, "8c2f0b6e-1d4a-4f7e-9b3c-2e5d6a7f8b90")

  struct ins
  {
    struct : halp::toggle<"Link">
    {
      static std::function<void(LinkedControls&, bool)> on_controller_interaction()
      {
        return [](LinkedControls& object, bool value) { object.linked = value; };
      }
    } link;
    halp::knob_f32<"Gain"> gain;
  } inputs;

  struct
  {
    halp::val_port<"Out", float> out;
  } outputs;

  bool linked{};
  void operator()() { }
};
}

TEST_CASE("Only the controllers with a port type of their own get a port factory", "[avnd][dynamic-ports][factories]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto key = Process::PortFactory::static_interfaceKey();

    // ao::Mux: one int spinbox controller
    {
      std::vector<score::InterfaceBase*> v;
      oscr::instantiate_fx<ao::Mux>(v, ctx, key);
      CHECK(v.size() == 1);
      for(auto* f : v)
        delete f;
    }

    // A toggle controller: nothing to register, and it must not be a hard error
    {
      std::vector<score::InterfaceBase*> v;
      oscr::instantiate_fx<LinkedControls>(v, ctx, key);
      CHECK(v.empty());
    }

    // The hook still runs on the model side
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = score::test::base_interval(*doc);
    auto* proc = new oscr::ProcessModel<LinkedControls>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7778}, doc->context(), &itv};
    REQUIRE(proc->inlets().size() == 2);
    auto* link = qobject_cast<Process::ControlInlet*>(proc->inlets()[0]);
    REQUIRE(link);
    CHECK(link->concreteKey() == Process::Toggle::static_concreteKey());
    CHECK(!static_cast<LinkedControls&>(proc->object_storage_for_ports_callbacks).linked);
    link->setValue(true);
    CHECK(static_cast<LinkedControls&>(proc->object_storage_for_ports_callbacks).linked);
    delete proc;
  });
}

TEST_CASE("The exec node follows the model's ports through resizes", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mixer || !mux)
      SKIP("ao::ValueMixer / ao::Mux are not built");

    // mux.Output -> mixer.Input 1
    auto* cable = makeCable(*doc, 1, *mux->outlets()[0], *mixer->inlets()[3]);
    const auto cable_id = cable->id();

    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;

    auto comp = component(plug, *mixer);
    auto node = comp->node;
    check_registered(setup, *mixer, *node);
    check_registered(setup, *mux, *component(plug, *mux)->node);

    auto edge_of = [&](const Id<Process::Cable>& id) {
      auto it = setup.m_cables.find(id);
      REQUIRE(it != setup.m_cables.end());
      REQUIRE(it->second);
      return it->second;
    };

    // The cable's edge is wired between the two exec ports
    {
      auto edge = edge_of(cable_id);
      CHECK(edge->in == node->root_inputs()[3]);
      CHECK(edge->out == component(plug, *mux)->node->root_outputs()[0]);
      CHECK(ossia::contains(edge->in->sources, edge.get()));
    }

    SECTION("growing")
    {
      const auto old_edge = edge_of(cable_id);
      setController(*doc, *mixer, 4);
      run_exec(plug);
      spin();

      REQUIRE(mixer->inlets().size() == 18);
      CHECK(comp->node == node); // same node, new ports
      check_registered(setup, *mixer, *node);

      // The cable is on the new exec port of Input 1
      auto edge = edge_of(cable_id);
      CHECK(edge != old_edge);
      CHECK(edge->in == node->root_inputs()[3]);
      CHECK(ossia::contains(edge->in->sources, edge.get()));
      CHECK(edge->out == component(plug, *mux)->node->root_outputs()[0]);
    }

    SECTION("shrinking forgets the removed ports")
    {
      setController(*doc, *mixer, 4);
      run_exec(plug);
      spin();
      REQUIRE(mixer->inlets().size() == 18);

      // The model ports that are about to be deleted: their addresses must
      // not stay behind as keys of the setup maps, or the next port allocated
      // at one of those addresses would be taken for them.
      std::vector<Process::Inlet*> doomed{
          mixer->inlets()[5], mixer->inlets()[9], mixer->inlets()[13],
          mixer->inlets()[17]};
      for(auto* p : doomed)
        REQUIRE(setup.inlets.find(p) != setup.inlets.end());

      setController(*doc, *mixer, 1);
      run_exec(plug);
      spin();

      REQUIRE(mixer->inlets().size() == 6);
      check_registered(setup, *mixer, *node);
      for(auto* p : doomed)
        CHECK(setup.inlets.find(p) == setup.inlets.end());

      // Input 1 went away, and the cable with it
      CHECK(score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc).cables.empty());

      // And back: the cable comes back on the re-created port
      doc->commandStack().undo();
      run_exec(plug);
      spin();
      REQUIRE(mixer->inlets().size() == 18);
      check_registered(setup, *mixer, *node);
      auto edge = edge_of(cable_id);
      CHECK(edge->in == node->root_inputs()[3]);
      CHECK(ossia::contains(edge->in->sources, edge.get()));
    }

    SECTION("a resize of the other end of a cable")
    {
      // mixer.Output -> mux.Input 1, then mux grows
      auto* cable2 = makeCable(*doc, 2, *mixer->outlets()[0], *mux->inlets()[3]);
      run_exec(plug);
      spin();
      auto mux_node = component(plug, *mux)->node;
      {
        auto edge = edge_of(cable2->id());
        CHECK(edge->in == mux_node->root_inputs()[3]);
        CHECK(edge->out == node->root_outputs()[0]);
      }

      setController(*doc, *mux, 6);
      run_exec(plug);
      spin();
      check_registered(setup, *mux, *mux_node);
      check_registered(setup, *mixer, *node);
      auto edge = edge_of(cable2->id());
      CHECK(edge->in == mux_node->root_inputs()[3]);
      CHECK(edge->out == node->root_outputs()[0]);
      CHECK(ossia::contains(edge->in->sources, edge.get()));
      CHECK(ossia::contains(edge->out->targets, edge.get()));
    }

    // Let the freed ports and the stop go through before the document closes
    spin(50);
  });
}

TEST_CASE("A value set on a control that appeared reaches the object", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    if(!mixer)
      SKIP("ao::ValueMixer is not built");

    // A value on a port that was there from the start
    control(*mixer, 4)->setValue(0.5f); // Mix 0

    auto& plug = load_execution(*doc);
    auto comp = component(plug, *mixer);
    auto& n = static_cast<oscr::safe_node<ao::ValueMixer>&>(*comp->node);
    auto& obj = n.impl.effect;

    REQUIRE(obj.inputs.mix_i.ports.size() == 2);
    CHECK(obj.inputs.mix_i.ports[0].value == 0.5f);

    setController(*doc, *mixer, 3);
    run_exec(plug);
    spin();
    REQUIRE(mixer->inlets().size() == 14);

    // The object's own port vectors are sized by the reload itself: a value
    // may come in before the first tick would have resized them.
    REQUIRE(obj.inputs.in_i.ports.size() == 3);
    REQUIRE(obj.inputs.mix_i.ports.size() == 3);
    REQUIRE(obj.inputs.solo_i.ports.size() == 3);
    REQUIRE(obj.inputs.mute_i.ports.size() == 3);
    // The port that was there kept its value
    CHECK(obj.inputs.mix_i.ports[0].value == 0.5f);

    // Mix 2 and Mute 2 are new: the UI -> exec connection is made on the
    // fly. Mute is the fourth dynamic port group, past the number of
    // controls of the object -- a spot that used to overflow a bitset.
    control(*mixer, 7)->setValue(0.25f);
    control(*mixer, 13)->setValue(true);
    run_exec(plug);
    CHECK(obj.inputs.mix_i.ports[2].value == 0.25f);
    CHECK(obj.inputs.mute_i.ports[2].value == true);

    // The one that was there still works too
    control(*mixer, 5)->setValue(0.125f);
    run_exec(plug);
    CHECK(obj.inputs.mix_i.ports[0].value == 0.125f);

    // The controller's own value goes through the normal control path
    CHECK(obj.inputs.controller.value == 3);

    // Shrink: the vectors follow, and a stale update for a removed port is
    // dropped rather than written past the end
    auto* mix2 = control(*mixer, 7);
    controller(*mixer)->setValue(1);
    // (mix2 is deleted here: the update below was queued from the UI before
    //  the shrink reached the exec side)
    (void)mix2;
    run_exec(plug);
    spin();
    REQUIRE(obj.inputs.mix_i.ports.size() == 1);
    CHECK(obj.inputs.mix_i.ports[0].value == 0.125f);

    spin(50);
  });
}

TEST_CASE("Ports that are not fields of the object keep their place", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    // Something for the base interval to execute, and the exec context
    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mux)
      SKIP("ao::Mux is not built");
    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;

    // The test object is not registered as a process factory: build its model
    // and its executor by hand, the way the interval component would.
    auto& itv = score::test::base_interval(*doc);
    auto* proc = new oscr::ProcessModel<DynWithMessage>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7777}, dctx, &itv};
    // message inlet, controller, In 0
    REQUIRE(proc->inlets().size() == 3);
    CHECK(proc->inlets()[0]->name() == "Bang");
    CHECK(proc->inlets()[1]->name() == "Count");
    CHECK(proc->inlets()[2]->name() == "In 0");

    auto comp = std::make_shared<oscr::Executor<DynWithMessage>>(*proc, plug.context(), nullptr);
    REQUIRE(comp->node);
    setup.register_node(*proc, comp->node);
    run_exec(plug);

    auto& n = static_cast<oscr::safe_node<DynWithMessage>&>(*comp->node);
    check_registered(setup, *proc, n);
    CHECK(n.root_inputs()[0] == &n.message_ports.message_inlets[0]);

    control(*proc, 1)->setValue(3);
    run_exec(plug);
    spin();
    REQUIRE(proc->inlets().size() == 5);
    check_registered(setup, *proc, n);
    // The message inlet is still the first exec port, the knobs come after
    CHECK(n.root_inputs()[0] == &n.message_ports.message_inlets[0]);
    CHECK(n.impl.effect.inputs.in_i.ports.size() == 3);

    control(*proc, 4)->setValue(0.75f);
    run_exec(plug);
    CHECK(n.impl.effect.inputs.in_i.ports[2].value == 0.75f);

    control(*proc, 1)->setValue(0);
    run_exec(plug);
    spin();
    REQUIRE(proc->inlets().size() == 2);
    check_registered(setup, *proc, n);
    CHECK(n.root_inputs()[0] == &n.message_ports.message_inlets[0]);
    CHECK(n.impl.effect.inputs.in_i.ports.empty());

    comp->cleanup();
    run_exec(plug);
    comp.reset();
    delete proc;
    spin(50);
  });
}

TEST_CASE("A cable on a port that moves follows it to its new exec port", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mixer || !mux)
      SKIP("ao::ValueMixer / ao::Mux are not built");

    // mux.Output -> mixer.Mix 0, which is at index 4 with two inputs and at
    // index 5 with three
    auto* mix0 = mixer->inlets()[4];
    REQUIRE(mix0->name() == "Mix 0");
    auto* cable = makeCable(*doc, 1, *mux->outlets()[0], *mix0);
    const auto cable_id = cable->id();

    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;
    auto node = component(plug, *mixer)->node;
    auto mux_node = component(plug, *mux)->node;
    auto& obj = static_cast<oscr::safe_node<ao::ValueMixer>&>(*node).impl.effect;

    auto edge_of = [&](const Id<Process::Cable>& id) {
      auto it = setup.m_cables.find(id);
      REQUIRE(it != setup.m_cables.end());
      REQUIRE(it->second);
      return it->second;
    };
    CHECK(edge_of(cable_id)->in == node->root_inputs()[4]);

    setController(*doc, *mixer, 3);
    run_exec(plug);
    spin();

    REQUIRE(mixer->inlets().size() == 14);
    REQUIRE(mixer->inlets()[5] == mix0);
    check_registered(setup, *mixer, *node);
    REQUIRE(score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc).cables.size() == 1);
    auto edge = edge_of(cable_id);
    CHECK(edge->in == node->root_inputs()[5]);
    CHECK(edge->out == mux_node->root_outputs()[0]);
    CHECK(ossia::contains(edge->in->sources, edge.get()));
    CHECK(ossia::contains(edge->out->targets, edge.get()));

    // The UI -> exec connection of Mix 0 was made when it was at index 4:
    // it still addresses the first port of its group
    control(*mixer, 5)->setValue(0.3f);
    run_exec(plug);
    REQUIRE(obj.inputs.mix_i.ports.size() == 3);
    CHECK(obj.inputs.mix_i.ports[0].value == 0.3f);

    doc->commandStack().undo();
    run_exec(plug);
    spin();
    REQUIRE(mixer->inlets().size() == 10);
    check_registered(setup, *mixer, *node);
    edge = edge_of(cable_id);
    CHECK(edge->in == node->root_inputs()[4]);
    CHECK(ossia::contains(edge->in->sources, edge.get()));

    spin(50);
  });
}

TEST_CASE("Undoing a shrink gives the re-created ports their value back on the exec side", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    if(!mixer)
      SKIP("ao::ValueMixer is not built");

    setController(*doc, *mixer, 3);
    REQUIRE(mixer->inlets().size() == 14);
    control(*mixer, 7)->setValue(0.75f);  // Mix 2
    control(*mixer, 13)->setValue(true); // Mute 2

    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;
    auto node = component(plug, *mixer)->node;
    auto& obj = static_cast<oscr::safe_node<ao::ValueMixer>&>(*node).impl.effect;
    REQUIRE(obj.inputs.mix_i.ports.size() == 3);
    CHECK(obj.inputs.mix_i.ports[2].value == 0.75f);
    CHECK(obj.inputs.mute_i.ports[2].value == true);

    setController(*doc, *mixer, 1);
    run_exec(plug);
    spin();
    REQUIRE(obj.inputs.mix_i.ports.size() == 1);

    doc->commandStack().undo();
    run_exec(plug);
    spin();
    REQUIRE(mixer->inlets().size() == 14);
    check_registered(setup, *mixer, *node);
    REQUIRE(obj.inputs.mix_i.ports.size() == 3);
    CHECK(obj.inputs.mix_i.ports[2].value == 0.75f);
    CHECK(obj.inputs.mute_i.ports[2].value == true);

    // The re-created port is wired up: an edit reaches the object
    control(*mixer, 7)->setValue(0.25f);
    run_exec(plug);
    CHECK(obj.inputs.mix_i.ports[2].value == 0.25f);

    spin(50);
  });
}

TEST_CASE("Dynamic ports of a cv processor come after its sample input", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mux)
      SKIP("ao::Mux is not built");
    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;

    auto& itv = score::test::base_interval(*doc);
    auto* proc = new oscr::ProcessModel<CvDyn>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7779}, dctx, &itv};
    auto names = [&] {
      std::vector<QString> res;
      for(auto* p : proc->inlets())
        res.push_back(p->name());
      return res;
    };
    // sample input, controller, In 0
    REQUIRE(proc->inlets().size() == 3);
    CHECK(proc->inlets()[1]->name() == "Count");
    CHECK(proc->inlets()[2]->name() == "In 0");
    REQUIRE(proc->outlets().size() == 1);
    const auto sample_in = proc->inlets()[0];
    const auto sample_in_name = sample_in->name();

    auto comp = std::make_shared<oscr::Executor<CvDyn>>(*proc, plug.context(), nullptr);
    REQUIRE(comp->node);
    setup.register_node(*proc, comp->node);
    run_exec(plug);
    auto& n = static_cast<oscr::safe_node<CvDyn>&>(*comp->node);
    check_registered(setup, *proc, n);
    CHECK(n.root_inputs()[0] == &n.arg_value_ports.in);
    CHECK(n.root_outputs()[0] == &n.arg_value_ports.out);

    control(*proc, 1)->setValue(3);
    run_exec(plug);
    spin();
    CHECK(
        names()
        == std::vector<QString>{sample_in_name, "Count", "In 0", "In 1", "In 2"});
    CHECK(proc->inlets()[0] == sample_in);
    check_registered(setup, *proc, n);
    CHECK(n.root_inputs()[0] == &n.arg_value_ports.in);
    REQUIRE(n.impl.effect.inputs.in_i.ports.size() == 3);

    control(*proc, 4)->setValue(0.75f);
    run_exec(plug);
    CHECK(n.impl.effect.inputs.in_i.ports[2].value == 0.75f);

    control(*proc, 1)->setValue(1);
    run_exec(plug);
    spin();
    CHECK(names() == std::vector<QString>{sample_in_name, "Count", "In 0"});
    check_registered(setup, *proc, n);
    CHECK(n.root_inputs()[0] == &n.arg_value_ports.in);

    comp->cleanup();
    run_exec(plug);
    comp.reset();
    delete proc;
    spin(50);
  });
}

TEST_CASE("Dynamic value outlets follow the model through resizes and keep their cables", "[avnd][dynamic-ports][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* demux = score::test::add_process(*doc, demux_uuid, {});
    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!demux || !mux)
      SKIP("ao::Demux / ao::Mux are not built");

    setController(*doc, *demux, 3);
    REQUIRE(demux->outlets().size() == 3);
    // demux.Output 2 -> mux.Input 0
    auto* cable = makeCable(*doc, 1, *demux->outlets()[2], *mux->inlets()[2]);
    const auto cable_id = cable->id();

    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;
    auto node = component(plug, *demux)->node;
    auto mux_node = component(plug, *mux)->node;
    check_registered(setup, *demux, *node);

    auto edge_of = [&](const Id<Process::Cable>& id) {
      auto it = setup.m_cables.find(id);
      REQUIRE(it != setup.m_cables.end());
      REQUIRE(it->second);
      return it->second;
    };
    auto check_cable = [&] {
      auto edge = edge_of(cable_id);
      CHECK(edge->out == node->root_outputs()[2]);
      CHECK(edge->in == mux_node->root_inputs()[2]);
      CHECK(ossia::contains(edge->out->targets, edge.get()));
      CHECK(ossia::contains(edge->in->sources, edge.get()));
    };
    check_cable();

    // (The object's own output vectors are only sized by the first tick, or
    //  by a reload: nothing to check on them before the first resize.)
    auto& obj = static_cast<oscr::safe_node<ao::Demux>&>(*node).impl.effect;

    // Grow: the cable stays on Output 2, now a new exec port
    setController(*doc, *demux, 5);
    run_exec(plug);
    spin();
    REQUIRE(demux->outlets().size() == 5);
    check_registered(setup, *demux, *node);
    check_cable();
    CHECK(obj.outputs.out_i.ports.size() == 5);

    // Shrink past it: the cable goes
    setController(*doc, *demux, 2);
    run_exec(plug);
    spin();
    REQUIRE(demux->outlets().size() == 2);
    check_registered(setup, *demux, *node);
    CHECK(score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc).cables.empty());
    CHECK(setup.m_cables.find(cable_id) == setup.m_cables.end());
    CHECK(obj.outputs.out_i.ports.size() == 2);
    // The other end has nothing left on it
    CHECK(mux_node->root_inputs()[2]->sources.empty());

    // And back
    doc->commandStack().undo();
    run_exec(plug);
    spin();
    REQUIRE(demux->outlets().size() == 5);
    check_registered(setup, *demux, *node);
    check_registered(setup, *mux, *mux_node);
    check_cable();

    spin(50);
  });
}

TEST_CASE("Dynamic audio outlets keep the interval's propagation through resizes", "[avnd][dynamic-ports][execution][audio]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto* splitter = score::test::add_process(*doc, audio_splitter_uuid, {});
    if(!splitter)
      SKIP("ao::AudioSplitter is not built");

    REQUIRE(splitter->inlets().size() == 2);
    REQUIRE(splitter->outlets().size() == 2);
    auto* channels = control(*splitter, 1);
    auto set_channels = [&](int n) {
      CommandDispatcher<>{dctx.commandStack}.submit<Scenario::SetControllerControlValue>(
          *channels, n, dctx);
    };
    auto out = [&](int i) {
      auto* o = qobject_cast<Process::AudioOutlet*>(splitter->outlets()[i]);
      REQUIRE(o);
      return o;
    };

    // A dynamic audio outlet is not propagated to the interval by itself
    CHECK(!out(0)->propagate());
    out(0)->setPropagate(true);
    out(1)->setPropagate(true);

    auto& plug = load_execution(*doc);
    auto& setup = plug.context().setup;
    auto node = component(plug, *splitter)->node;
    auto itv_node = plug.baseScenario()->baseInterval().OSSIAInterval()->node;
    REQUIRE(itv_node);

    auto edges_to_interval = [&](ossia::outlet* o) {
      int n = 0;
      for(auto e : o->targets)
        if(e->in_node == itv_node)
          n++;
      return n;
    };
    auto total_edges_to_interval = [&] {
      int n = 0;
      for(auto o : node->root_outputs())
        n += edges_to_interval(o);
      return n;
    };

    check_registered(setup, *splitter, *node);
    CHECK(edges_to_interval(node->root_outputs()[0]) == 1);
    CHECK(edges_to_interval(node->root_outputs()[1]) == 1);

    // Grow: the two propagated channels keep their edge, on their new exec
    // port, the new channels have none, nothing is doubled
    set_channels(4);
    run_exec(plug);
    spin();
    REQUIRE(splitter->outlets().size() == 4);
    check_registered(setup, *splitter, *node);
    CHECK(edges_to_interval(node->root_outputs()[0]) == 1);
    CHECK(edges_to_interval(node->root_outputs()[1]) == 1);
    CHECK(edges_to_interval(node->root_outputs()[2]) == 0);
    CHECK(edges_to_interval(node->root_outputs()[3]) == 0);
    CHECK(total_edges_to_interval() == 2);
    for(auto o : node->root_outputs())
      for(auto e : o->targets)
        CHECK(e->out == o);

    // Propagating a new one goes through the usual path
    out(3)->setPropagate(true);
    run_exec(plug);
    CHECK(edges_to_interval(node->root_outputs()[3]) == 1);
    CHECK(total_edges_to_interval() == 3);

    // Shrink to one channel: only its edge is left
    set_channels(1);
    run_exec(plug);
    spin();
    REQUIRE(splitter->outlets().size() == 1);
    check_registered(setup, *splitter, *node);
    CHECK(total_edges_to_interval() == 1);
    CHECK(edges_to_interval(node->root_outputs()[0]) == 1);

    // Undo: the channels come back with their propagation flag and edge
    doc->commandStack().undo();
    run_exec(plug);
    spin();
    REQUIRE(splitter->outlets().size() == 4);
    check_registered(setup, *splitter, *node);
    CHECK(out(1)->propagate());
    CHECK(!out(2)->propagate());
    CHECK(out(3)->propagate());
    CHECK(edges_to_interval(node->root_outputs()[0]) == 1);
    CHECK(edges_to_interval(node->root_outputs()[1]) == 1);
    CHECK(edges_to_interval(node->root_outputs()[2]) == 0);
    CHECK(edges_to_interval(node->root_outputs()[3]) == 1);
    CHECK(total_edges_to_interval() == 3);

    // An audio cable from a dynamic channel to another process follows it
    auto* splitter2 = score::test::add_process(*doc, audio_splitter_uuid, {});
    REQUIRE(splitter2);
    auto* cable = makeCable(*doc, 1, *splitter->outlets()[1], *splitter2->inlets()[0]);
    // Cabling an audio outlet takes it out of the interval's mix (CreateCable)
    CHECK(!out(1)->propagate());
    run_exec(plug);
    spin();
    CHECK(total_edges_to_interval() == 2);
    auto node2 = component(plug, *splitter2)->node;
    auto edge_of = [&](const Id<Process::Cable>& id) {
      auto it = setup.m_cables.find(id);
      REQUIRE(it != setup.m_cables.end());
      REQUIRE(it->second);
      return it->second;
    };
    {
      auto edge = edge_of(cable->id());
      CHECK(edge->out == node->root_outputs()[1]);
      CHECK(edge->in == node2->root_inputs()[0]);
    }
    set_channels(6);
    run_exec(plug);
    spin();
    check_registered(setup, *splitter, *node);
    {
      auto edge = edge_of(cable->id());
      CHECK(edge->out == node->root_outputs()[1]);
      CHECK(edge->in == node2->root_inputs()[0]);
      CHECK(ossia::contains(edge->out->targets, edge.get()));
    }
    CHECK(edges_to_interval(node->root_outputs()[0]) == 1);
    CHECK(edges_to_interval(node->root_outputs()[1]) == 0);
    CHECK(edges_to_interval(node->root_outputs()[3]) == 1);
    CHECK(total_edges_to_interval() == 2);

    spin(50);
  });
}
