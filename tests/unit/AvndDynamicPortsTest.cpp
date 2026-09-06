// Dynamic ports of avendish processes, at the model level.
//
// A process such as Mux has a "controller" control (its input count): editing
// it makes the process add or remove ports, from inside the control's own
// setValue(). Around that sit:
//  * SetControllerControlValue, the command the controller widgets submit,
//    which has to bring cables and port values back on undo;
//  * the controller widgets themselves, which must not submit the command
//    from inside their own event since the resize rebuilds them;
//  * save / load, which must restore the port count exactly once.

#include <State/Address.hpp>

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/SetControllerControlValue.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <set>

namespace
{
// ao::Mux: inlets = [Input count (controller), Current index, Input 0..N-1]
const QString mux_uuid = QStringLiteral("deb6c556-afda-47c2-b545-d74c8e3958e4");
// ao::ValueMixer: inlets = [Input count, Mode, Input 0..N-1, Mix 0..N-1, Solo 0..N-1, Mute 0..N-1]
const QString value_mixer_uuid = QStringLiteral("0fd108dd-daa8-4667-869d-f409da70e823");
// ao::Demux: inlets = [Output count, Current index, Input], outlets = [Output 0..N-1]
const QString demux_uuid = QStringLiteral("37ba8700-a924-4364-b761-a14cc036cef3");

Process::ControlInlet* controller(Process::ProcessModel& p)
{
  auto* c = qobject_cast<Process::ControlInlet*>(p.inlets()[0]);
  REQUIRE(c);
  return c;
}

int controllerValue(Process::ProcessModel& p)
{
  return ossia::convert<int>(controller(p)->value());
}

std::vector<QString> inletNames(const Process::ProcessModel& p)
{
  std::vector<QString> res;
  for(auto* i : p.inlets())
    res.push_back(i->name());
  return res;
}

std::vector<QString> outletNames(const Process::ProcessModel& p)
{
  std::vector<QString> res;
  for(auto* o : p.outlets())
    res.push_back(o->name());
  return res;
}

bool uniqueIds(const Process::ProcessModel& p)
{
  std::set<int32_t> ids;
  for(auto* i : p.inlets())
    if(!ids.insert(i->id().val()).second)
      return false;
  ids.clear();
  for(auto* o : p.outlets())
    if(!ids.insert(o->id().val()).second)
      return false;
  return true;
}

Scenario::ScenarioDocumentModel& scenarioModel(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
}

Process::Cable* makeCable(
    score::Document& doc, int id, const Process::Port& source, const Process::Port& sink)
{
  auto& ctx = doc.context();
  CommandDispatcher<>{ctx.commandStack}.submit<Dataflow::CreateCable>(
      scenarioModel(doc), Id<Process::Cable>{id}, Process::CableType::ImmediateGlutton,
      source, sink);
  auto it = scenarioModel(doc).cables.find(Id<Process::Cable>{id});
  REQUIRE(it != scenarioModel(doc).cables.end());
  return &*it;
}

void setController(score::Document& doc, Process::ProcessModel& p, int value)
{
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::SetControllerControlValue>(*controller(p), value, doc.context());
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

Process::ProcessModel* roundtrip_datastream(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  const QByteArray bytes = DataStreamReader::marshall(proc);
  DataStream::Deserializer des{bytes};
  return deserialize_interface(pl, des, dctx, parent);
}

Process::ProcessModel* roundtrip_json(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  JSONReader reader;
  reader.readFrom(proc);
  const auto doc = readJson(reader.toByteArray());
  JSONObject::Deserializer des{doc};
  return deserialize_interface(pl, des, dctx, parent);
}
}

TEST_CASE("The controller drives the number of dynamic ports", "[avnd][dynamic-ports][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mux)
      SKIP("ao::Mux is not built");

    // Default: 2 inputs
    CHECK(controllerValue(*mux) == 2);
    CHECK(
        inletNames(*mux)
        == std::vector<QString>{"Input count", "Current index", "Input 0", "Input 1"});
    CHECK(mux->outlets().size() == 1);

    SECTION("growing appends ports, in order, with unique ids")
    {
      controller(*mux)->setValue(5);
      CHECK(
          inletNames(*mux)
          == std::vector<QString>{
              "Input count", "Current index", "Input 0", "Input 1", "Input 2", "Input 3",
              "Input 4"});
      CHECK(uniqueIds(*mux));

      // The surviving ports are the same objects
      controller(*mux)->setValue(3);
      CHECK(
          inletNames(*mux)
          == std::vector<QString>{
              "Input count", "Current index", "Input 0", "Input 1", "Input 2"});
    }

    SECTION("shrinking removes the ports from the end, and keeps the others")
    {
      auto* input0 = mux->inlets()[2];
      auto* input1 = mux->inlets()[3];
      controller(*mux)->setValue(1);
      REQUIRE(mux->inlets().size() == 3);
      CHECK(mux->inlets()[2] == input0);
      CHECK(inletNames(*mux) == std::vector<QString>{"Input count", "Current index", "Input 0"});
      (void)input1;

      controller(*mux)->setValue(0);
      CHECK(inletNames(*mux) == std::vector<QString>{"Input count", "Current index"});

      controller(*mux)->setValue(2);
      CHECK(
          inletNames(*mux)
          == std::vector<QString>{"Input count", "Current index", "Input 0", "Input 1"});
      CHECK(uniqueIds(*mux));
    }

    SECTION("setting the same count again is a no-op")
    {
      auto before = mux->inlets();
      controller(*mux)->setValue(2);
      CHECK(mux->inlets() == before);
    }

    SECTION("out of range counts are ignored")
    {
      auto before = mux->inlets();
      controller(*mux)->setValue(-1);
      CHECK(mux->inlets() == before);
      controller(*mux)->setValue(100000);
      CHECK(mux->inlets() == before);
    }
  });
}

