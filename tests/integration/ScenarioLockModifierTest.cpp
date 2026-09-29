// Integration test: dragging the start of an interval moves the whole interval,
// or with Shift held resizes it (lock mode); Ctrl held scales. What decides is
// the modifiers held at that moment, whatever received their key events: the
// scenario view, another item or widget that had the keyboard focus when a key
// was let go, or nothing because the creation of the interval was under way.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ExpandMode.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineEdit>

#include <catch2/catch_test_macros.hpp>

#include <ossia/detail/algorithms.hpp>

#include <functional>
#include <vector>

namespace
{
struct Fixture
{
  score::Document* doc{};
  Scenario::ProcessModel* scenario{};
  Scenario::ScenarioPresenter* pres{};
  QGraphicsScene* scene{};
  QWidget* graphicsView{};
  Scenario::EditionSettings* settings{};
  // Somewhere else that can have the keyboard focus: the inspector, say
  QLineEdit elsewhere;
};

void makeFixture(Fixture& f, const score::GUIApplicationContext& ctx)
{
  f.doc = score::test::new_document(ctx);
  REQUIRE(f.doc);
  auto& root = static_cast<Scenario::ScenarioDocumentModel&>(
                   f.doc->model().modelDelegate())
                   .baseInterval();
  f.scenario = &static_cast<Scenario::ProcessModel&>(*root.processes.begin());
  auto& plug = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
  f.pres = plug.focusedPresenter();
  REQUIRE(f.pres);
  REQUIRE(&f.pres->model() == f.scenario);
  f.settings = &plug.editionSettings();

  auto* docPres
      = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
          *f.doc);
  REQUIRE(docPres);
  f.scene = &docPres->view().scene();
  f.graphicsView = &docPres->view().view();

  // The window is not shown here: activate the scene as a shown, active view
  // does, so that the scenario has the scene's keyboard focus.
  QEvent activate{QEvent::WindowActivate};
  QCoreApplication::sendEvent(f.scene, &activate);
  f.pres->view().setFocus();
  REQUIRE(f.scene->focusItem() == &f.pres->view());

  REQUIRE(f.settings->lockMode() == LockMode::Free);
  REQUIRE(f.settings->expandMode() == ExpandMode::GrowShrink);
}

// A key event goes to the widget that has the focus.
void sendKey(QWidget& to, QEvent::Type type, int key, Qt::KeyboardModifiers mods)
{
  QKeyEvent ev{type, key, mods};
  QCoreApplication::sendEvent(&to, &ev);
  QApplication::processEvents();
}

