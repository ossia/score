// The mouse on the monitoring views: the mode switch steps through a list,
// one mode per wheel notch, as one undoable edit each; the resize corner of a
// node without outlets resizes it, and ends its edit when the scene takes the
// grab away without a release.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Monitoring/ModeSwitch.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
Process::ProcessModel&
add_process(score::Document& doc, Scenario::IntervalModel& interval, const char* uuid)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(QString::fromLatin1(uuid));
  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, key, QString{}, QPointF{});

  Process::ProcessModel* added = nullptr;
  for(auto& p : interval.processes)
    if(p.concreteKey() == key)
      added = &p;
  REQUIRE(added != nullptr);
  return *added;
}

Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

// LFO: it has a combo box among its controls
constexpr const char* lfo_uuid = "1e17e479-3513-44c8-a8a7-017be9f6ac8a";
// LED View: a fully custom view, with no outlet
constexpr const char* led_view_uuid = "C618774D-086F-4888-8404-23B6E59440F8";

void sendMouse(
    QGraphicsScene& scene, QEvent::Type type, QPointF scenePos, QPointF downScenePos,
    Qt::MouseButton button, Qt::MouseButtons buttons)
{
  QGraphicsSceneMouseEvent ev{type};
  ev.setScenePos(scenePos);
  ev.setLastScenePos(scenePos);
  ev.setButtonDownScenePos(Qt::LeftButton, downScenePos);
  ev.setScreenPos(scenePos.toPoint());
  ev.setLastScreenPos(scenePos.toPoint());
  ev.setButtonDownScreenPos(Qt::LeftButton, downScenePos.toPoint());
  ev.setButton(button);
  ev.setButtons(buttons);
  ev.setModifiers(Qt::NoModifier);
  QCoreApplication::sendEvent(&scene, &ev);
}
}

TEST_CASE(
    "mode switch: the stepper goes through the list, wrapping, one edit each",
    "[ui][monitor][mode_switch]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_process(*doc, base_interval(*doc), lfo_uuid);

    Process::ComboBox* combo{};
    for(auto* inl : lfo.inlets())
      if(auto* c = qobject_cast<Process::ComboBox*>(inl); c && c->alternatives.size() > 2)
        combo = c;
    REQUIRE(combo != nullptr);
    const int n = int(combo->alternatives.size());
    auto index = [&] {
      for(int i = 0; i < n; i++)
        if(combo->alternatives[i].second == combo->value())
          return i;
      return -1;
    };

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QObject owner;
    std::unique_ptr<score::QGraphicsCombo> sw{
        Process::ModeSwitch::make(combo, pctx, nullptr, &owner)};
    REQUIRE(sw);

    auto& stack = doc->commandStack();
    const int start = index();
    REQUIRE(start >= 0);
    const int commands = stack.size();

    sw->step(1);
    CHECK(index() == (start + 1) % n);
    CHECK(stack.size() == commands + 1);

    // Backwards past the first wraps around to the last
    sw->step(-2);
    CHECK(index() == ((start - 1) % n + n) % n);

    // Each step is an edit of its own
    const int edits = stack.size() - commands;
    CHECK(edits == 2);
    for(int i = 0; i < edits; i++)
      stack.undoQuiet();
    CHECK(index() == start);
  });
}

TEST_CASE(
    "node resize corner: resizes, and a lost grab ends the edit",
    "[ui][monitor][nodal][resize_corner]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& led = add_process(*doc, base_interval(*doc), led_view_uuid);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};

    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 2000., 2000.}};
    scene.addItem(root);
    auto* node = new Process::NodeItem{led, pctx, TimeVal::fromMsecs(1000.), root};
    node->setPos(100., 100.);
    QCoreApplication::processEvents();

    // The corner: the one child that only takes the left button, above all
    QGraphicsItem* corner{};
    for(auto* child : node->childItems())
      if(child->zValue() >= 1000. && child->boundingRect() == QRectF{-10., -10., 10., 10.})
        corner = child;
    REQUIRE(corner != nullptr);

    const QSizeF size0 = led.size();
    const QPointF pos0 = led.position();
    const QPointF grip = corner->mapToScene(QPointF{-4., -4.});
    REQUIRE(scene.itemAt(grip, QTransform{}) == corner);

    auto& stack = doc->commandStack();
    const int commands = stack.size();

    // Press on the corner, drag, then a button-less move: the scene drops
    // the grab and sends no release
    sendMouse(scene, QEvent::GraphicsSceneMousePress, grip, grip, Qt::LeftButton, Qt::LeftButton);
    REQUIRE(scene.mouseGrabberItem() == corner);
    const QPointF to = grip + QPointF{40., 30.};
    sendMouse(scene, QEvent::GraphicsSceneMouseMove, to, grip, Qt::NoButton, Qt::LeftButton);
    CHECK(led.size().width() == size0.width() + 40.);
    CHECK(led.size().height() == size0.height() + 30.);
    CHECK(led.position() == pos0);

    sendMouse(scene, QEvent::GraphicsSceneMouseMove, to, to, Qt::NoButton, Qt::NoButton);
    CHECK(scene.mouseGrabberItem() == nullptr);
    CHECK(stack.size() == commands + 1);

    // The next drag starts from the new size, as a new edit
    const QSizeF size1 = led.size();
    const QPointF grip1 = corner->mapToScene(QPointF{-4., -4.});
    sendMouse(scene, QEvent::GraphicsSceneMousePress, grip1, grip1, Qt::LeftButton, Qt::LeftButton);
    const QPointF to1 = grip1 + QPointF{10., 10.};
    sendMouse(scene, QEvent::GraphicsSceneMouseMove, to1, grip1, Qt::NoButton, Qt::LeftButton);
    sendMouse(scene, QEvent::GraphicsSceneMouseRelease, to1, grip1, Qt::LeftButton, Qt::NoButton);
    CHECK(led.size().width() == size1.width() + 10.);
    CHECK(stack.size() == commands + 2);

    delete root;
  });
}