TEST_CASE("One controller can drive several dynamic port groups", "[avnd][dynamic-ports][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    if(!mixer)
      SKIP("ao::ValueMixer is not built");

    CHECK(controllerValue(*mixer) == 2);
    REQUIRE(mixer->inlets().size() == 2 + 4 * 2);

    controller(*mixer)->setValue(3);
    REQUIRE(mixer->inlets().size() == 2 + 4 * 3);
    CHECK(
        inletNames(*mixer)
        == std::vector<QString>{
            "Input count", "Mode", "Input 0", "Input 1", "Input 2", "Mix 0", "Mix 1",
            "Mix 2", "Solo 0", "Solo 1", "Solo 2", "Mute 0", "Mute 1", "Mute 2"});
    CHECK(uniqueIds(*mixer));

    // The knobs are controls, the raw value ports are not
    CHECK(qobject_cast<Process::ControlInlet*>(mixer->inlets()[2]) == nullptr);
    CHECK(qobject_cast<Process::ControlInlet*>(mixer->inlets()[5]) != nullptr);
    CHECK(qobject_cast<Process::ControlInlet*>(mixer->inlets()[8]) != nullptr);

    controller(*mixer)->setValue(1);
    REQUIRE(mixer->inlets().size() == 2 + 4 * 1);
    CHECK(
        inletNames(*mixer)
        == std::vector<QString>{"Input count", "Mode", "Input 0", "Mix 0", "Solo 0", "Mute 0"});
  });
}

TEST_CASE("Dynamic outlets follow their controller too", "[avnd][dynamic-ports][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* demux = score::test::add_process(*doc, demux_uuid, {});
    if(!demux)
      SKIP("ao::Demux is not built");

    CHECK(controllerValue(*demux) == 1);
    CHECK(outletNames(*demux) == std::vector<QString>{"Output 0"});

    controller(*demux)->setValue(4);
    CHECK(
        outletNames(*demux)
        == std::vector<QString>{"Output 0", "Output 1", "Output 2", "Output 3"});
    CHECK(uniqueIds(*demux));
    CHECK(demux->inlets().size() == 3);

    controller(*demux)->setValue(2);
    CHECK(outletNames(*demux) == std::vector<QString>{"Output 0", "Output 1"});
  });
}

