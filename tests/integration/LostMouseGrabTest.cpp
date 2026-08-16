// Regression tests for the three symptoms of a drag that never gets its
// mouse release.
//
// QGraphicsScenePrivate::sendMouseEvent drops the implicit grab, without
// delivering anything, as soon as a *button-less* move arrives while an item
// holds it -- which is what happens when the button is let go outside the
// window and the pointer then comes back in. The item gets QEvent::UngrabMouse
// and never a mouseReleaseEvent.
//
// Each TEST_CASE below reproduces that and pins one consequence:
//  - the control keeps `moving` set and never commits its ongoing command;
//  - the next control the user touches has its edits redirected onto the first
//    one, because SetControlValue::update() only refreshes the value and not
//    the path;
//  - a node whose *port* is selected reads as selected but is not in the
//    selection stack, so dragging it moved nothing.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <Process/Commands/SetControlValue.hpp>

#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsMultiSliderXY.hpp>
#include <score/graphics/widgets/QGraphicsPathGeneratorXY.hpp>
#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
// The current "LFO" -- five float knobs plus an enum and a combo, i.e. exactly
// the controls the bug was reported on.
const auto lfo_key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("1e17e479-3513-44c8-a8a7-017be9f6ac8a"));

Process::ProcessModel& add_lfo(score::Document& doc, Scenario::IntervalModel& interval)
{
  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, lfo_key, QString{}, QPointF{});

  Process::ProcessModel* added = nullptr;
  for(auto& p : interval.processes)
    if(p.concreteKey() == lfo_key)
      added = &p;

  REQUIRE(added != nullptr);
  return *added;
}

std::vector<Process::ControlInlet*> controls(Process::ProcessModel& proc)
{
  std::vector<Process::ControlInlet*> out;
  for(auto* inl : proc.inlets())
    if(auto* ctl = qobject_cast<Process::ControlInlet*>(inl))
      out.push_back(ctl);
  return out;
}

// QGraphicsScene::event is public, but the mouse handlers are not: going
// through event() is what puts the scene's own grabber bookkeeping in play,
// which is the whole point here.
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

void sendMouse(
    Scene& scene, QEvent::Type type, QPointF scenePos, QPointF downScenePos,
    QPointF lastScenePos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
  QGraphicsSceneMouseEvent ev{type};
  ev.setScenePos(scenePos);
  ev.setLastScenePos(lastScenePos);
  ev.setButtonDownScenePos(Qt::LeftButton, downScenePos);

  // Screen coordinates drive InfiniteScroller; offset so the wrap-at-the-edge
  // path never triggers.
  const QPoint screen = (scenePos + QPointF{600, 400}).toPoint();
  const QPoint lastScreen = (lastScenePos + QPointF{600, 400}).toPoint();
  const QPoint downScreen = (downScenePos + QPointF{600, 400}).toPoint();
  ev.setScreenPos(screen);
  ev.setLastScreenPos(lastScreen);
  ev.setButtonDownScreenPos(Qt::LeftButton, downScreen);

  ev.setButton(button);
  ev.setButtons(buttons);
  ev.setModifiers(Qt::NoModifier);
  QCoreApplication::sendEvent(&scene, &ev);
}

void press(Scene& scene, QPointF at)
{
  sendMouse(
      scene, QEvent::GraphicsSceneMousePress, at, at, at, Qt::LeftButton,
      Qt::LeftButton);
}

void drag(Scene& scene, QPointF from, QPointF to, QPointF down)
{
  sendMouse(
      scene, QEvent::GraphicsSceneMouseMove, to, down, from, Qt::NoButton,
      Qt::LeftButton);
}

void release(Scene& scene, QPointF at, QPointF down)
{
  sendMouse(
      scene, QEvent::GraphicsSceneMouseRelease, at, down, at, Qt::LeftButton,
      Qt::NoButton);
}

//! The pointer coming back into the window after the button was released
//! outside it: a move with no buttons held. The scene answers by clearing the
//! grabber and returning, so the grabbing item never sees a release.
void loseGrab(Scene& scene, QPointF at)
{
  sendMouse(
      scene, QEvent::GraphicsSceneMouseMove, at, at, at, Qt::NoButton, Qt::NoButton);
}

