// Integration test: dropping an effect (a time-independent process) on an
// interval of a scenario creates a nodal slot for it. The node shows up
// centered in that slot, and the slot is as tall as the node.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
using score::test::run_events_for;

Process::NodeItem* nodeOf(Scenario::NodalIntervalView& v, const Process::ProcessModel& p)
{
  for(auto item : v.nodeContainer().childItems())
    if(auto n = dynamic_cast<Process::NodeItem*>(item))
      if(&n->model() == &p)
        return n;
  return nullptr;
}

//! The nodal canvas showing `p`: the only one, the interval's nodal slot.
Scenario::NodalIntervalView* nodalViewOf(score::Document& doc, const Process::ProcessModel& p)
{
  auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc);
  REQUIRE(pr);
  for(auto item : pr->view().scene().items())
    if(auto n = dynamic_cast<Scenario::NodalIntervalView*>(item))
      if(nodeOf(*n, p))
        return n;
  return nullptr;
}

//! The part of the nodal slot that the document's view shows, in scene
//! coordinates.
QRectF visibleSlot(score::Document& doc, Scenario::NodalIntervalView& v)
{
  auto p = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc);
  auto& gv = p->view().view();
  const QRectF viewRect
      = gv.mapToScene(gv.viewport()->rect()).boundingRect();
  return v.sceneBoundingRect().intersected(viewRect);
}

// Any effect: avnd's Counter (Control/Mappings) is always built.
const auto effect_key
    = UuidKey<Process::ProcessModel>{"acdc0a7e-676f-462c-b46d-c6cd99fa74a2"};
const auto float_key
    = UuidKey<Process::ProcessModel>{"ee3a50c0-a202-4f51-a26d-be57a939997d"};

Scenario::IntervalModel& newInterval(score::Document& doc)
{
  auto& scenar = score::test::base_scenario(doc);
  auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
      scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(10000), 0.1, false};
  const auto id = create->createdInterval();
  CommandDispatcher<>{doc.context().commandStack}.submit(create);
  score::test::settle();
  return scenar.interval(id);
}

//! Drops the effect \p key on \p itv as the library does, in a new nodal slot,
//! and lets its node settle on the size it only reaches some time after being
//! created. Null when the effect is not built.
Process::ProcessModel* dropEffect(
    score::Document& doc, Scenario::IntervalModel& itv,
    const UuidKey<Process::ProcessModel>& key)
{
  Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto proc = m.createProcessInNewSlot(itv, key, {}, QPointF{});
  if(!proc)
    return nullptr;
  m.commit();
  run_events_for(300);
  return proc;
}

}

TEST_CASE(
    "An effect dropped on an interval is centered in its new nodal slot, which fits it",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto proc = dropEffect(*doc, itv, effect_key);
    if(!proc)
      SKIP("avnd Counter not built");
    REQUIRE(proc->flags() & Process::ProcessFlags::TimeIndependent);

    auto& sv = itv.smallView();
    auto slot = std::find_if(sv.begin(), sv.end(), [](auto& s) { return s.nodal; });
    REQUIRE(slot != sv.end());

    auto nodal = nodalViewOf(*doc, *proc);
    REQUIRE(nodal);
    auto node = nodeOf(*nodal, *proc);
    REQUIRE(node);

    const QRectF nodeRect = node->sceneBoundingRect();
    const QRectF visible = visibleSlot(*doc, *nodal);
    INFO("node " << nodeRect.x() << "," << nodeRect.y() << " " << nodeRect.width() << "x"
                 << nodeRect.height());
    INFO("visible slot " << visible.x() << "," << visible.y() << " " << visible.width()
                         << "x" << visible.height());
    INFO("slot height " << slot->height);

    // The whole node is visible
    CHECK(visible.contains(nodeRect));
    // Centered
    CHECK(nodeRect.center().x() == Approx(visible.center().x()).margin(2.));
    CHECK(nodeRect.center().y() == Approx(visible.center().y()).margin(2.));
    // The slot fits the node: a small margin above and below
    CHECK(slot->height >= nodeRect.height());
    CHECK(slot->height <= nodeRect.height() + 40.);
    const double fitted = slot->height;

    // Undo the drop, redo it: the same slot again
    doc->commandStack().undo();
    run_events_for(100);
    CHECK(std::none_of(
        itv.smallView().begin(), itv.smallView().end(), [](auto& s) { return s.nodal; }));
    doc->commandStack().redo();
    run_events_for(300);
    REQUIRE(!itv.processes.empty());
    auto& sv2 = itv.smallView();
    auto slot2 = std::find_if(sv2.begin(), sv2.end(), [](auto& s) { return s.nodal; });
    REQUIRE(slot2 != sv2.end());
    CHECK(slot2->height == Approx(fitted));
    auto& redone = *itv.processes.begin();
    auto nodal2 = nodalViewOf(*doc, redone);
    REQUIRE(nodal2);
    auto node2 = nodeOf(*nodal2, redone);
    REQUIRE(node2);
    const QRectF nodeRect2 = node2->sceneBoundingRect();
    const QRectF visible2 = visibleSlot(*doc, *nodal2);
    CHECK(visible2.contains(nodeRect2));
    CHECK(nodeRect2.center().x() == Approx(visible2.center().x()).margin(2.));
    CHECK(nodeRect2.center().y() == Approx(visible2.center().y()).margin(2.));
  });
}

