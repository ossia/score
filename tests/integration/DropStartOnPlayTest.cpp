// Alt while dropping in a scenario drops without magnetism and gives the new
// time sync a trigger with start-on-play, exactly like a double-click in the
// scenario does. It holds both for drops that make an interval and for drops
// that make a state.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <State/MessageListSerialization.hpp>

#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QMimeData>

#include <qpa/qwindowsysteminterface.h>

#include <catch2/catch_test_macros.hpp>

#include <ossia/detail/algorithms.hpp>

#include <vector>

namespace
{
// The drop handlers read qApp->keyboardModifiers(), which only the platform
// layer writes: a synthesized QKeyEvent would not reach it.
void holdModifier(Qt::KeyboardModifier mod, int key, bool held)
{
  const auto mods = held ? mod : Qt::NoModifier;
  QWindowSystemInterface::handleKeyEvent(
      nullptr, held ? QEvent::KeyPress : QEvent::KeyRelease, key, mods);
  QWindowSystemInterface::flushWindowSystemEvents();
  REQUIRE(QGuiApplication::keyboardModifiers() == mods);
}

void holdAlt(bool held)
{
  holdModifier(Qt::AltModifier, Qt::Key_Alt, held);
}

void holdCtrl(bool held)
{
  holdModifier(Qt::ControlModifier, Qt::Key_Control, held);
}

std::vector<Id<Scenario::IntervalModel>> intervalIds(const Scenario::ProcessModel& sc)
{
  std::vector<Id<Scenario::IntervalModel>> res;
  for(auto& itv : sc.intervals)
    res.push_back(itv.id());
  return res;
}

std::vector<Id<Scenario::StateModel>> stateIds(const Scenario::ProcessModel& sc)
{
  std::vector<Id<Scenario::StateModel>> res;
  for(auto& st : sc.states)
    res.push_back(st.id());
  return res;
}

std::vector<const Scenario::TimeSyncModel*>
triggers(const Scenario::ProcessModel& scenario)
{
  std::vector<const Scenario::TimeSyncModel*> res;
  for(auto& ts : scenario.timeSyncs)
    if(ts.active())
      res.push_back(&ts);
  return res;
}

void doubleClick(Scenario::ScenarioPresenter& pres, QPointF pos)
{
  auto& view = pres.view();
  QGraphicsSceneMouseEvent ev{QEvent::GraphicsSceneMouseDoubleClick};
  ev.setPos(pos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
  view.scene()->sendEvent(&view, &ev);
  QApplication::processEvents();
}

void sendDrop(Scenario::ScenarioPresenter& pres, QPointF pos, QMimeData& mime)
{
  auto& view = pres.view();
  QGraphicsSceneDragDropEvent ev{QEvent::GraphicsSceneDrop};
  ev.setPos(pos);
  ev.setMimeData(&mime);
  ev.setModifiers(QGuiApplication::keyboardModifiers());
  view.scene()->sendEvent(&view, &ev);
  QApplication::processEvents();
}

void dropProcess(Scenario::ScenarioPresenter& pres, QPointF pos)
{
  QMimeData mime;
  Mime<Process::ProcessData>::Serializer s{mime};
  s.serialize(Process::ProcessData{
      Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), QStringLiteral("Scenario"),
      {}});

  sendDrop(pres, pos, mime);
}

void dropMessages(Scenario::ScenarioPresenter& pres, QPointF pos)
{
  State::Message msg;
  msg.address = State::AddressAccessor{State::Address{"dev", {"foo"}}};
  msg.value = 1.f;

  QMimeData mime;
  Mime<State::MessageList>::Serializer s{mime};
  s.serialize(State::MessageList{msg});

  sendDrop(pres, pos, mime);
}
}

TEST_CASE(
    "Alt while dropping a process makes it start on play",
    "[integration][scenario][drop][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    auto* pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
                     .focusedPresenter();
    REQUIRE(pres);
    REQUIRE(&pres->model() == &scenario);

    auto& stack = doc->commandStack();
    const auto intervals_before = scenario.intervals.size();
    const QPointF pos{300., 60.};
    REQUIRE(triggers(scenario).empty());

    // A plain drop creates the interval and leaves every sync untriggered.
    holdAlt(false);
    dropProcess(*pres, pos);

    CHECK(scenario.intervals.size() > intervals_before);
    CHECK(triggers(scenario).empty());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(scenario.intervals.size() == intervals_before);

    // Ctrl is not a modifier the drop reads.
    holdCtrl(true);
    dropProcess(*pres, pos);
    holdCtrl(false);

    CHECK(scenario.intervals.size() > intervals_before);
    CHECK(triggers(scenario).empty());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(scenario.intervals.size() == intervals_before);

    // A double-click: the reference behaviour.
    doubleClick(*pres, pos);

    const auto clicked = triggers(scenario);
    REQUIRE(clicked.size() == 1);
    CHECK(clicked.front()->active());
    CHECK(clicked.front()->isStartPoint());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(triggers(scenario).empty());

    // The same, obtained by holding alt during the drop.
    const auto before = intervalIds(scenario);
    holdAlt(true);
    dropProcess(*pres, pos);
    holdAlt(false);

    REQUIRE(scenario.intervals.size() == intervals_before + 1);
    const Scenario::IntervalModel* itv{};
    for(auto& i : scenario.intervals)
      if(!ossia::contains(before, i.id()))
        itv = &i;
    REQUIRE(itv);
    auto& sync = Scenario::startTimeSync(*itv, scenario);

    CHECK(sync.active());
    CHECK(sync.isStartPoint());

    const auto dropped_trig = triggers(scenario);
    REQUIRE(dropped_trig.size() == 1);
    CHECK(dropped_trig.front() == &sync);

    // One undo takes the whole drop away, trigger included.
    stack.undo();
    QApplication::processEvents();
    CHECK(scenario.intervals.size() == intervals_before);
    CHECK(triggers(scenario).empty());
  });
}

TEST_CASE(
    "Alt while dropping messages makes the state start on play",
    "[integration][scenario][drop][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    auto* pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
                     .focusedPresenter();
    REQUIRE(pres);

    auto& stack = doc->commandStack();
    const auto states_before = scenario.states.size();
    const QPointF pos{300., 60.};
    REQUIRE(triggers(scenario).empty());

    // A plain drop creates the state and leaves every sync untriggered.
    holdAlt(false);
    dropMessages(*pres, pos);

    CHECK(scenario.states.size() > states_before);
    CHECK(triggers(scenario).empty());

    stack.undo();
    QApplication::processEvents();
    REQUIRE(scenario.states.size() == states_before);

    const auto before = stateIds(scenario);
    holdAlt(true);
    dropMessages(*pres, pos);
    holdAlt(false);

    REQUIRE(scenario.states.size() == states_before + 1);
    const Scenario::StateModel* st{};
    for(auto& s : scenario.states)
      if(!ossia::contains(before, s.id()))
        st = &s;
    REQUIRE(st);
    auto& sync = Scenario::parentTimeSync(*st, scenario);

    CHECK(sync.active());
    CHECK(sync.isStartPoint());

    const auto dropped_trig = triggers(scenario);
    REQUIRE(dropped_trig.size() == 1);
    CHECK(dropped_trig.front() == &sync);

    stack.undo();
    QApplication::processEvents();
    CHECK(scenario.states.size() == states_before);
    CHECK(triggers(scenario).empty());
  });
}