// Straight to one item, bypassing the scene's hit-testing: a node is covered
// by its own controls and ports, so a scene-level press never reaches it.
void sendToItem(
    Scene& scene, QGraphicsItem& item, QEvent::Type type, QPointF pos, QPointF scenePos,
    QPointF downScenePos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
  QGraphicsSceneMouseEvent ev{type};
  ev.setPos(pos);
  ev.setScenePos(scenePos);
  ev.setLastScenePos(scenePos);
  ev.setButtonDownScenePos(Qt::LeftButton, downScenePos);
  ev.setScreenPos(scenePos.toPoint());
  ev.setLastScreenPos(scenePos.toPoint());
  ev.setButtonDownScreenPos(Qt::LeftButton, downScenePos.toPoint());
  ev.setButton(button);
  ev.setButtons(buttons);
  ev.setModifiers(Qt::NoModifier);
  scene.sendEvent(&item, &ev);
}

double asDouble(const ossia::value& v)
{
  return ossia::convert<double>(v);
}
}

TEST_CASE(
    "a button-less move takes the grab away without a release",
    "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsKnob knob{nullptr};
    knob.setRange(0., 1., 0.);
    knob.setValue(0.5);
    scene.addItem(&knob);
    score::InfiniteScroller::cancel();

    int released = 0;
    QObject::connect(
        &knob, &score::QGraphicsKnob::sliderReleased, &knob, [&] { released++; });

    const QPointF centre = knob.mapToScene(knob.boundingRect().center());
    press(scene, centre);
    REQUIRE(scene.mouseGrabberItem() == &knob);

    drag(scene, centre, centre - QPointF{0, 40}, centre);
    REQUIRE(released == 0);

    loseGrab(scene, centre + QPointF{300, 300});

    // Qt dropped the grab silently. Before the fix the knob never heard about
    // it and stayed mid-edit for the rest of the session.
    CHECK(scene.mouseGrabberItem() == nullptr);
    CHECK(released == 1);

    // ... and closing it is idempotent: the ungrab Qt sends after every normal
    // release must not emit a second one.
    press(scene, centre);
    drag(scene, centre, centre - QPointF{0, 10}, centre);
    release(scene, centre - QPointF{0, 10}, centre);
    CHECK(released == 2);
    loseGrab(scene, centre + QPointF{300, 300});
    CHECK(released == 2);

    scene.removeItem(&knob);
  });
}

// The combo is the widget bug 3 was reported on, and it closes its drag through
// its own DefaultComboImpl rather than the shared slider/knob helpers.
TEST_CASE(
    "a combo box also ends its edit when the grab is taken away",
    "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo combo{QStringList{"a", "b", "c", "d"}, nullptr};
    scene.addItem(&combo);
    combo.setValue(0);
    score::InfiniteScroller::cancel();

    int released = 0;
    QObject::connect(
        &combo, &score::QGraphicsCombo::sliderReleased, &combo, [&] { released++; });

    const QPointF centre = combo.mapToScene(combo.boundingRect().center());
    press(scene, centre);
    REQUIRE(scene.mouseGrabberItem() == &combo);
    drag(scene, centre, centre - QPointF{0, 40}, centre);
    REQUIRE(released == 0);

    loseGrab(scene, centre + QPointF{300, 300});
    CHECK(released == 1);

    scene.removeItem(&combo);
  });
}

// Pins the dispatcher on its own, with no widget in the picture: the test below
// goes through real control items and would still pass on the strength of the
// widget-side cleanup alone, so a regression here has to be caught separately.
namespace
{
//! Press, drag, then have the scene take the grab away without a release, and
//! check the control closed its edit exactly once -- and that the ungrab Qt
//! sends after a *normal* release does not close it a second time.
template <typename T, typename Setup>
void checksLostGrab(Setup&& setup)
{
  Scene scene;
  T item{nullptr};
  setup(item);
  scene.addItem(&item);
  score::InfiniteScroller::cancel();

  int released = 0;
  QObject::connect(&item, &T::sliderReleased, &item, [&] { released++; });

  // sceneBoundingRect(): boundingRect() is private on several of these.
  const QPointF centre = item.sceneBoundingRect().center();

  press(scene, centre);
  REQUIRE(scene.mouseGrabberItem() == &item);
  drag(scene, centre, centre - QPointF{0, 30}, centre);
  REQUIRE(released == 0);

  loseGrab(scene, centre + QPointF{400, 400});
  CHECK(scene.mouseGrabberItem() == nullptr);
  CHECK(released == 1);

  press(scene, centre);
  drag(scene, centre, centre - QPointF{0, 10}, centre);
  release(scene, centre - QPointF{0, 10}, centre);
  const int afterRelease = released;
  loseGrab(scene, centre + QPointF{400, 400});
  CHECK(released == afterRelease);

  scene.removeItem(&item);
}
}