TEST_CASE("Dynamic ports and their values survive save / load, without duplicates", "[avnd][dynamic-ports][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    if(!mixer)
      SKIP("ao::ValueMixer is not built");

    controller(*mixer)->setValue(3);
    REQUIRE(mixer->inlets().size() == 14);
    // Mix 2, Mute 1
    static_cast<Process::ControlInlet*>(mixer->inlets()[7])->setValue(0.75f);
    static_cast<Process::ControlInlet*>(mixer->inlets()[12])->setValue(true);

    const auto names = inletNames(*mixer);

    for(auto* loaded :
        {roundtrip_datastream(*mixer, dctx, &doc->model()),
         roundtrip_json(*mixer, dctx, &doc->model())})
    {
      REQUIRE(loaded);
      // Loading runs the controller's setup hook with the saved count: it must
      // find the saved ports and not create a second set.
      CHECK(inletNames(*loaded) == names);
      CHECK(controllerValue(*loaded) == 3);
      CHECK(uniqueIds(*loaded));
      CHECK(
          static_cast<Process::ControlInlet*>(loaded->inlets()[7])->value()
          == ossia::value{0.75f});
      CHECK(
          static_cast<Process::ControlInlet*>(loaded->inlets()[12])->value()
          == ossia::value{true});

      // And it is still live: the controller keeps working on the loaded copy
      controller(*loaded)->setValue(1);
      CHECK(loaded->inlets().size() == 6);
      delete loaded;
    }
  });
}

TEST_CASE("A whole document with dynamic ports and cables reloads", "[avnd][dynamic-ports][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* a = score::test::add_process(*doc, mux_uuid, {});
    auto* b = score::test::add_process(*doc, mux_uuid, {});
    if(!a || !b)
      SKIP("ao::Mux is not built");

    controller(*a)->setValue(4);
    // b.Output -> a.Input 3
    makeCable(*doc, 1, *b->outlets()[0], *a->inlets()[5]);

    auto* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded);

    auto& itv = score::test::base_interval(*reloaded);
    auto ra_it = itv.processes.find(a->id());
    REQUIRE(ra_it != itv.processes.end());
    auto& ra = *ra_it;
    CHECK(inletNames(ra) == inletNames(*a));
    CHECK(controllerValue(ra) == 4);

    auto& cables = scenarioModel(*reloaded).cables;
    REQUIRE(cables.size() == 1);
    auto& cable = *cables.begin();
    CHECK(cable.sink().try_find(reloaded->context()) == ra.inlets()[5]);
    CHECK(ra.inlets()[5]->cables().size() == 1);
  });
}

