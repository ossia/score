// Two automations of the same address meeting at a state keep each other's
// end points in sync: the one that ends there sets the start of the one that
// follows, and the other way around. The values go through each curve's
// [min, max] scaling in float, and are stored as doubles, so an exact
// comparison always sees a difference while SegmentModel::setStart / setEnd
// (fuzzy, as QPointF's operator!=) change nothing. A side that announces a
// change anyway makes the two call each other until the stack overflows.
#include <State/Address.hpp>

#include <Process/Process.hpp>

#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QApplication>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

namespace
{
using score::test::base_scenario;

Automation::ProcessModel& addAutomation(score::Document& doc, Scenario::IntervalModel& itv)
{
  auto cmd = new Scenario::Command::AddOnlyProcessToInterval{
      itv, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}, QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
  auto& p = itv.processes.at(cmd->processId());
  return *safe_cast<Automation::ProcessModel*>(&p);
}

Automation::ProcessModel* automationIn(Scenario::IntervalModel& itv)
{
  for(auto& p : itv.processes)
    if(auto a = qobject_cast<Automation::ProcessModel*>(&p))
      return a;
  return nullptr;
}

double endY(Automation::ProcessModel& a)
{
  return a.curve().sortedSegments().back()->end().y();
}
double startY(Automation::ProcessModel& a)
{
  return a.curve().sortedSegments().front()->start().y();
}
}

TEST_CASE(
    "automations of one address around a state settle on the value they share",
    "[integration][automation]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto& ctx = doc->context();
    auto& scenar = base_scenario(*doc);

    auto create1 = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto id1 = create1->createdInterval();
    CommandDispatcher<>{ctx.commandStack}.submit(create1);
    auto& itv1 = scenar.interval(id1);
    auto create2 = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, itv1.endState(), TimeVal::fromMsecs(4000), 0.3, false};
    const auto id2 = create2->createdInterval();
    CommandDispatcher<>{ctx.commandStack}.submit(create2);
    auto& itv2 = scenar.interval(id2);

    // 0.1 is 0.0125 in the first range and (0.1 - 0.001) / 7.999 in the
    // second, neither exact in float.
    const auto addr = *State::parseAddressAccessor("score:/controls/op/decay");
    auto& a = addAutomation(*doc, itv1);
    auto& b = addAutomation(*doc, itv2);
    a.setMin(0.);
    a.setMax(8.);
    b.setMin(0.001);
    b.setMax(8.);
    a.curve().sortedSegments().back()->setEnd({1., 0.1 / 8.});
    b.curve().sortedSegments().front()->setStart({0., (0.1 - 0.001) / 7.999});
    a.setAddress(addr);
    b.setAddress(addr);

    // Editing either curve must not recurse.
    a.curve().changed();
    b.curve().changed();
    QApplication::processEvents();
    CHECK(endY(a) == Catch::Approx(0.0125));
    CHECK(startY(b) == Catch::Approx((0.1 - 0.001) / 7.999));

    // Loading the document connects them again, which must not recurse either.
    for(auto reloaded :
        {score::test::reload_via_json(app, *doc), score::test::reload_via_bytes(app, *doc)})
    {
      REQUIRE(reloaded);
      auto& rs = base_scenario(*reloaded);
      auto ra = automationIn(rs.interval(id1));
      auto rb = automationIn(rs.interval(id2));
      REQUIRE(ra);
      REQUIRE(rb);
      CHECK(endY(*ra) == Catch::Approx(0.0125));
      CHECK(startY(*rb) == Catch::Approx((0.1 - 0.001) / 7.999));

      // A real change still reaches the other automation.
      ra->curve().sortedSegments().back()->setEnd({1., 0.5});
      ra->curve().changed();
      QApplication::processEvents();
      CHECK(startY(*rb) == Catch::Approx((4. - 0.001) / 7.999).epsilon(1e-5));
    }
  });
}