TEST_CASE(
    "every draggable control closes its edit on a lost grab",
    "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    SECTION("spinbox")
    {
      checksLostGrab<score::QGraphicsSpinbox>(
          [](auto& i) { i.setRange(0., 1., 0.5); });
    }

    // Guards on `handle`, which the widget owns, rather than on `moving`, which
    // only its consumers write.
    SECTION("range slider")
    {
      checksLostGrab<score::QGraphicsRangeSlider>(
          [](auto& i) { i.setRange(0., 1., ossia::vec2f{0.2f, 0.8f}); });
    }

    // These two used to declare `int m_grab{-1}` and test it as a bool.
    SECTION("multi slider xy")
    {
      checksLostGrab<score::QGraphicsMultiSliderXY>([](auto&) { });
    }

    SECTION("path generator xy")
    {
      checksLostGrab<score::QGraphicsPathGeneratorXY>([](auto&) { });
    }
  });
}

TEST_CASE(
    "an unfinished ongoing command is closed rather than retargeted",
    "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& lfo = add_lfo(*doc, interval);

    const auto ctls = controls(lfo);
    REQUIRE(ctls.size() >= 2);
    auto& a = *ctls[0];
    auto& b = *ctls[1];

    auto& disp = doc->context().dispatcher;

    // An edit of A that never gets its commit(), exactly as a lost mouse
    // release leaves things.
    disp.submit<Process::SetControlValue>(a, ossia::value{0.25f});
    const auto aLeftAt = asDouble(a.value());

    // B's edit must build its own command, not update() A's.
    disp.submit<Process::SetControlValue>(b, ossia::value{0.75f});
    disp.commit();

    CHECK(asDouble(b.value()) == 0.75);
    CHECK(asDouble(a.value()) == aLeftAt);
  });
}

TEST_CASE(
    "an abandoned control edit does not swallow the next control's edit",
    "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& lfo = add_lfo(*doc, interval);

    const auto ctls = controls(lfo);
    REQUIRE(ctls.size() >= 2);
    auto& a = *ctls[0];
    auto& b = *ctls[1];

    auto& portFactories = ctx.interfaces<Process::PortFactoryList>();
    auto* fa = portFactories.get(a.concreteKey());
    auto* fb = portFactories.get(b.concreteKey());
    REQUIRE(fa != nullptr);
    REQUIRE(fb != nullptr);

    QObject holder;
    Scene scene;
    score::InfiniteScroller::cancel();

    auto* itemA = fa->makeControlItem(a, doc->context(), nullptr, &holder);
    auto* itemB = fb->makeControlItem(b, doc->context(), nullptr, &holder);
    REQUIRE(itemA != nullptr);
    REQUIRE(itemB != nullptr);
    scene.addItem(itemA);
    scene.addItem(itemB);
    itemA->setPos(0., 0.);
    itemB->setPos(300., 0.);

    const double a0 = asDouble(a.value());
    const double b0 = asDouble(b.value());

    // Drag A, and lose the grab before it ever gets a release.
    const QPointF ca = itemA->mapToScene(itemA->boundingRect().center());
    press(scene, ca);
    drag(scene, ca, ca - QPointF{0, 60}, ca);
    const double aDragged = asDouble(a.value());
    REQUIRE(aDragged != a0);

    loseGrab(scene, ca + QPointF{600, 600});

    // Now drag B to completion.
    const QPointF cb = itemB->mapToScene(itemB->boundingRect().center());
    press(scene, cb);
    drag(scene, cb, cb - QPointF{0, 60}, cb);
    release(scene, cb - QPointF{0, 60}, cb);

    // Without the fix the ongoing SetControlValue left open by A was reused:
    // update() refreshes only the value, so B's drag wrote into A's inlet --
    // B's widget moved while A's model value (and execution value) followed.
    CHECK(asDouble(b.value()) != b0);
    CHECK(asDouble(a.value()) == aDragged);

    scene.removeItem(itemA);
    scene.removeItem(itemB);
    delete itemA;
    delete itemB;
  });
}

