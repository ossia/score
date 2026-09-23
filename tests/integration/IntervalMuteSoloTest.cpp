// Mute and solo of an interval: undoable, saved with the document, and a solo
// silences the buses it does not involve.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/MakeBus.hpp>
#include <Scenario/Commands/Interval/SetMuteSolo.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Process/Commands/EditPort.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace
{
Scenario::ScenarioDocumentModel& scenarioDoc(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());
}

Scenario::IntervalModel& addInterval(
    score::Document& doc, const Scenario::ProcessModel& scenario, int start_ms,
    double y)
{
  CommandDispatcher<> disp{doc.context().commandStack};
  auto* start = new Scenario::Command::CreateTimeSync_Event_State{
      scenario, TimeVal::fromMsecs(start_ms), y};
  disp.submit(start);
  auto* itv = new Scenario::Command::CreateInterval_State_Event_TimeSync{
      scenario, start->createdState(), TimeVal::fromMsecs(start_ms + 3000), y, false};
  disp.submit(itv);
  return scenario.intervals.at(itv->createdInterval());
}

Scenario::ProcessModel& addScenario(score::Document& doc, Scenario::IntervalModel& itv)
{
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  auto& sc = m.createProcess<Scenario::ProcessModel>(itv, QString{}, QPointF{});
  m.commit();
  return sc;
}

void makeBus(score::Document& doc, const Scenario::IntervalModel& itv)
{
  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::SetBus>(scenarioDoc(doc), itv, true);
}

void solo(score::Document& doc, const Scenario::IntervalModel& itv, bool b)
{
  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::SetIntervalSoloed>(itv, b);
}

QByteArray saveAsJson(score::Document& doc)
{
  JSONReader w;
  doc.saveAsJson(w);
  return w.toByteArray();
}

score::Document* loadJson(const score::GUIApplicationContext& ctx, QByteArray bytes)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  auto doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("mute-solo.score"), std::move(bytes), JSONObject::type(),
      *delegates.begin());
  QApplication::processEvents();
  return doc;
}
}

TEST_CASE("Muting an interval is undoable and saved", "[integration][interval][mute]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& root = scenarioDoc(*doc).baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());
    auto& itv = addInterval(*doc, scenario, 1000, 0.3);
    const auto id = itv.id();

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::SetIntervalMuted>(itv, true);
    CHECK(itv.muted());
    CHECK(itv.executionState() == Scenario::IntervalExecutionState::Muted);

    auto& stack = doc->commandStack();
    stack.undo();
    CHECK(!itv.muted());
    stack.redo();
    CHECK(itv.muted());

    disp.submit<Scenario::Command::SetIntervalSoloed>(itv, true);
    CHECK(itv.soloed());

    auto* loaded = loadJson(ctx, saveAsJson(*doc));
    REQUIRE(loaded);
    auto& lroot = scenarioDoc(*loaded).baseInterval();
    auto& lscenario = static_cast<Scenario::ProcessModel&>(*lroot.processes.begin());
    auto& litv = lscenario.intervals.at(id);
    CHECK(litv.muted());
    CHECK(litv.soloed());
    CHECK(!lroot.muted());
    CHECK(!lroot.soloed());
  });
}

TEST_CASE("A solo silences the buses it does not involve", "[integration][interval][solo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& sdoc = scenarioDoc(*doc);
    auto& root = sdoc.baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    // a and b side by side at the root; c inside a.
    auto& a = addInterval(*doc, scenario, 1000, 0.2);
    auto& b = addInterval(*doc, scenario, 1000, 0.6);
    auto& inner = addScenario(*doc, a);
    auto& c = addInterval(*doc, inner, 500, 0.4);
    for(auto* itv : {&a, &b, &c})
      makeBus(*doc, *itv);

    auto silenced = [&] {
      return std::array{a.soloMuted(), b.soloMuted(), c.soloMuted()};
    };
    CHECK(silenced() == std::array{false, false, false});

    SECTION("a solo silences the other buses")
    {
      solo(*doc, b, true);
      CHECK(silenced() == std::array{true, false, true});
      CHECK(a.effectivelyMuted());
      CHECK(!a.muted());
      CHECK(a.executionState() == Scenario::IntervalExecutionState::Muted);

      solo(*doc, b, false);
      CHECK(silenced() == std::array{false, false, false});
    }

    SECTION("a bus holding a soloed bus stays heard")
    {
      solo(*doc, c, true);
      CHECK(silenced() == std::array{false, true, false});
    }

    SECTION("a bus inside a soloed bus stays heard")
    {
      solo(*doc, a, true);
      CHECK(silenced() == std::array{false, true, false});
    }

    SECTION("undoing a solo lifts the silence")
    {
      solo(*doc, b, true);
      doc->commandStack().undo();
      CHECK(silenced() == std::array{false, false, false});
    }

    SECTION("a bus that stops being a bus is heard again")
    {
      solo(*doc, b, true);
      CommandDispatcher<> disp{doc->context().commandStack};
      disp.submit<Scenario::Command::SetBus>(sdoc, a, false);
      CHECK(!a.soloMuted());
      CHECK(c.soloMuted());
    }
  });
}

TEST_CASE("An interval's upmix is one undoable step and is saved", "[integration][interval][upmix]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& root = scenarioDoc(*doc).baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());
    auto& itv = addInterval(*doc, scenario, 1000, 0.3);
    const auto id = itv.id();
    auto& out = *itv.outlet;

    {
      MacroCommandDispatcher<Process::SetUpmix> disp{doc->context().commandStack};
      disp.submit(new Process::SetUpmixMode{out, 1});
      disp.submit(new Process::SetUpmixChannels{out, 2});
      disp.commit();
    }
    CHECK(out.upmixMode() == 1);
    CHECK(out.upmixChannels() == 2);

    doc->commandStack().undo();
    CHECK(out.upmixMode() == 0);
    CHECK(out.upmixChannels() == 0);
    doc->commandStack().redo();
    CHECK(out.upmixChannels() == 2);

    auto* loaded = loadJson(ctx, saveAsJson(*doc));
    REQUIRE(loaded);
    auto& lroot = scenarioDoc(*loaded).baseInterval();
    auto& lscenario = static_cast<Scenario::ProcessModel&>(*lroot.processes.begin());
    auto& lout = *lscenario.intervals.at(id).outlet;
    CHECK(lout.upmixMode() == 1);
    CHECK(lout.upmixChannels() == 2);
  });
}