TEST_CASE(
    "Once the user acts in the view, a node changing size leaves the canvas where it is",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto proc = dropEffect(*doc, itv, effect_key);
    if(!proc)
      SKIP("avnd Counter not built");

    auto nodal = nodalViewOf(*doc, *proc);
    REQUIRE(nodal);
    auto& container = nodal->nodeContainer();

    // A new node settling on its size: the canvas follows it.
    const QSizeF sz = proc->size();
    proc->setSize(QSizeF{sz.width() + 100., sz.height() + 60.});
    run_events_for(50);
    const double followedScale = container.scale();

    // The user presses in the view (to resize the node, say): from then on the
    // canvas stays put under the cursor, whatever the node's size does.
    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    auto* vp = pr->view().view().viewport();
    const QPoint at = pr->view().view().mapFromScene(nodal->sceneBoundingRect().center());
    QMouseEvent press{
        QEvent::MouseButtonPress, QPointF(at), vp->mapToGlobal(QPointF(at)), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(vp, &press);
    QMouseEvent release{
        QEvent::MouseButtonRelease, QPointF(at), vp->mapToGlobal(QPointF(at)),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
    QApplication::sendEvent(vp, &release);
    run_events_for(50);
    const QPointF pinned = container.pos();

    const auto slotHeight = [&] {
      auto& sv = itv.smallView();
      return std::find_if(sv.begin(), sv.end(), [](auto& s) { return s.nodal; })->height;
    };
    const double slotBefore = slotHeight();
    proc->setSize(QSizeF{sz.width() + 400., sz.height() + 300.});
    run_events_for(50);
    // Neither the canvas nor the slot around it follow the node any more.
    CHECK(slotHeight() == slotBefore);
    CHECK(container.pos() == pinned);
    CHECK(container.scale() == Approx(followedScale));
    proc->setSize(sz);
    run_events_for(50);
    CHECK(container.pos() == pinned);
    CHECK(container.scale() == Approx(followedScale));
  });
}

#include <score/model/Skin.hpp>

#include <QGraphicsView>
#include <QMouseEvent>