TEST_CASE(
    "a node whose port is selected can still be dragged", "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& lfo = add_lfo(*doc, interval);

    const auto ctls = controls(lfo);
    REQUIRE(!ctls.empty());

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    Scene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(root);
    auto* node
        = new Process::NodeItem{lfo, pctx, TimeVal::fromMsecs(1000.), root};
    node->setPos(50., 50.);
    QCoreApplication::processEvents();

    // Selecting a port of the process marks the *process* selected -- that is
    // what draws the node's frame -- while the selection stack holds only the
    // port. Any behaviour keyed off the flag rather than the stack breaks here.
    doc->context().selectionStack.pushNewSelection(Selection{ctls.front()});
    QCoreApplication::processEvents();

    REQUIRE(lfo.selection.get());
    REQUIRE(!doc->context().selectionStack.currentSelection().contains(&lfo));

    const QPointF before = lfo.position();

    // Drag the node body. Before the fix the press deferred selection on the
    // flag, so the move submitted MoveNodes over an empty process list and the
    // node simply did not budge.
    const QPointF hit{4., 4.};
    const QPointF down = node->mapToScene(hit);
    const QPointF to = down + QPointF{60., 40.};
    sendToItem(
        scene, *node, QEvent::GraphicsSceneMousePress, hit, down, down, Qt::LeftButton,
        Qt::LeftButton);
    sendToItem(
        scene, *node, QEvent::GraphicsSceneMouseMove, hit + QPointF{60., 40.}, to, down,
        Qt::NoButton, Qt::LeftButton);
    sendToItem(
        scene, *node, QEvent::GraphicsSceneMouseRelease, hit + QPointF{60., 40.}, to,
        down, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();

    CHECK(lfo.position() != before);
    CHECK(doc->context().selectionStack.currentSelection().contains(&lfo));

    delete node;
    scene.removeItem(root);
    delete root;
  });
}

TEST_CASE(
    "a node drag that loses its grab is committed, not carried into the next one",
    "[integration][gui][controls]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& lfo = add_lfo(*doc, interval);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    Scene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(root);
    auto* node = new Process::NodeItem{lfo, pctx, TimeVal::fromMsecs(1000.), root};
    node->setPos(50., 50.);
    QCoreApplication::processEvents();

    const QPointF start = lfo.position();
    const QPointF hit{4., 4.};

    auto dragBy = [&](QPointF by) {
      const QPointF down = node->mapToScene(hit);
      sendToItem(
          scene, *node, QEvent::GraphicsSceneMousePress, hit, down, down, Qt::LeftButton,
          Qt::LeftButton);
      sendToItem(
          scene, *node, QEvent::GraphicsSceneMouseMove, hit + by, down + by, down,
          Qt::NoButton, Qt::LeftButton);
    };

    // First drag, abandoned: the scene takes the grab away with no release.
    const int commandsBefore = doc->commandStack().size();
    dragBy(QPointF{60., 40.});
    const QPointF afterFirst = lfo.position();
    REQUIRE(afterFirst != start);

    QEvent ungrab{QEvent::UngrabMouse};
    scene.sendEvent(node, &ungrab);

    // The move must have been committed *there*, not left open: the stack has
    // to have grown, and the node must not have moved doing it.
    CHECK(doc->commandStack().size() == commandsBefore + 1);
    CHECK(lfo.position() == afterFirst);

    // A second drag has to start a *fresh* MoveNodes. Without the commit above
    // it would update() the first one instead, which still holds the original
    // positions -- so the node would jump back and move relative to the first
    // press rather than from where it now sits.
    dragBy(QPointF{10., 10.});
    sendToItem(
        scene, *node, QEvent::GraphicsSceneMouseRelease, hit + QPointF{10., 10.},
        node->mapToScene(hit) + QPointF{10., 10.}, node->mapToScene(hit),
        Qt::LeftButton, Qt::NoButton);

    CHECK(lfo.position() == afterFirst + QPointF{10., 10.});

    delete node;
    scene.removeItem(root);
    delete root;
  });
}