void sendMouse(
    QGraphicsScene& scene, QEvent::Type type, QPointF pos, QPointF down,
    Qt::MouseButtons buttons, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  QGraphicsSceneMouseEvent ev{type};
  ev.setScenePos(pos);
  ev.setLastScenePos(pos);
  ev.setButtonDownScenePos(Qt::LeftButton, down);
  ev.setScreenPos(pos.toPoint());
  ev.setLastScreenPos(pos.toPoint());
  ev.setButtonDownScreenPos(Qt::LeftButton, down.toPoint());
  ev.setButton(type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
  ev.setButtons(buttons);
  ev.setModifiers(mods);
  QCoreApplication::sendEvent(&scene, &ev);
  QApplication::processEvents();
}

void drag(QGraphicsScene& scene, QPointF from, QPointF to)
{
  sendMouse(scene, QEvent::GraphicsSceneMousePress, from, from, Qt::LeftButton);
  sendMouse(scene, QEvent::GraphicsSceneMouseMove, (from + to) / 2., from, Qt::LeftButton);
  sendMouse(scene, QEvent::GraphicsSceneMouseMove, to, from, Qt::LeftButton);
  sendMouse(scene, QEvent::GraphicsSceneMouseRelease, to, from, Qt::NoButton);
}

// A new interval, made with the creation tool from a state in the void;
// `during` runs with the mouse button still down.
const Scenario::IntervalModel& createInterval(Fixture& f, std::function<void()> during)
{
  auto& sc = *f.scenario;
  std::vector<Id<Scenario::IntervalModel>> before;
  for(auto& itv : sc.intervals)
    before.push_back(itv.id());

  Id<Scenario::StateModel> dot;
  {
    Scenario::Command::Macro m{new Scenario::Command::CreateStateMacro, f.doc->context()};
    const auto pt = f.pres->toScenarioPoint(QPointF{300., 0.3 * f.pres->view().height()});
    dot = std::get<2>(m.createDot(sc, pt)).id();
    m.commit();
  }
  QApplication::processEvents();

  f.settings->setTool(Scenario::Tool::Create);
  const auto from = f.pres->state(dot).view()->scenePos();
  const auto to = from + QPointF{300., 0.};
  sendMouse(*f.scene, QEvent::GraphicsSceneMousePress, from, from, Qt::LeftButton);
  sendMouse(*f.scene, QEvent::GraphicsSceneMouseMove, (from + to) / 2., from, Qt::LeftButton);
  sendMouse(*f.scene, QEvent::GraphicsSceneMouseMove, to, from, Qt::LeftButton);
  if(during)
    during();
  sendMouse(*f.scene, QEvent::GraphicsSceneMouseRelease, to, from, Qt::NoButton);
  CHECK(f.settings->tool() == Scenario::Tool::Select);

  const Scenario::IntervalModel* created{};
  for(auto& itv : sc.intervals)
    if(!ossia::contains(before, itv.id()))
      created = &itv;
  REQUIRE(created);
  // Pressed on the dot's event or state: it starts there
  REQUIRE(Scenario::startState(*created, sc).eventId() == sc.state(dot).eventId());
  return *created;
}

enum class Expect
{
  MovesWhole,
  Resizes
};

// Drags the start state of the interval to the right.
void dragStartAndCheck(Fixture& f, const Scenario::IntervalModel& itv, Expect expect)
{
  auto& sc = *f.scenario;
  const auto start0 = Scenario::startEvent(itv, sc).date();
  const auto end0 = Scenario::endEvent(itv, sc).date();

  auto* view = f.pres->state(itv.startState()).view();
  REQUIRE(view);
  const auto from = view->scenePos();
  const auto to = from + QPointF{60., 0.};
  drag(*f.scene, from, to);

  const auto start1 = Scenario::startEvent(itv, sc).date();
  const auto end1 = Scenario::endEvent(itv, sc).date();
  REQUIRE(start1 > start0);
  switch(expect)
  {
    case Expect::MovesWhole:
      CHECK(end1 - start1 == end0 - start0);
      CHECK(end1 > end0);
      break;
    case Expect::Resizes:
      CHECK(end1 == end0);
      CHECK(end1 - start1 < end0 - start0);
      break;
  }
}

void pressShift(QWidget& to)
{
  sendKey(to, QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
}
void releaseShift(QWidget& to)
{
  sendKey(to, QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
}
}

TEST_CASE(
    "Dragging the start of a new interval follows the modifiers held",
    "[integration][scenario][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    SECTION("nothing held: the whole interval moves")
    {
      Fixture f;
      makeFixture(f, ctx);
      auto& itv = createInterval(f, {});
      dragStartAndCheck(f, itv, Expect::MovesWhole);
    }

    SECTION("Shift held in the scenario: the interval is resized")
    {
      Fixture f;
      makeFixture(f, ctx);
      auto& itv = createInterval(f, {});
      pressShift(*f.graphicsView);
      CHECK(f.settings->lockMode() == LockMode::Constrained);
      dragStartAndCheck(f, itv, Expect::Resizes);
      releaseShift(*f.graphicsView);
      CHECK(f.settings->lockMode() == LockMode::Free);
    }

    SECTION("Shift let go while another widget has the focus")
    {
      Fixture f;
      makeFixture(f, ctx);
      auto& itv = createInterval(f, {});
      pressShift(*f.graphicsView);
      releaseShift(f.elsewhere);
      CHECK(f.settings->lockMode() == LockMode::Free);
      dragStartAndCheck(f, itv, Expect::MovesWhole);
    }

    SECTION("Shift held through the creation, let go in the scenario")
    {
      Fixture f;
      makeFixture(f, ctx);
      pressShift(*f.graphicsView);
      auto& itv = createInterval(f, {});
      releaseShift(*f.graphicsView);
      CHECK(f.settings->lockMode() == LockMode::Free);
      dragStartAndCheck(f, itv, Expect::MovesWhole);
    }

    SECTION("Shift held through the creation and still held")
    {
      Fixture f;
      makeFixture(f, ctx);
      pressShift(*f.graphicsView);
      auto& itv = createInterval(f, {});
      CHECK(f.settings->lockMode() == LockMode::Constrained);
      dragStartAndCheck(f, itv, Expect::Resizes);
      releaseShift(*f.graphicsView);
    }

    SECTION("Shift pressed during the creation, let go elsewhere")
    {
      Fixture f;
      makeFixture(f, ctx);
      auto& itv = createInterval(f, [&] { pressShift(*f.graphicsView); });
      releaseShift(f.elsewhere);
      CHECK(f.settings->lockMode() == LockMode::Free);
      dragStartAndCheck(f, itv, Expect::MovesWhole);
    }

    SECTION("Shift pressed elsewhere, held during the drag")
    {
      Fixture f;
      makeFixture(f, ctx);
      auto& itv = createInterval(f, {});
      pressShift(f.elsewhere);
      CHECK(f.settings->lockMode() == LockMode::Constrained);
      dragStartAndCheck(f, itv, Expect::Resizes);
      releaseShift(f.elsewhere);
    }

    SECTION("Ctrl let go while another widget has the focus")
    {
      Fixture f;
      makeFixture(f, ctx);
      auto& itv = createInterval(f, {});
      sendKey(*f.graphicsView, QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
      CHECK(f.settings->expandMode() == ExpandMode::Scale);
      sendKey(f.elsewhere, QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
      CHECK(f.settings->expandMode() == ExpandMode::GrowShrink);
      dragStartAndCheck(f, itv, Expect::MovesWhole);
    }
  });
}
