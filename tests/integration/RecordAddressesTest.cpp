// "Record automations from here" and "Record messages from here": the
// addresses selected in the explorer are recorded by the engine from a point
// of a scenario, and placed in a new box there in one undoable step.
//
// What has to hold:
//  - while stopped, nothing records until an address gets a value; that value
//    starts playback from the point and is where the take starts;
//  - as automations: an automation for every number of the addresses that
//    moved (one for a float, one per axis of a vec2), bound to the address
//    and its index, spanning the values it got; values an automation cannot
//    play (strings) as the states of a scenario in the same box; an address
//    that got nothing is not placed;
//  - as messages: a scenario whose states send the values in order;
//  - undo removes the box, redo puts it back; undo is off while recording.

#include <State/Address.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <Curve/Settings/CurveSettingsModel.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include "../unit/FakeDeviceProtocol.hpp"

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
}

template <typename F>
bool spin_until(F f, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

//! A device of the explorer whose addresses the test sets
struct Addresses
{
  Explorer::DeviceDocumentPlugin& plug;
  QString name{QStringLiteral("rec")};

  Addresses(const score::GUIApplicationContext& ctx, score::Document& doc)
      : plug{doc.context().plugin<Explorer::DeviceDocumentPlugin>()}
  {
    score::test::fake::registerFakeProtocol(ctx);
    CommandDispatcher<>{doc.context().commandStack}.submit(new Explorer::Command::LoadDevice{
        plug, Device::Node{score::test::fake::fakeSettings(name, {}), nullptr}});
    REQUIRE(node());
  }

  Device::Node* node() const
  {
    for(auto& n : plug.rootNode())
      if(n.get<Device::DeviceSettings>().name == name)
        return &n;
    return nullptr;
  }

  State::Address add(const QString& addr, ossia::value v)
  {
    auto* dev = node();
    REQUIRE(dev);
    Device::AddressSettings as;
    as.name = addr;
    as.value = std::move(v);
    as.ioType = ossia::access_mode::BI;
    plug.updateProxy.addAddress(Device::NodePath{*dev}, as, dev->childCount());
    return State::Address{name, {addr}};
  }

  void push(const State::Address& a, ossia::value v)
  {
    auto* dev = plug.list().findDevice(name);
    REQUIRE(dev);
    REQUIRE(dev->getDevice());
    auto* n = ossia::net::find_node(
        dev->getDevice()->get_root_node(), "/" + a.path.front().toStdString());
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    n->get_parameter()->push_value(std::move(v));
  }
};

std::vector<Automation::ProcessModel*> automations(const Scenario::IntervalModel& box)
{
  std::vector<Automation::ProcessModel*> r;
  for(auto& p : box.processes)
    if(auto* a = qobject_cast<Automation::ProcessModel*>(&p))
      r.push_back(a);
  return r;
}

Scenario::ProcessModel* subScenario(const Scenario::IntervalModel& box)
{
  for(auto& p : box.processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&p))
      return s;
  return nullptr;
}

//! The values the states of a scenario send, in time order
std::vector<ossia::value> sent(const Scenario::ProcessModel& sc)
{
  std::vector<std::pair<TimeVal, ossia::value>> v;
  for(auto& st : sc.states)
  {
    const auto date = sc.event(st.eventId()).date();
    for(const auto& m : Process::flatten(st.messages().rootNode()))
      v.emplace_back(date, m.value);
  }
  std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
  std::vector<ossia::value> r;
  for(auto& e : v)
    r.push_back(e.second);
  return r;
}
}