TEST_CASE("SetControllerControlValue: undo brings ports, values and cables back", "[avnd][dynamic-ports][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& stack = doc->commandStack();

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mixer || !mux)
      SKIP("ao::ValueMixer / ao::Mux are not built");

    // 3 inputs, with values on the knobs of the last one and a cable into it
    setController(*doc, *mixer, 3);
    REQUIRE(mixer->inlets().size() == 14);
    auto* mix2 = static_cast<Process::ControlInlet*>(mixer->inlets()[7]);
    auto* mute2 = static_cast<Process::ControlInlet*>(mixer->inlets()[13]);
    mix2->setValue(0.75f);
    mute2->setValue(true);
    auto* cable = makeCable(*doc, 1, *mux->outlets()[0], *mixer->inlets()[4]); // Input 2
    const auto cable_id = cable->id();
    const int commands_before = stack.size();

    SECTION("shrink, undo, redo")
    {
      setController(*doc, *mixer, 1);
      CHECK(stack.size() == commands_before + 1);
      CHECK(mixer->inlets().size() == 6);
      // The cable went away with its port
      CHECK(scenarioModel(*doc).cables.size() == 0);
      CHECK(mux->outlets()[0]->cables().empty());

      stack.undo();
      spin();
      REQUIRE(mixer->inlets().size() == 14);
      CHECK(controllerValue(*mixer) == 3);
      CHECK(
          inletNames(*mixer)
          == std::vector<QString>{
              "Input count", "Mode", "Input 0", "Input 1", "Input 2", "Mix 0", "Mix 1",
              "Mix 2", "Solo 0", "Solo 1", "Solo 2", "Mute 0", "Mute 1", "Mute 2"});
      // The re-created ports have their previous values
      CHECK(static_cast<Process::ControlInlet*>(mixer->inlets()[7])->value() == ossia::value{0.75f});
      CHECK(static_cast<Process::ControlInlet*>(mixer->inlets()[13])->value() == ossia::value{true});
      // And the cable is back, on both ends, with the same id
      REQUIRE(scenarioModel(*doc).cables.size() == 1);
      auto& c = *scenarioModel(*doc).cables.begin();
      CHECK(c.id() == cable_id);
      CHECK(c.sink().try_find(dctx) == mixer->inlets()[4]);
      CHECK(c.source().try_find(dctx) == mux->outlets()[0]);
      CHECK(mixer->inlets()[4]->cables().size() == 1);
      CHECK(mux->outlets()[0]->cables().size() == 1);

      stack.redo();
      spin();
      CHECK(mixer->inlets().size() == 6);
      CHECK(scenarioModel(*doc).cables.size() == 0);
      CHECK(mux->outlets()[0]->cables().empty());

      stack.undo();
      spin();
      CHECK(mixer->inlets().size() == 14);
      CHECK(scenarioModel(*doc).cables.size() == 1);
    }

    SECTION("grow, undo")
    {
      setController(*doc, *mixer, 5);
      CHECK(mixer->inlets().size() == 22);
      // The surviving port and its cable were not touched
      CHECK(scenarioModel(*doc).cables.size() == 1);
      CHECK(mixer->inlets()[4]->cables().size() == 1);
      CHECK(static_cast<Process::ControlInlet*>(mixer->inlets()[9])->value() == ossia::value{0.75f});

      stack.undo();
      spin();
      CHECK(mixer->inlets().size() == 14);
      CHECK(scenarioModel(*doc).cables.size() == 1);
      CHECK(mixer->inlets()[4]->cables().size() == 1);
      CHECK(static_cast<Process::ControlInlet*>(mixer->inlets()[7])->value() == ossia::value{0.75f});
    }

    SECTION("the ongoing form: several updates, one command")
    {
      auto& disp = dctx.dispatcher;
      disp.submit<Scenario::SetControllerControlValue>(*controller(*mixer), 4, dctx);
      CHECK(mixer->inlets().size() == 18);
      disp.submit<Scenario::SetControllerControlValue>(*controller(*mixer), 1, dctx);
      CHECK(mixer->inlets().size() == 6);
      disp.submit<Scenario::SetControllerControlValue>(*controller(*mixer), 2, dctx);
      CHECK(mixer->inlets().size() == 10);
      disp.commit();
      CHECK(stack.size() == commands_before + 1);
      CHECK(mixer->inlets().size() == 10);
      CHECK(scenarioModel(*doc).cables.size() == 0);

      stack.undo();
      spin();
      CHECK(mixer->inlets().size() == 14);
      CHECK(scenarioModel(*doc).cables.size() == 1);
      CHECK(mixer->inlets()[4]->cables().size() == 1);
    }

    SECTION("the command survives serialization")
    {
      auto* cmd = new Scenario::SetControllerControlValue{*controller(*mixer), 1, dctx};
      const QByteArray data = cmd->serialize();
      delete cmd;

      auto* cmd2 = new Scenario::SetControllerControlValue;
      cmd2->deserialize(data);
      CommandDispatcher<>{dctx.commandStack}.submit(cmd2);
      CHECK(mixer->inlets().size() == 6);
      CHECK(scenarioModel(*doc).cables.size() == 0);
      stack.undo();
      spin();
      CHECK(mixer->inlets().size() == 14);
      CHECK(scenarioModel(*doc).cables.size() == 1);
    }
  });
}

