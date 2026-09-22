// Integration test: what a click on a state hits, and what a double-click in
// the scenario leaves selected.
//
// A state is the small round handle at the end of an interval. It is drawn on
// top of the interval, and a press there must reach it whatever the interval
// is doing: showing its rack, or selected.

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <ossia/detail/algorithms.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// The current "LFO": adding it to an interval gives the interval a slot, hence
// a shown rack.
const auto lfo_key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("1e17e479-3513-44c8-a8a7-017be9f6ac8a"));

Scenario::StateView* viewFor(QGraphicsScene& scene, const Scenario::StateModel& st)
{
  for(auto* item : scene.items())
    if(auto* v = dynamic_cast<Scenario::StateView*>(item))
      if(&v->presenter().model() == &st)
        return v;
  return nullptr;
}

QGraphicsItem* viewOfType(QGraphicsScene& scene, int type)
{
  for(auto* item : scene.items())
    if(item->type() == type)
      return item;
  return nullptr;
}

void doubleClick(QGraphicsScene& scene, QPointF at)
{
  for(auto type :
      {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease,
       QEvent::GraphicsSceneMouseDoubleClick, QEvent::GraphicsSceneMouseRelease})
  {
    QGraphicsSceneMouseEvent ev{type};
    ev.setScenePos(at);
    ev.setLastScenePos(at);
    ev.setButtonDownScenePos(Qt::LeftButton, at);
    ev.setScreenPos(at.toPoint());
    ev.setLastScreenPos(at.toPoint());
    ev.setButtonDownScreenPos(Qt::LeftButton, at.toPoint());
    ev.setButton(Qt::LeftButton);
    ev.setButtons(
        type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
    QCoreApplication::sendEvent(&scene, &ev);
  }
}
}

TEST_CASE("States sort above every interval", "[integration][scenario][gui]")
{
  using Z = Scenario::ZPos;
  CHECK(Z::State > Z::Interval);
  CHECK(Z::State > Z::IntervalWithRack);
  CHECK(Z::State > Z::SelectedInterval);
  CHECK(Z::State > Z::SelectedEvent);
  CHECK(Z::State > Z::SelectedTimeSync);
  CHECK(Z::SelectedState >= Z::State);

  // The intervals keep their own order amongst themselves
  CHECK(Z::Interval < Z::IntervalWithRack);
  CHECK(Z::IntervalWithRack < Z::SelectedInterval);
}

TEST_CASE("A press on a state reaches it, whatever the interval does",
          "[integration][scenario][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    CommandDispatcher<> dispatcher{doc->context().commandStack};
    auto* createStart = new Scenario::Command::CreateTimeSync_Event_State{
        scenario, TimeVal::fromMsecs(1000), 0.4};
    dispatcher.submit(createStart);
    auto* createInterval = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenario, createStart->createdState(), TimeVal::fromMsecs(4000), 0.4, false};
    dispatcher.submit(createInterval);

    auto& interval = scenario.intervals.at(createInterval->createdInterval());
    auto& startState = scenario.states.at(createStart->createdState());
    auto& endState = scenario.states.at(createInterval->createdState());

    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      m.createProcessInNewSlot(interval, lfo_key, QString{});
      m.commit();
    }
    QApplication::processEvents();
    REQUIRE(interval.smallViewVisible());

    auto* presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter);
    auto& scene = presenter->view().scene();

    auto probe = [&](const Scenario::StateModel& st) {
      auto* view = viewFor(scene, st);
      REQUIRE(view);
      // The visual area of the dot: its drawn radius, at the cardinal points
      // and the centre.
      const double r = Scenario::StateView::pointRadius - 1.;
      for(QPointF off :
          {QPointF{0, 0}, QPointF{r, 0}, QPointF{-r, 0}, QPointF{0, r}, QPointF{0, -r},
           QPointF{r / 2, r / 2}, QPointF{-r / 2, -r / 2}})
      {
        INFO("offset " << off.x() << ", " << off.y());
        CHECK(scene.itemAt(view->scenePos() + off, QTransform{}) == view);
      }
    };

    // An interval showing its rack
    probe(startState);
    probe(endState);

    // A selected interval
    score::SelectionDispatcher{doc->context().selectionStack}.select(interval);
    QApplication::processEvents();
    probe(startState);
    probe(endState);
  });
}

TEST_CASE("Double-clicking the scenario selects the state it creates",
          "[integration][scenario][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());

    auto* presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter);
    auto& scene = presenter->view().scene();

    std::vector<Id<Scenario::StateModel>> before;
    for(auto& st : scenario.states)
      before.push_back(st.id());

    // A point on the scenario layer itself, away from anything else
    QPointF target;
    {
      auto* layer = viewOfType(scene, Scenario::ItemType::ScenarioProcess);
      REQUIRE(layer);
      const auto r = layer->sceneBoundingRect();
      target = {r.left() + 150., r.top() + 0.4 * r.height()};
      REQUIRE(scene.itemAt(target, QTransform{}) == layer);
    }

    doubleClick(scene, target);
    QApplication::processEvents();

    const Scenario::StateModel* created = nullptr;
    for(auto& st : scenario.states)
      if(ossia::find(before, st.id()) == before.end())
        created = &st;
    REQUIRE(created);

    CHECK(created->selection.get());

    const Selection sel = doc->context().selectionStack.currentSelection();
    REQUIRE(sel.size() == 1);
    CHECK(sel.at(0) == created);
  });
}