// The cursor Qt really shows: mouse moves sent to the document's view,
// approaching a Float node's Out port from below.
TEST_CASE("The hand cursor shows over a port only, in the document's view", "[integration][nodal][gui][cursor]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto proc = dropEffect(*doc, itv, float_key);
    if(!proc)
      SKIP("avnd Float not built");

    auto nodal = nodalViewOf(*doc, *proc);
    REQUIRE(nodal);
    auto node = nodeOf(*nodal, *proc);
    REQUIRE(node);
    Dataflow::PortItem* out{};
    for(auto item : node->scene()->items())
      if(auto p = dynamic_cast<Dataflow::PortItem*>(item))
        if(&p->port() == proc->outlets()[0])
          out = p;
    REQUIRE(out);

    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    auto& gv = pr->view().view();
    auto* vp = gv.viewport();
    for(double z : {1., 2., 4.})
    {
      CAPTURE(z);
      gv.setTransform(QTransform::fromScale(z, z));
      gv.centerOn(out->sceneCenter());
      run_events_for(30);
      const QPoint c = gv.mapFromScene(out->sceneCenter());
      const auto hand = score::Skin::instance().CursorPointingHand;
      auto isHand = [&] {
        const auto cur = vp->cursor();
        return cur.shape() == hand.shape()
               && cur.pixmap().cacheKey() == hand.pixmap().cacheKey();
      };
      // Every view pixel within 30 px of the port: the hand, and a click on the
      // port, over the drawn circle only (radius 3 and a 1.5 px pen), and over all
      // of its inside. The view hit-tests a pixel as its whole square: measured
      // from the port's exact center to the nearest and farthest points of that
      // square, not from a pixel center, which would move with the port's
      // position below the pixel.
      const double zoom = gv.transform().m11();
      const QPointF exact = gv.viewportTransform().map(out->sceneCenter());
      const double drawn = (3. + 0.75 + 0.5) * zoom;
      int handOutside = 0, grabOutside = 0, missedInside = 0;
      std::string worst;
      for(int dy = -30; dy <= 30; dy++)
        for(int dx = -30; dx <= 30; dx++)
        {
          const QPoint p = c + QPoint{dx, dy};
          QMouseEvent ev{
              QEvent::MouseMove, QPointF(p), vp->mapToGlobal(QPointF(p)), Qt::NoButton,
              Qt::NoButton, Qt::NoModifier};
          QApplication::sendEvent(vp, &ev);
          auto axis = [](double lo, double v) {
            const double hi = lo + 1.;
            const double nearest = v < lo ? lo - v : v > hi ? v - hi : 0.;
            const double farthest = std::max(std::abs(v - lo), std::abs(v - hi));
            return std::pair{nearest, farthest};
          };
          const auto [nx, fx] = axis(p.x(), exact.x());
          const auto [ny, fy] = axis(p.y(), exact.y());
          const double nearest = std::hypot(nx, ny);
          const double farthest = std::hypot(fx, fy);
          const bool hand = isHand();
          const bool grabs = gv.itemAt(p) == out;
          if(nearest > drawn && hand)
          {
            handOutside++;
            worst = std::to_string(dx) + "," + std::to_string(dy);
          }
          if(nearest > drawn && grabs)
            grabOutside++;
          if(farthest <= 3. * zoom && (!hand || !grabs))
            missedInside++;
        }
      INFO("zoom " << zoom << ", e.g. hand at " << worst);
      CHECK(handOutside == 0);
      CHECK(grabOutside == 0);
      CHECK(missedInside == 0);
    }
  });
}

#include <score/graphics/TextItem.hpp>

#include <QDrag>
#include <QTimer>

// A cable can be dragged from a port's name too, not only from its circle.
TEST_CASE("Dragging a port's label starts a cable from the port", "[integration][nodal][gui][cable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto proc = dropEffect(*doc, itv, float_key);
    if(!proc)
      SKIP("avnd Float not built");

    auto nodal = nodalViewOf(*doc, *proc);
    REQUIRE(nodal);
    Dataflow::PortItem* out{};
    for(auto item : nodal->scene()->items())
      if(auto p = dynamic_cast<Dataflow::PortItem*>(item))
        if(&p->port() == proc->outlets()[0])
          out = p;
    REQUIRE(out);
    score::SimpleTextItem* label{};
    for(auto* sibling : out->parentItem()->childItems())
      if(auto* t = dynamic_cast<score::SimpleTextItem*>(sibling))
        label = t;
    REQUIRE(label);

    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    auto& gv = pr->view().view();
    auto* vp = gv.viewport();
    gv.centerOn(label->sceneBoundingRect().center());
    run_events_for(30);
    const QPoint at = gv.mapFromScene(label->sceneBoundingRect().center());
    REQUIRE(vp->rect().contains(at));
    REQUIRE(gv.itemAt(at) == label);

    // Seen from inside the drag loop, which is then cancelled.
    Dataflow::PortItem* dragged{};
    QTimer::singleShot(50, [&] {
      dragged = Dataflow::PortItem::clickedPort;
      QDrag::cancel();
    });

    QMouseEvent press{
        QEvent::MouseButtonPress, QPointF(at), vp->mapToGlobal(QPointF(at)), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(vp, &press);
    const QPoint to = at + QPoint{40, 10};
    QMouseEvent move{
        QEvent::MouseMove, QPointF(to), vp->mapToGlobal(QPointF(to)), Qt::NoButton,
        Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(vp, &move);
    run_events_for(100);
    QMouseEvent release{
        QEvent::MouseButtonRelease, QPointF(to), vp->mapToGlobal(QPointF(to)),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
    QApplication::sendEvent(vp, &release);

    CHECK(dragged == out);
    CHECK(Dataflow::PortItem::clickedPort == nullptr);
  });
}