TEST_CASE("A resize outside of the command does not leave dangling cables", "[avnd][dynamic-ports][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto* a = score::test::add_process(*doc, mux_uuid, {});
    auto* b = score::test::add_process(*doc, mux_uuid, {});
    if(!a || !b)
      SKIP("ao::Mux is not built");

    controller(*a)->setValue(3);
    // b.Output -> a.Input 0 (stays) and b.Output -> a.Input 2 (goes away)
    makeCable(*doc, 1, *b->outlets()[0], *a->inlets()[2]);
    makeCable(*doc, 2, *b->outlets()[0], *a->inlets()[4]);
    REQUIRE(scenarioModel(*doc).cables.size() == 2);
    REQUIRE(b->outlets()[0]->cables().size() == 2);

    // A preset, a remote control... anything that sets the value directly
    controller(*a)->setValue(1);
    REQUIRE(a->inlets().size() == 3);

    auto& cables = scenarioModel(*doc).cables;
    REQUIRE(cables.size() == 1);
    CHECK(cables.begin()->id() == Id<Process::Cable>{1});
    CHECK(cables.begin()->sink().try_find(dctx) == a->inlets()[2]);
    CHECK(b->outlets()[0]->cables().size() == 1);
    CHECK(a->inlets()[2]->cables().size() == 1);
  });
}

TEST_CASE("The controller spinbox submits its command after its own event", "[avnd][dynamic-ports][ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& stack = doc->commandStack();

    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mux)
      SKIP("ao::Mux is not built");

    auto* ctl = controller(*mux);
    // Set by the model for every controller: the widget reports on release only
    CHECK(ctl->noValueChangeOnMove);

    auto& factories = ctx.interfaces<Process::PortFactoryList>();
    auto* factory = factories.get(ctl->concreteKey());
    REQUIRE(factory);

    QGraphicsScene scene;
    QObject context;
    auto* item = factory->makeControlItem(*ctl, dctx, nullptr, &context);
    REQUIRE(item);
    scene.addItem(item);
    auto* spinbox = qgraphicsitem_cast<score::QGraphicsIntSpinbox*>(item);
    REQUIRE(spinbox);
    CHECK(spinbox->value() == 2);

    const int commands_before = stack.size();

    // The release of the spinbox: value set, then sliderMoved. The model must
    // not change yet, the command runs once the event is over.
    spinbox->setValue(4);
    spinbox->sliderMoved();
    CHECK(mux->inlets().size() == 4);
    CHECK(stack.size() == commands_before);

    spin();
    CHECK(mux->inlets().size() == 6);
    CHECK(controllerValue(*mux) == 4);
    CHECK(stack.size() == commands_before + 1);

    // The scene hiding the widget on the way out of the release re-emits the
    // edit (QEvent::UngrabMouse): the second one finds nothing to do.
    spinbox->setValue(5);
    spinbox->sliderMoved();
    spinbox->sliderMoved();
    spin();
    CHECK(mux->inlets().size() == 7);
    CHECK(stack.size() == commands_before + 2);

    // A release that did not move the value submits nothing
    spinbox->sliderMoved();
    spin();
    CHECK(stack.size() == commands_before + 2);

    // The widget follows the model
    stack.undo();
    spin();
    CHECK(controllerValue(*mux) == 4);
    CHECK(spinbox->value() == 4);
    // size() counts the redo stack too
    CHECK(stack.size() == commands_before + 2);

    // The pending edit is not tied to the widget: the resize it asks for
    // tears the widget down, and the edit still has to go through.
    spinbox->setValue(1);
    spinbox->sliderMoved();
    scene.removeItem(item);
    delete item;
    spin();
    CHECK(controllerValue(*mux) == 1);
    CHECK(stack.size() == commands_before + 2);
  });
}