TEST_CASE(
    "Recording addresses as automations starts on the first value and places a box",
    "[recording][addresses]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("rec_x", 0.f);
    const auto v = dev.add("rec_v", ossia::vec2f{0.f, 0.f});
    const auto s = dev.add("rec_s", std::string{});
    const auto still = dev.add("rec_still", 3.f);
    spin(100);

    auto& curves = ctx.settings<Curve::Settings::Model>();
    curves.setPlayWhileRecording(true);
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();

    const auto boxes_before = scenar.intervals.size();
    const int commands_before = doc->commandStack().currentIndex();
    const Scenario::Point pt{TimeVal::fromMsecs(1000), 0.3};
    auto* session = rec.recordAddresses(
        scenar, pt, {x, v, s, still}, Recording::AddressPlacement::Automations);
    REQUIRE(session);
    REQUIRE(session->sources() == 4);

    // Nothing plays until an address gets a value
    spin(300);
    CHECK(!exec.isPlaying());
    bool finished = false;
    QObject::connect(session, &Recording::RecordSession::finished, [&] { finished = true; });

    dev.push(x, 0.5f);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(300);
    dev.push(x, 0.2f);
    dev.push(v, ossia::vec2f{1.f, 2.f});
    dev.push(s, std::string{"a"});
    spin(200);
    dev.push(x, 0.9f);
    dev.push(s, std::string{"b"});
    spin(300);

    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(scenar.intervals.size() == boxes_before + 1);
    auto* box = session ? session->placedBox() : nullptr;
    REQUIRE(box);
    CHECK(box->parent() == &scenar);
    // Where playback started, at the point
    CHECK(std::abs((box->date() - pt.date).msec()) < 50.);
    CHECK(box->duration.defaultDuration().msec() > 500.);

    // x, and v's two axes; nothing for the address that did not move
    const auto autos = automations(*box);
    REQUIRE(autos.size() == 3);
    int x_count = 0, v0 = 0, v1 = 0;
    for(auto* a : autos)
    {
      const auto& addr = a->address();
      if(addr.address == x && addr.qualifiers.get().accessors.empty())
      {
        x_count++;
        CHECK(std::abs(a->min() - 0.2) < 1e-6);
        CHECK(std::abs(a->max() - 0.9) < 1e-6);
      }
      else if(addr.address == v && addr.qualifiers.get().accessors.size() == 1)
      {
        (addr.qualifiers.get().accessors[0] == 0 ? v0 : v1)++;
      }
      CHECK(addr.address != still);
    }
    CHECK(x_count == 1);
    CHECK(v0 == 1);
    CHECK(v1 == 1);

    // The strings, as messages, in order
    auto* sc = subScenario(*box);
    REQUIRE(sc);
    const auto vals = sent(*sc);
    REQUIRE(vals.size() >= 2);
    CHECK(vals[vals.size() - 2] == ossia::value{std::string{"a"}});
    CHECK(vals.back() == ossia::value{std::string{"b"}});

    // One undoable step
    REQUIRE(doc->commandStack().currentIndex() == commands_before + 1);
    doc->commandStack().undo();
    CHECK(scenar.intervals.size() == boxes_before);
    doc->commandStack().redo();
    CHECK(scenar.intervals.size() == boxes_before + 1);
  });
}

TEST_CASE(
    "Recording addresses as messages places their values as states",
    "[recording][addresses]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("msg_x", 0);
    spin(100);

    ctx.settings<Curve::Settings::Model>().setPlayWhileRecording(true);
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();

    auto* session = rec.recordAddresses(
        scenar, {TimeVal::zero(), 0.5}, {x}, Recording::AddressPlacement::Messages);
    REQUIRE(session);
    bool finished = false;
    QObject::connect(session, &Recording::RecordSession::finished, [&] { finished = true; });

    dev.push(x, 1);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(200);
    dev.push(x, 2);
    spin(200);
    dev.push(x, 3);
    spin(200);

    // Stopping playback ends it too
    engine.execution().request_stop();
    REQUIRE(spin_until([&] { return finished; }));

    auto* box = session->placedBox();
    REQUIRE(box);
    CHECK(automations(*box).empty());
    auto* sc = subScenario(*box);
    REQUIRE(sc);
    const auto vals = sent(*sc);
    REQUIRE(vals.size() == 3);
    CHECK(vals[0] == ossia::value{1});
    CHECK(vals[1] == ossia::value{2});
    CHECK(vals[2] == ossia::value{3});
  });
}

TEST_CASE("A recording of addresses stopped before any value places nothing", "[recording][addresses]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("none_x", 0.f);
    spin(100);

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    const auto boxes_before = scenar.intervals.size();
    const int commands_before = doc->commandStack().currentIndex();
    auto* session = rec.recordAddresses(
        scenar, {TimeVal::zero(), 0.5}, {x}, Recording::AddressPlacement::Automations);
    REQUIRE(session);
    bool finished = false;
    QObject::connect(session, &Recording::RecordSession::finished, [&] { finished = true; });
    spin(200);
    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    CHECK(!exec.isPlaying());
    CHECK(scenar.intervals.size() == boxes_before);
    CHECK(doc->commandStack().currentIndex() == commands_before);
    // Another can start
    CHECK(spin_until([&] { return rec.session() == nullptr; }));
  });
}
