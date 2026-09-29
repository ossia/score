// A state played by the execution leaves out the addresses that a tween
// automation of the interval it starts takes over; played from the UI it sends
// them all.

#include <State/Address.hpp>
#include <State/Message.hpp>

#include <Process/State/MessageNode.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Curve/CurveModel.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <algorithm>

namespace
{
struct Scene
{
  ossia::net::generic_device dev{"dev"};
  ossia::execution_state exec;

  explicit Scene(int count)
  {
    for(int i = 0; i < count; i++)
      ossia::net::find_or_create_node(dev.get_root_node(), "/p" + std::to_string(i))
          .create_parameter(ossia::val_type::FLOAT);
    ossia::net::find_or_create_node(dev.get_root_node(), "/v")
        .create_parameter(ossia::val_type::VEC3F);
    exec.register_device(&dev);
    exec.apply_device_changes();
  }

  const ossia::net::parameter_base* param(const std::string& path)
  {
    auto n = ossia::net::find_node(dev.get_root_node(), path);
    return n ? n->get_parameter() : nullptr;
  }
};

State::AddressAccessor address(const QString& s)
{
  auto a = State::parseAddressAccessor(s);
  REQUIRE(a);
  return *a;
}

//! The (parameter, accessor) pairs a state sends
std::vector<std::pair<const ossia::net::parameter_base*, ossia::destination_index>>
sent(const ossia::state& s)
{
  std::vector<std::pair<const ossia::net::parameter_base*, ossia::destination_index>> res;
  for(const auto& e : s)
    if(auto m = e.target<ossia::message>())
      res.emplace_back(&m->dest.address(), m->dest.index);
  return res;
}

bool sends(const ossia::state& s, const ossia::net::parameter_base* p)
{
  return std::ranges::any_of(sent(s), [p](const auto& e) { return e.first == p; });
}

//! A state, the interval it starts and the automations in that interval
struct Cue
{
  Scenario::StateModel* state{};
  Scenario::IntervalModel* interval{};
  std::vector<Automation::ProcessModel*> automations;
};

Cue makeCue(
    score::Document& doc, const std::vector<std::pair<QString, bool>>& automations)
{
  const auto& ctx = doc.context();
  auto& scenar = score::test::base_scenario(doc);
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, ctx};
  auto [ts, ev, st] = m.createDot(scenar, {TimeVal::fromMsecs(1000), 0.3});
  auto& itv = m.createIntervalAfter(scenar, st.id(), {TimeVal::fromMsecs(3000), 0.3});

  Cue cue{&st, &itv, {}};
  for(const auto& [addr, tween] : automations)
  {
    auto& proc = m.automate(
        itv, {}, getStrongId(itv.processes), address(addr), Curve::CurveDomain{0.25, 0.75},
        tween);
    cue.automations.push_back(&static_cast<Automation::ProcessModel&>(proc));
  }
  m.commit();
  return cue;
}
}

TEST_CASE(
    "a state played by the execution leaves out what a tween automation takes over",
    "[integration][state][tween]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    const auto& ctx = doc->context();
    Scene scene{2};
    auto cue = makeCue(*doc, {{"dev:/p0", true}});
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
        *cue.state, State::MessageList{{address("dev:/p0"), 0.3f}, {address("dev:/p1"), 0.5f}});

    using Engine::score_to_ossia::StatePlay;
    const auto p0 = scene.param("/p0");
    const auto p1 = scene.param("/p1");
    REQUIRE(p0);
    REQUIRE(p1);

    // The state sets both; the automation takes p0 over from there
    const auto fromUi = Engine::score_to_ossia::state(*cue.state, scene.exec);
    CHECK(sends(fromUi, p0));
    CHECK(sends(fromUi, p1));

    const auto played
        = Engine::score_to_ossia::state(*cue.state, scene.exec, StatePlay::Execution);
    CHECK(!sends(played, p0));
    CHECK(sends(played, p1));

    // Without tween the automation takes nothing over, and the state is told so
    // that its execution rebuilds what it sends.
    int updates = 0;
    QObject::connect(
        cue.state, &Scenario::StateModel::sig_statesUpdated, cue.state,
        [&] { updates++; });
    CommandDispatcher<>{ctx.commandStack}.submit<Automation::SetTween>(
        *cue.automations.front(), false);
    CHECK(updates > 0);
    CHECK(sends(
        Engine::score_to_ossia::state(*cue.state, scene.exec, StatePlay::Execution), p0));

    updates = 0;
    doc->commandStack().undo();
    CHECK(updates > 0);
    CHECK(!sends(
        Engine::score_to_ossia::state(*cue.state, scene.exec, StatePlay::Execution), p0));
  });
}

TEST_CASE(
    "a tween automation on one member of a vector takes over only that member",
    "[integration][state][tween]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    const auto& ctx = doc->context();
    Scene scene{0};
    auto cue = makeCue(*doc, {{"dev:/v@[1]", true}});
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
        *cue.state,
        State::MessageList{{address("dev:/v@[0]"), 0.5f}, {address("dev:/v@[1]"), 0.5f}});

    const auto v = scene.param("/v");
    REQUIRE(v);
    const auto played = sent(Engine::score_to_ossia::state(
        *cue.state, scene.exec, Engine::score_to_ossia::StatePlay::Execution));
    REQUIRE(played.size() == 1);
    CHECK(played.front().first == v);
    CHECK(played.front().second == ossia::destination_index{0});
  });
}

TEST_CASE(
    "a state starting many tween automations sends only what they do not take over",
    "[integration][state][tween][scale]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    const auto& ctx = doc->context();
    constexpr int count = 1000;
    Scene scene{2 * count};

    // Every address is set; every even one is automated with tween
    std::vector<std::pair<QString, bool>> automations;
    State::MessageList messages;
    for(int i = 0; i < count; i++)
    {
      automations.emplace_back(QStringLiteral("dev:/p%1").arg(2 * i), true);
      messages.push_back({address(QStringLiteral("dev:/p%1").arg(2 * i)), float(i)});
      messages.push_back({address(QStringLiteral("dev:/p%1").arg(2 * i + 1)), float(i)});
    }
    auto cue = makeCue(*doc, automations);
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
        *cue.state, messages);

    const auto played = sent(Engine::score_to_ossia::state(
        *cue.state, scene.exec, Engine::score_to_ossia::StatePlay::Execution));
    CHECK(played.size() == std::size_t(count));
    for(int i = 0; i < count; i++)
    {
      const auto odd = scene.param("/p" + std::to_string(2 * i + 1));
      CHECK(std::ranges::any_of(played, [odd](const auto& e) { return e.first == odd; }));
    }

    const auto fromUi = sent(Engine::score_to_ossia::state(*cue.state, scene.exec));
    CHECK(fromUi.size() == std::size_t(2 * count));
  });
}