TEST_CASE("SetControllerControlValue: cables on ports that move keep their port", "[avnd][dynamic-ports][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& stack = doc->commandStack();

    auto* mixer = score::test::add_process(*doc, value_mixer_uuid, {});
    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mixer || !mux)
      SKIP("ao::ValueMixer / ao::Mux are not built");

    // count 2: [Input count, Mode, Input 0, Input 1, Mix 0, Mix 1, Solo 0, Solo 1, Mute 0, Mute 1]
    auto* mode = mixer->inlets()[1];
    auto* mix0 = mixer->inlets()[4];
    auto* mute1 = mixer->inlets()[9];
    REQUIRE(mix0->name() == "Mix 0");
    REQUIRE(mute1->name() == "Mute 1");

    // A group growing pushes Mix 0 and Mute 1 to another index; Mode stays put.
    makeCable(*doc, 1, *mux->outlets()[0], *mix0);
    makeCable(*doc, 2, *mux->outlets()[0], *mute1);
    makeCable(*doc, 3, *mux->outlets()[0], *mode);
    REQUIRE(scenarioModel(*doc).cables.size() == 3);

    auto sinks = [&] {
      std::set<const Process::Port*> res;
      for(auto& c : scenarioModel(*doc).cables)
        res.insert(c.sink().try_find(dctx));
      return res;
    };
    const std::set<const Process::Port*> all{mode, mix0, mute1};

    setController(*doc, *mixer, 3);
    REQUIRE(mixer->inlets().size() == 14);
    // The ports survived the resize, at a new index
    // [Input count, Mode, Input 0-2, Mix 0-2, Solo 0-2, Mute 0-2]
    CHECK(mixer->inlets()[5] == mix0);
    CHECK(mixer->inlets()[12] == mute1);
    // And so did their cables, at both ends
    CHECK(scenarioModel(*doc).cables.size() == 3);
    CHECK(sinks() == all);
    CHECK(mix0->cables().size() == 1);
    CHECK(mute1->cables().size() == 1);
    CHECK(mode->cables().size() == 1);
    CHECK(mux->outlets()[0]->cables().size() == 3);

    stack.undo();
    spin();
    REQUIRE(mixer->inlets().size() == 10);
    CHECK(scenarioModel(*doc).cables.size() == 3);
    CHECK(sinks() == all);
    CHECK(mux->outlets()[0]->cables().size() == 3);

    stack.redo();
    spin();
    REQUIRE(mixer->inlets().size() == 14);
    CHECK(scenarioModel(*doc).cables.size() == 3);
    CHECK(sinks() == all);
    CHECK(mux->outlets()[0]->cables().size() == 3);

    // Shrinking below Mute 1 takes only its cable
    setController(*doc, *mixer, 1);
    REQUIRE(mixer->inlets().size() == 6);
    CHECK(mixer->inlets()[3] == mix0);
    CHECK(scenarioModel(*doc).cables.size() == 2);
    CHECK(sinks() == std::set<const Process::Port*>{mode, mix0});
    CHECK(mux->outlets()[0]->cables().size() == 2);

    stack.undo();
    spin();
    REQUIRE(mixer->inlets().size() == 14);
    CHECK(scenarioModel(*doc).cables.size() == 3);
    // Mute 1 was re-created by the undo
    CHECK(mixer->inlets()[12]->name() == "Mute 1");
    CHECK(sinks() == std::set<const Process::Port*>{mode, mix0, mixer->inlets()[12]});
    CHECK(mux->outlets()[0]->cables().size() == 3);
  });
}

TEST_CASE("A controller saved as a plain spin box loads as a controller", "[avnd][dynamic-ports][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto* a = score::test::add_process(*doc, mux_uuid, {});
    if(!a)
      SKIP("ao::Mux is not built");

    controller(*a)->setValue(3);
    REQUIRE(a->inlets().size() == 5);
    const auto controller_id = controller(*a)->id();
    const State::AddressAccessor address{State::Address{"dev", {"count"}}};
    controller(*a)->setAddress(address);

    // Documents saved before the controller ports existed have the controller
    // as a plain Process::IntSpinBox: same data, another factory key.
    const auto controller_key = controller(*a)->concreteKey();
    const auto plain_key = Process::IntSpinBox::static_concreteKey();
    REQUIRE(controller_key != plain_key);

    JSONReader reader;
    reader.readFrom(*a);
    QByteArray json = reader.toByteArray();
    const auto from = score::uuids::toByteArray(controller_key.impl());
    const auto to = score::uuids::toByteArray(plain_key.impl());
    REQUIRE(json.count(from) == 1);
    json.replace(from, to);

    auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
    const auto jdoc = readJson(json);
    JSONObject::Deserializer des{jdoc};
    auto* loaded = deserialize_interface(pl, des, dctx, &doc->model());
    REQUIRE(loaded);

    REQUIRE(loaded->inlets().size() == 5);
    CHECK(inletNames(*loaded) == inletNames(*a));
    CHECK(uniqueIds(*loaded));
    auto* ctl = controller(*loaded);
    // It comes up as a controller: that is what selects the widget that
    // defers its edit and the command that keeps the cables on undo
    CHECK(ctl->concreteKey() == controller_key);
    CHECK(ctl->id() == controller_id);
    CHECK(ctl->noValueChangeOnMove);
    CHECK(ossia::convert<int>(ctl->value()) == 3);
    CHECK(ctl->address() == address);

    // And it drives the ports
    ctl->setValue(1);
    CHECK(loaded->inlets().size() == 3);
    delete loaded;
  });
}

TEST_CASE("The port items of removed ports go away with them", "[avnd][dynamic-ports][ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* mux = score::test::add_process(*doc, mux_uuid, {});
    if(!mux)
      SKIP("ao::Mux is not built");
    controller(*mux)->setValue(3);
    REQUIRE(mux->inlets().size() == 5);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(root);
    auto* node = new Process::NodeItem{*mux, pctx, TimeVal::fromMsecs(1000.), root};
    spin();

    // The item of a port is registered by model port
    auto item_of = [&](const Process::Port* p) -> Dataflow::PortItem* {
      auto it = dfm.ports().find(p);
      return it != dfm.ports().end() ? it->second : nullptr;
    };
    std::vector<const Process::Port*> shown;
    for(auto* p : mux->inlets())
      if(item_of(p))
        shown.push_back(p);
    REQUIRE(!shown.empty());

    auto* doomed = mux->inlets()[4];
    const bool doomed_shown = item_of(doomed) != nullptr;

    // The port is deleted right after inletsChanged(): its item must be gone
    // by then, or a port later allocated at the same address would find it.
    controller(*mux)->setValue(2);
    CHECK(item_of(doomed) == nullptr);
    spin();
    CHECK(item_of(doomed) == nullptr);
    for(auto* p : shown)
      if(p != doomed)
      {
        auto* item = item_of(p);
        CHECK(item);
        if(item)
          CHECK(&item->port() == p);
      }

    // Ports that come back get an item of their own
    controller(*mux)->setValue(4);
    spin();
    for(auto* p : shown)
      if(p != doomed)
        CHECK(item_of(p));
    if(doomed_shown)
    {
      CHECK(item_of(mux->inlets()[4]));
      CHECK(item_of(mux->inlets()[5]));
    }

    delete node;
    scene.removeItem(root);
    delete root;
  });
}

TEST_CASE("Undoing a preset load puts the cables back at both ends", "[avnd][dynamic-ports][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& stack = doc->commandStack();

    auto* a = score::test::add_process(*doc, mux_uuid, {});
    auto* b = score::test::add_process(*doc, mux_uuid, {});
    if(!a || !b)
      SKIP("ao::Mux is not built");

    // A preset with two inputs, loaded on a mux with four and a cable on the last
    const Process::Preset two_inputs = a->savePreset();
    controller(*a)->setValue(4);
    REQUIRE(a->inlets().size() == 6);
    makeCable(*doc, 1, *b->outlets()[0], *a->inlets()[5]);
    REQUIRE(b->outlets()[0]->cables().size() == 1);

    // A process with dynamic ports gets the command that backs the cables up
    auto& factories = ctx.interfaces<Process::LoadPresetCommandFactoryList>();
    auto* cmd = factories.make(&Process::LoadPresetCommandFactory::make, *a, two_inputs, dctx);
    REQUIRE(cmd);
    CommandDispatcher<>{dctx.commandStack}.submit(cmd);
    REQUIRE(a->inlets().size() == 4);
    CHECK(scenarioModel(*doc).cables.empty());
    CHECK(b->outlets()[0]->cables().empty());

    stack.undo();
    spin();
    REQUIRE(a->inlets().size() == 6);
    REQUIRE(scenarioModel(*doc).cables.size() == 1);
    auto& c = *scenarioModel(*doc).cables.begin();
    CHECK(c.sink().try_find(dctx) == a->inlets()[5]);
    CHECK(c.source().try_find(dctx) == b->outlets()[0]);
    CHECK(a->inlets()[5]->cables().size() == 1);
    // The other end had been emptied by the load and was not reloaded
    CHECK(b->outlets()[0]->cables().size() == 1);
  });
}
