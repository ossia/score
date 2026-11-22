// Integration test: editing a patch in an interval's nodal slot.
//
// Cables are drawn over the nodes they link, and over the other cables when
// one of their nodes is selected. Double-clicking the canvas background asks
// for a process to add there. A cable pulled off its port and let go of away
// from any port is removed. Removing a process from the middle of a chain
// reconnects what it linked, unless Shift is held. Every port's menu can learn
// its address. The hover previews of ports can be turned off.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Library/ProcessPicker.hpp>
#include <Process/ProcessList.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <QCursor>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QMouseEvent>
#include <QGraphicsView>
#include <QDialogButtonBox>
#include <QDrag>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QListWidget>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QTimer>
#include <score/serialization/JSONVisitor.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <QTemporaryDir>
#include <QToolButton>

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace
{
using score::test::run_events_for;

// avnd's Float value: one value inlet, one value outlet. Always built.
const auto float_key = UuidKey<Process::ProcessModel>{"ee3a50c0-a202-4f51-a26d-be57a939997d"};

//! Picks the Float process in the process picker that is open, as typing its
//! name and Return does.
bool pickFloat()
{
  auto* picker = dynamic_cast<Library::ProcessPicker*>(QApplication::activePopupWidget());
  if(!picker)
    return false;
  const auto& fact = *score::GUIAppContext().interfaces<Process::ProcessFactoryList>().get(float_key);
  picker->setSearch(fact.prettyName());
  return picker->accept();
}

Scenario::ScenarioDocumentModel& docModel(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());
}

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

Process::ProcessModel* addFloat(score::Document& doc, Scenario::IntervalModel& itv, QPointF pos)
{
  Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto proc = m.createProcessInNewSlot(itv, float_key, {}, pos);
  if(!proc)
    return nullptr;
  m.commit();
  run_events_for(200);
  return proc;
}

Process::Cable& connect(score::Document& doc, Process::ProcessModel& a, Process::ProcessModel& b)
{
  auto& dm = docModel(doc);
  const auto id = getStrongId(dm.cables);
  CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
      dm, id, Process::CableType::ImmediateGlutton, *a.outlets()[0], *b.inlets()[0]);
  run_events_for(50);
  return dm.cables.at(id);
}

const Process::Cable* cableBetween(score::Document& doc, const Process::ProcessModel& a, const Process::ProcessModel& b)
{
  for(auto& c : docModel(doc).cables)
    if(&c.source().find(doc.context()) == a.outlets()[0]
       && &c.sink().find(doc.context()) == b.inlets()[0])
      return &c;
  return nullptr;
}

QGraphicsScene& documentScene(score::Document& doc)
{
  auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc);
  REQUIRE(pr);
  return pr->view().scene();
}

Scenario::NodalIntervalView* nodalViewOf(score::Document& doc, const Process::ProcessModel& p)
{
  for(auto item : documentScene(doc).items())
    if(auto n = dynamic_cast<Scenario::NodalIntervalView*>(item))
      for(auto child : n->nodeContainer().childItems())
        if(auto node = dynamic_cast<Process::NodeItem*>(child); node && &node->model() == &p)
          return n;
  return nullptr;
}

//! A point of the canvas, in its coordinates, with nothing on it
QPointF emptySpot(Scenario::NodalIntervalView& view)
{
  const QRectF r = view.boundingRect().adjusted(6., 6., -6., -6.);
  for(QPointF p : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight(),
                   QPointF{r.center().x(), r.top()}, QPointF{r.center().x(), r.bottom()}})
  {
    bool free = true;
    for(auto* item : view.scene()->items(view.mapToScene(p)))
      if(item != &view && view.isAncestorOf(item))
        free = false;
    if(free)
      return p;
  }
  FAIL("no free spot on the nodal canvas");
  return {};
}

//! A double click of the left button at `scenePos` through the document's
//! view, as the platform delivers it
void doubleClickAt(score::Document& doc, QPointF scenePos)
{
  auto& gv = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc)
                 ->view()
                 .view();
  gv.centerOn(scenePos);
  run_events_for(30);
  auto* vp = gv.viewport();
  const QPointF at = gv.mapFromScene(scenePos);
  const QPointF global = vp->mapToGlobal(at);
  auto send = [&](QEvent::Type type, Qt::MouseButtons buttons) {
    QMouseEvent ev{type, at, global, Qt::LeftButton, buttons, Qt::NoModifier};
    QApplication::sendEvent(vp, &ev);
  };
  send(QEvent::MouseButtonPress, Qt::LeftButton);
  send(QEvent::MouseButtonRelease, Qt::NoButton);
  send(QEvent::MouseButtonDblClick, Qt::LeftButton);
  send(QEvent::MouseButtonRelease, Qt::NoButton);
}

Process::NodeItem* nodeOf(score::Document& doc, const Process::ProcessModel& p)
{
  for(auto item : documentScene(doc).items())
    if(auto node = dynamic_cast<Process::NodeItem*>(item); node && &node->model() == &p)
      return node;
  return nullptr;
}

Dataflow::CableItem* itemOf(score::Document& doc, const Process::Cable& c)
{
  for(auto item : documentScene(doc).items())
    if(auto ci = qgraphicsitem_cast<Dataflow::CableItem*>(item); ci && &ci->model() == &c)
      return ci;
  return nullptr;
}

Dataflow::PortItem* portItemOf(score::Document& doc, const Process::Port& p)
{
  for(auto item : documentScene(doc).items())
    if(auto pi = qgraphicsitem_cast<Dataflow::PortItem*>(item); pi && &pi->port() == &p)
      return pi;
  return nullptr;
}

//! Two Float nodes side by side in one interval's nodal slot, the first
//! cabled into the second.
struct TwoNodes
{
  score::Document* doc{};
  Scenario::IntervalModel* itv{};
  Process::ProcessModel* a{};
  Process::ProcessModel* b{};
  Process::Cable* cable{};

  explicit TwoNodes(const score::GUIApplicationContext& app)
  {
    doc = score::test::new_document(app);
    REQUIRE(doc);
    itv = &newInterval(*doc);
    a = addFloat(*doc, *itv, {0., 0.});
    REQUIRE(a);
    b = addFloat(*doc, *itv, {300., 0.});
    REQUIRE(b);
    cable = &connect(*doc, *a, *b);
    REQUIRE(nodalViewOf(*doc, *a));
    REQUIRE(nodalViewOf(*doc, *a) == nodalViewOf(*doc, *b));
  }
};
}

TEST_CASE("A cable between nodes is drawn over them", "[integration][nodal][gui][cable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto* item = itemOf(*t.doc, *t.cable);
    REQUIRE(item);
    auto* na = nodeOf(*t.doc, *t.a);
    auto* nb = nodeOf(*t.doc, *t.b);
    REQUIRE(na);
    REQUIRE(nb);

    // Siblings of the nodes, stacked over them
    CHECK(item->parentItem() == na->parentItem());
    CHECK(item->zValue() == Dataflow::CableItem::nodalZ);
    CHECK(item->zValue() > na->zValue());
    CHECK(item->zValue() > nb->zValue());
  });
}

TEST_CASE("Selecting a node raises and highlights its cables", "[integration][nodal][gui][cable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto* item = itemOf(*t.doc, *t.cable);
    REQUIRE(item);
    auto& stack = t.doc->context().selectionStack;

    stack.pushNewSelection({t.a});
    run_events_for(20);
    CHECK(item->linksSelectedNode());
    CHECK(item->zValue() == Dataflow::CableItem::selectedZ);

    stack.pushNewSelection({t.b});
    run_events_for(20);
    CHECK(item->linksSelectedNode());
    CHECK(item->zValue() == Dataflow::CableItem::selectedZ);

    stack.deselect();
    run_events_for(20);
    CHECK(!item->linksSelectedNode());
    CHECK(item->zValue() == Dataflow::CableItem::nodalZ);
  });
}

TEST_CASE(
    "Double-clicking the nodal background asks for a process to add there",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto* view = nodalViewOf(*t.doc, *t.a);
    REQUIRE(view);

    const QPointF at = emptySpot(*view);
    const QPointF expected = view->nodeContainer().mapFromParent(at);

    const auto before = t.itv->processes.size();
    doubleClickAt(*t.doc, view->mapToScene(at));
    run_events_for(100);

    REQUIRE(pickFloat());
    run_events_for(50);
    REQUIRE(t.itv->processes.size() == before + 1);
    const Process::ProcessModel* added{};
    for(auto& p : t.itv->processes)
      if(&p != t.a && &p != t.b && p.concreteKey() == float_key)
        added = &p;
    REQUIRE(added);
    CHECK(QLineF{added->position(), expected}.length() < 1.);

    // One undo step
    t.doc->commandStack().undo();
    CHECK(t.itv->processes.size() == before);
  });
}

TEST_CASE(
    "A cable pulled off its port and let go of away from any port is removed",
    "[integration][nodal][gui][cable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto* src = portItemOf(*t.doc, *t.a->outlets()[0]);
    REQUIRE(src);
    auto& scene = documentScene(*t.doc);
    auto* view = nodalViewOf(*t.doc, *t.a);

    const QPointF away = view->mapToScene(emptySpot(*view));

    const auto processes = t.itv->processes.size();
    const auto cableId = t.cable->id();
    auto& cables = docModel(*t.doc).cables;

    auto& gv = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
                   *t.doc)
                   ->view()
                   .view();

    //! What the platform sends while the cable is dragged over `at` and let go
    //! of there, as seen from inside the drag
    auto dropAt = [&](QPointF at) {
      QMimeData mime;
      mime.setData(score::mime::port(), {});
      for(auto type : {QEvent::GraphicsSceneDragEnter, QEvent::GraphicsSceneDragMove,
                       QEvent::GraphicsSceneDrop})
      {
        QGraphicsSceneDragDropEvent ev{type};
        ev.setScenePos(at);
        ev.setScreenPos(gv.viewport()->mapToGlobal(gv.mapFromScene(at)));
        ev.setWidget(gv.viewport());
        ev.setMimeData(&mime);
        ev.setPossibleActions(Qt::CopyAction | Qt::MoveAction);
        ev.setProposedAction(Qt::CopyAction);
        ev.setDropAction(Qt::CopyAction);
        QApplication::sendEvent(&scene, &ev);
      }
      QDrag::cancel();
    };

    SECTION("away from any port")
    {
      QTimer::singleShot(50, [&] { dropAt(away); });
      // Pulling the sink end: the source stays as the anchor
      Dataflow::beginPortDrag(*src, src->sceneCenter(), {t.cable});
      run_events_for(50);

      CHECK(cables.find(cableId) == cables.end());
      CHECK(t.a->outlets()[0]->cables().empty());
      CHECK(t.b->inlets()[0]->cables().empty());
      CHECK(t.itv->processes.size() == processes);

      t.doc->commandStack().undo();
      run_events_for(20);
      CHECK(cables.find(cableId) != cables.end());
    }

    SECTION("over the scenario, which made an automation of a dropped control port")
    {
      // The base scenario's own surface, with nothing of it under the point
      QGraphicsItem* scenarioView{};
      for(auto* item : scene.items())
        if(item->type() == Scenario::ScenarioView::Type)
          scenarioView = item;
      REQUIRE(scenarioView);
      std::optional<QPointF> spot;
      const QRectF visible
          = gv.mapToScene(gv.viewport()->rect()).boundingRect().intersected(
              scenarioView->sceneBoundingRect());
      for(double y = visible.top() + 10.; y < visible.bottom() && !spot; y += 15.)
        for(double x = visible.left() + 10.; x < visible.right() && !spot; x += 15.)
        {
          const auto under = scene.items(QPointF{x, y});
          if(!under.empty() && under.front() == scenarioView)
            spot = QPointF{x, y};
        }
      REQUIRE(spot);

      auto& scenario = score::test::base_scenario(*t.doc);
      const auto intervals = scenario.intervals.size();

      // Pulling the source end: the anchor is the control inlet, the port an
      // automation is made for
      auto* sinkItem = portItemOf(*t.doc, *t.b->inlets()[0]);
      REQUIRE(sinkItem);
      QTimer::singleShot(50, [&] { dropAt(*spot); });
      Dataflow::beginPortDrag(*sinkItem, sinkItem->sceneCenter(), {t.cable});
      run_events_for(50);

      CHECK(cables.find(cableId) == cables.end());
      CHECK(scenario.intervals.size() == intervals);
    }

    SECTION("just missing a port, it stays")
    {
      // A few pixels away from a port it cannot go to: a near miss, not a
      // throw-away
      const QPoint px = gv.mapFromScene(src->sceneCenter()) + QPoint{6, 0};
      QTimer::singleShot(50, [&] { dropAt(gv.mapToScene(px)); });
      Dataflow::beginPortDrag(*src, src->sceneCenter(), {t.cable});
      run_events_for(50);
      CHECK(cables.find(cableId) != cables.end());
    }
  });
}

namespace
{
//! Part of `view` shown in the document's view, in `view` coordinates
QRectF shownPart(score::Document& doc, Scenario::NodalIntervalView& view)
{
  auto& gv = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc)
                 ->view()
                 .view();
  const QRectF r{
      view.mapFromScene(gv.mapToScene(0, 0)),
      view.mapFromScene(gv.mapToScene(gv.width(), gv.height()))};
  return r.intersected(view.boundingRect());
}

QRectF nodeRectIn(Scenario::NodalIntervalView& view, Process::NodeItem& node)
{
  return view.mapRectFromScene(node.sceneBoundingRect());
}

//! A node at the origin and another one far away from it, in a canvas pinned
//! at 1:1.
struct FarApartNodes
{
  score::Document* doc{};
  Scenario::IntervalModel* itv{};
  Process::ProcessModel* a{};
  Process::ProcessModel* far{};
  Scenario::NodalIntervalView* view{};

  explicit FarApartNodes(const score::GUIApplicationContext& app)
  {
    doc = score::test::new_document(app);
    REQUIRE(doc);
    itv = &newInterval(*doc);
    a = addFloat(*doc, *itv, {0., 0.});
    REQUIRE(a);
    far = addFloat(*doc, *itv, {5000., 3000.});
    REQUIRE(far);
    view = nodalViewOf(*doc, *a);
    REQUIRE(view);
    view->rescale();
    run_events_for(50);
  }

  //! Pans so that the center of `a` is at `at` (view coordinates)
  void putAAt(QPointF at)
  {
    auto* na = nodeOf(*doc, *a);
    REQUIRE(na);
    view->panBy(at - nodeRectIn(*view, *na).center());
    run_events_for(50);
  }

  Process::ProcessModel& addAfterA()
  {
    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    const auto before = itv->processes.size();
    // What picking a process in the inspector or the library does with `a`
    // selected
    Scenario::createProcessAfterPort(
        *pr, Process::ProcessData{float_key, "Float", {}}, {}, {}, *a, *a->outlets()[0],
        true);
    run_events_for(300);
    REQUIRE(itv->processes.size() == before + 1);
    for(auto& p : itv->processes)
      if(&p != a && &p != far && p.concreteKey() == float_key)
        return p;
    FAIL("no process added");
    return *a;
  }
};
}

TEST_CASE(
    "Adding a node next to a visible one does not move the view",
    "[integration][nodal][gui][reveal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    FarApartNodes t{app};
    const QRectF shown = shownPart(*t.doc, *t.view);
    REQUIRE(!shown.isEmpty());
    t.putAAt({shown.left() + 0.25 * shown.width(), shown.center().y()});

    const QPointF pos = t.view->nodeContainer().pos();
    const double scale = t.view->zoom();

    auto& added = t.addAfterA();
    auto* node = nodeOf(*t.doc, added);
    REQUIRE(node);
    REQUIRE(shown.contains(nodeRectIn(*t.view, *node)));

    CHECK(t.view->zoom() == scale);
    CHECK(QLineF{t.view->nodeContainer().pos(), pos}.length() < 0.5);
  });
}

TEST_CASE(
    "Adding a node outside of the view scrolls just enough to show it",
    "[integration][nodal][gui][reveal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    FarApartNodes t{app};
    const QRectF shown = shownPart(*t.doc, *t.view);
    REQUIRE(!shown.isEmpty());
    // `a` right at the right edge: what is added after it lands outside
    auto* na = nodeOf(*t.doc, *t.a);
    REQUIRE(na);
    const QRectF ra = nodeRectIn(*t.view, *na);
    t.putAAt({shown.right() - ra.width() / 2. - 2., shown.center().y()});

    const double scale = t.view->zoom();
    const QPointF pos = t.view->nodeContainer().pos();

    auto& added = t.addAfterA();
    auto* node = nodeOf(*t.doc, added);
    REQUIRE(node);
    const QRectF r = nodeRectIn(*t.view, *node);
    INFO("shown " << shown.x() << " " << shown.y() << " " << shown.width() << "x"
                  << shown.height() << ", node " << r.x() << " " << r.y() << " "
                  << r.width() << "x" << r.height());

    CHECK(t.view->zoom() == scale);
    CHECK(shown.contains(r));
    // Only as far as needed: the node ends up against the edge it came from,
    // and the view did not move on the axis where the node was already shown.
    CHECK(r.right() <= shown.right());
    CHECK(r.right() > shown.right() - 10.);
    const QPointF moved = t.view->nodeContainer().pos() - pos;
    CHECK(moved.x() < 0.);
    CHECK(std::abs(moved.y()) < 0.5);
  });
}

TEST_CASE(
    "Adding a node larger than the view zooms out just enough to show it",
    "[integration][nodal][gui][reveal]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    FarApartNodes t{app};
    // A thin slot and a large zoom: a node does not fit in it
    for(std::size_t i = 0; i < t.itv->smallView().size(); i++)
      t.itv->setSlotHeight(Scenario::SlotId{i, Scenario::Slot::SmallView}, 40.);
    t.view->zoomTo(5.0);
    run_events_for(100);
    const QRectF shown = shownPart(*t.doc, *t.view);
    REQUIRE(!shown.isEmpty());
    t.putAAt(shown.center());

    const double scale = t.view->zoom();

    auto& added = t.addAfterA();
    auto* node = nodeOf(*t.doc, added);
    REQUIRE(node);
    const QRectF r = nodeRectIn(*t.view, *node);
    const QRectF shownAfter = shownPart(*t.doc, *t.view);
    INFO("shown " << shownAfter.width() << "x" << shownAfter.height() << ", node "
                  << r.width() << "x" << r.height() << ", scale "
                  << t.view->zoom() << " from " << scale);

    CHECK(t.view->zoom() < scale);
    CHECK(shownAfter.adjusted(-0.5, -0.5, 0.5, 0.5).contains(r));
    // No further than needed: the node still fills the view in one direction,
    // which fitting every node (5000 units apart) would not.
    CHECK(
        std::max(r.width() / shownAfter.width(), r.height() / shownAfter.height())
        > 0.8);
  });
}

TEST_CASE(
    "Delete reconnects around a removed process, Shift+Delete removes its cables",
    "[integration][nodal][gui][remove]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto* a = addFloat(*doc, itv, {0., 0.});
    REQUIRE(a);
    auto* b = addFloat(*doc, itv, {300., 0.});
    auto* c = addFloat(*doc, itv, {600., 0.});
    REQUIRE(b);
    REQUIRE(c);
    connect(*doc, *a, *b);
    connect(*doc, *b, *c);

    auto& reconnecting = app.actions.action<Actions::RemoveElements>();
    auto& disconnecting = app.actions.action<Actions::RemoveElementsAndCables>();

    // Delete and Backspace both, on every platform
    const auto shortcuts = reconnecting.action()->shortcuts();
    CHECK(shortcuts.contains(QKeySequence{Qt::Key_Delete}));
    CHECK(shortcuts.contains(QKeySequence{Qt::Key_Backspace}));
    const auto shifted = disconnecting.action()->shortcuts();
    CHECK(shifted.contains(QKeySequence{Qt::SHIFT | Qt::Key_Delete}));
    CHECK(shifted.contains(QKeySequence{Qt::SHIFT | Qt::Key_Backspace}));

    auto& stack = doc->commandStack();
    auto removeB = [&](score::Action& act) {
      doc->context().selectionStack.pushNewSelection({b});
      run_events_for(20);
      act.action()->trigger();
      run_events_for(50);
    };
    // Undo brings back a new object: compare ids
    const auto bId = b->id();
    auto containsB = [&] { return itv.processes.find(bId) != itv.processes.end(); };
    auto restoredB = [&]() -> Process::ProcessModel& { return itv.processes.at(bId); };
    const int index = stack.currentIndex();
    const auto cableCount = docModel(*doc).cables.size();

    // One action, one undo step, and the exact cables back after it
    auto checkUndo = [&] {
      CHECK(stack.currentIndex() == index + 1);
      stack.undo();
      run_events_for(20);
      CHECK(stack.currentIndex() == index);
      REQUIRE(containsB());
      CHECK(cableBetween(*doc, *a, restoredB()));
      CHECK(cableBetween(*doc, restoredB(), *c));
      CHECK(!cableBetween(*doc, *a, *c));
      CHECK(docModel(*doc).cables.size() == cableCount);
    };

    SECTION("Delete")
    {
      removeB(reconnecting);
      CHECK(!containsB());
      CHECK(cableBetween(*doc, *a, *c));
      CHECK(docModel(*doc).cables.size() == 1);
      checkUndo();
    }

    SECTION("Shift+Delete")
    {
      removeB(disconnecting);
      CHECK(!containsB());
      CHECK(!cableBetween(*doc, *a, *c));
      CHECK(a->outlets()[0]->cables().empty());
      CHECK(c->inlets()[0]->cables().empty());
      checkUndo();
    }

    SECTION("Delete with A->C already there adds no second one")
    {
      connect(*doc, *a, *c);
      const int diamondIndex = stack.currentIndex();
      removeB(reconnecting);
      CHECK(!containsB());
      CHECK(docModel(*doc).cables.size() == 1);
      CHECK(cableBetween(*doc, *a, *c));
      CHECK(stack.currentIndex() == diamondIndex + 1);
    }
  });
}

TEST_CASE("A port's menu learns its address", "[integration][nodal][gui][learn]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto& ctx = t.doc->context();
    auto* port = portItemOf(*t.doc, *t.b->inlets()[0]);
    REQUIRE(port);
    const auto before = t.b->inlets()[0]->address();

    // An OSC device with one parameter: it only ever sends, so nothing has to
    // answer for its tree to exist.
    Device::ProtocolFactory* osc{};
    for(auto& f : app.interfaces<Device::ProtocolFactoryList>())
      if(f.prettyName() == "OSC")
        osc = &f;
    REQUIRE(osc);
    auto settings = osc->defaultSettings();
    settings.name = "learndev";
    Device::Node dev{settings, nullptr};
    Device::AddressSettings param;
    param.name = "level";
    param.value = 0.5f;
    param.ioType = ossia::access_mode::BI;
    dev.push_back(Device::Node{param, &dev});
    auto& devplug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    CommandDispatcher<>{ctx.commandStack}.submit(
        new Explorer::Command::LoadDevice{devplug, std::move(dev)});

    auto& providers = app.interfaces<Device::DeviceModelProviderList>();
    auto* provider = providers.getBestProvider(ctx);
    REQUIRE(provider);
    auto* model = provider->getNodeModel(ctx);
    REQUIRE(model);

    Device::Node* node{};
    for(auto& d : model->rootNode())
      if(d.displayName() == "learndev" && d.childCount() == 1)
        node = &d.childAt(0);
    REQUIRE(node);
    const auto expected = Device::address(*node).address;

    QMenu menu;
    port->addLearnAction(menu, ctx);
    QAction* learn{};
    for(auto* act : menu.actions())
      if(act->text() == QObject::tr("Learn address"))
        learn = act;
    REQUIRE(learn);

    bool opened = false;
    int listed = 0;
    QTimer::singleShot(50, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      opened = dialog != nullptr;
      if(!dialog)
        return;
      // The device sends a value: the dialog lists that address and selects it
      const auto idx = model->modelIndexFromNode(*node, 0);
      Q_EMIT model->dataChanged(idx, idx);
      QApplication::processEvents();
      // Checked after the dialog: a failed REQUIRE here would leave it open
      auto* list = dialog->findChild<QListWidget*>();
      listed = list ? list->count() : -1;
      auto* buttons = dialog->findChild<QDialogButtonBox*>();
      if(!list || !buttons || list->count() != 1)
      {
        dialog->reject();
        return;
      }
      list->setCurrentRow(0);
      buttons->button(QDialogButtonBox::Ok)->click();
    });
    learn->trigger();
    run_events_for(150);

    REQUIRE(opened);
    REQUIRE(listed == 1);
    CHECK(t.b->inlets()[0]->address().address == expected);

    t.doc->commandStack().undo();
    CHECK(t.b->inlets()[0]->address() == before);
  });
}

#include <Process/Dataflow/PortProbe.hpp>
#include <Process/PreviewSettings.hpp>

TEST_CASE("Hover previews of ports can be turned off", "[integration][nodal][gui][probe]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto& scene = documentScene(*t.doc);
    auto* port = portItemOf(*t.doc, *t.a->outlets()[0]);
    REQUIRE(port);
    auto& settings = Process::PreviewSettings::instance();
    const bool initial = settings.hoverProbes();

    auto probes = [&] {
      int n = 0;
      for(auto* item : scene.items())
        if(dynamic_cast<Process::PortProbeItem*>(item))
          n++;
      return n;
    };
    auto hover = [&] {
      Process::PortProbeHover::instance().enter(
          port->port(),
          score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*t.doc)
              ->context(),
          scene, port->sceneCenter());
      run_events_for(500);
    };

    settings.setHoverProbes(true);
    hover();
    // The monitoring plug-in shows a value port's preview
    REQUIRE(probes() > 0);

    // Turning it off closes an open probe, and no other opens
    settings.setHoverProbes(false);
    run_events_for(50);
    CHECK(probes() == 0);
    Process::PortProbeHover::instance().leave();
    run_events_for(300);
    hover();
    CHECK(probes() == 0);

    Process::PortProbeHover::instance().leave();
    run_events_for(300);
    settings.setHoverProbes(initial);

  });
}

TEST_CASE(
    "A click anywhere but on a port's preview closes it, or the one about to open",
    "[integration][nodal][gui][probe]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    TwoNodes t{app};
    auto& scene = documentScene(*t.doc);
    auto* port = portItemOf(*t.doc, *t.a->outlets()[0]);
    REQUIRE(port);
    auto& settings = Process::PreviewSettings::instance();
    const bool initial = settings.hoverProbes();
    settings.setHoverProbes(true);

    auto probes = [&] {
      int n = 0;
      for(auto* item : scene.items())
        if(dynamic_cast<Process::PortProbeItem*>(item))
          n++;
      return n;
    };
    auto enter = [&] {
      Process::PortProbeHover::instance().enter(
          port->port(),
          score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*t.doc)
              ->context(),
          scene, port->sceneCenter());
    };
    // A press, wherever it is, as the window gets it
    auto press = [&] {
      QWidget* target = scene.views().front()->viewport();
      QMouseEvent ev{
          QEvent::MouseButtonPress, QPointF{2., 2.}, target->mapToGlobal(QPointF{2., 2.}),
          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
      QApplication::sendEvent(target, &ev);
    };

    enter();
    run_events_for(500);
    REQUIRE(probes() > 0);
    press();
    run_events_for(50);
    CHECK(probes() == 0);

    // Pressed before it opens: it does not open
    Process::PortProbeHover::instance().leave();
    run_events_for(300);
    enter();
    run_events_for(50);
    press();
    run_events_for(500);
    CHECK(probes() == 0);

    Process::PortProbeHover::instance().leave();
    run_events_for(300);
    settings.setHoverProbes(initial);
  });
}

#include <Nodal/Process.hpp>
#include <Nodal/View.hpp>

TEST_CASE(
    "Double-clicking a Nodal process's background asks for a process to add there",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
    auto* proc = m.createProcessInNewSlot(
        itv, UuidKey<Process::ProcessModel>{"f5678806-c431-45c5-ae3a-fae5183380fb"}, {}, QPointF{});
    REQUIRE(proc);
    m.commit();
    run_events_for(200);
    auto& nodal = static_cast<Nodal::Model&>(*proc);

    // In the full view of its interval, where its layer is shown
    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    pr->setDisplayedInterval(&itv);
    run_events_for(200);

    Nodal::View* view{};
    for(auto* item : documentScene(*doc).items())
      if(auto* v = dynamic_cast<Nodal::View*>(item))
        view = v;
    REQUIRE(view);

    const QPointF at = view->boundingRect().center();
    doubleClickAt(*doc, view->mapToScene(at));
    run_events_for(100);

    REQUIRE(pickFloat());
    run_events_for(50);
    REQUIRE(nodal.nodes.size() == 1);
    CHECK(QLineF{nodal.nodes.begin()->position(), at}.length() < 1.);

    doc->commandStack().undo();
    CHECK(nodal.nodes.size() == 0);
  });
}

TEST_CASE("Tab in the score asks for a process to add to the selected box", "[integration][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    run_events_for(100);
    doc->context().selectionStack.pushNewSelection({&itv});

    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    auto& view = pr->view().view();
    QKeyEvent tab{QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier};
    QApplication::sendEvent(&view, &tab);
    run_events_for(100);

    const auto before = itv.processes.size();
    REQUIRE(pickFloat());
    run_events_for(50);
    REQUIRE(itv.processes.size() == before + 1);
    bool found = false;
    for(auto& p : itv.processes)
      found |= p.concreteKey() == float_key;
    CHECK(found);

    doc->commandStack().undo();
    CHECK(itv.processes.size() == before);
  });
}

TEST_CASE(
    "Tab on a selected outlet adds a process cabled to it, and goes on from it",
    "[integration][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto* first = addFloat(*doc, itv, QPointF{100, 100});
    REQUIRE(first);
    REQUIRE_FALSE(first->outlets().empty());
    auto& outlet = *first->outlets()[0];
    doc->context().selectionStack.pushNewSelection({&outlet});

    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    QKeyEvent tab{QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier};
    QApplication::sendEvent(&pr->view().view(), &tab);
    run_events_for(100);

    // Only what an outlet of its type can go to
    auto* picker = dynamic_cast<Library::ProcessPicker*>(QApplication::activePopupWidget());
    REQUIRE(picker);
    picker->setSearch({});
    for(const auto& name : picker->processes())
      CHECK_FALSE(name.isEmpty());

    const auto before = itv.processes.size();
    const auto cablesBefore = docModel(*doc).cables.size();
    REQUIRE(pickFloat());
    run_events_for(50);
    REQUIRE(itv.processes.size() == before + 1);
    REQUIRE(docModel(*doc).cables.size() == cablesBefore + 1);

    Process::ProcessModel* added{};
    for(auto& p : itv.processes)
      if(&p != first)
        added = &p;
    REQUIRE(added);
    auto& cable = *docModel(*doc).cables.begin();
    CHECK(&cable.source().find(doc->context()) == &outlet);
    CHECK(&cable.sink().find(doc->context()) == added->inlets()[0]);
    // Beside the node it comes from
    CHECK(added->position().x() > first->position().x());

    // Its outlet is selected: Tab again goes on from it
    const auto& sel = doc->context().selectionStack.currentSelection();
    REQUIRE(sel.size() == 1);
    CHECK(sel.at(0) == added->outlets()[0]);

    // One undo step
    doc->commandStack().undo();
    CHECK(itv.processes.size() == before);
    CHECK(docModel(*doc).cables.size() == cablesBefore);
  });
}

namespace
{
void pressTab(score::Document& doc)
{
  auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc);
  REQUIRE(pr);
  QKeyEvent tab{QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier};
  QApplication::sendEvent(&pr->view().view(), &tab);
  run_events_for(100);
}

std::vector<const Process::ProcessModel*>
processesBut(const Scenario::IntervalModel& itv, const Process::ProcessModel* but)
{
  std::vector<const Process::ProcessModel*> v;
  for(auto& p : itv.processes)
    if(&p != but)
      v.push_back(&p);
  return v;
}
}

TEST_CASE(
    "Processes added by Tab each go where a new one goes, not on each other",
    "[integration][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    run_events_for(100);

    for(int i = 0; i < 2; i++)
    {
      doc->context().selectionStack.pushNewSelection({&itv});
      pressTab(*doc);
      REQUIRE(pickFloat());
      run_events_for(50);
    }
    REQUIRE(itv.processes.size() == 2);
    const auto procs = processesBut(itv, nullptr);
    CHECK(procs[0]->position() != procs[1]->position());
  });
}

TEST_CASE(
    "Tab twice on an outlet puts the new nodes apart, and hands its address on",
    "[integration][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto* first = addFloat(*doc, itv, QPointF{100, 100});
    REQUIRE(first);
    auto& outlet = *first->outlets()[0];
    const State::AddressAccessor address{State::Address{"test", {"value"}}};
    outlet.setAddress(address);

    doc->context().selectionStack.pushNewSelection({&outlet});
    pressTab(*doc);
    REQUIRE(pickFloat());
    run_events_for(50);
    // What it was sending to, the end of the chain sends to now
    CHECK(outlet.address() == State::AddressAccessor{});
    auto added = processesBut(itv, first);
    REQUIRE(added.size() == 1);
    CHECK(added[0]->outlets()[0]->address() == address);

    doc->context().selectionStack.pushNewSelection({&outlet});
    pressTab(*doc);
    REQUIRE(pickFloat());
    run_events_for(50);
    added = processesBut(itv, first);
    REQUIRE(added.size() == 2);
    const QRectF a{added[0]->position(), added[0]->size()};
    const QRectF b{added[1]->position(), added[1]->size()};
    CHECK_FALSE(a.intersects(b));
  });
}

TEST_CASE(
    "A right click on nodes over each other selects the one on top, not the one "
    "under it",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto* under = addFloat(*doc, itv, QPointF{100, 100});
    auto* over = addFloat(*doc, itv, QPointF{120, 110});
    REQUIRE(under);
    REQUIRE(over);
    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    pr->setDisplayedInterval(&itv);
    pr->setNodalMode(true);
    run_events_for(200);

    auto* top = nodeOf(*doc, *over);
    auto* bottom = nodeOf(*doc, *under);
    REQUIRE(top);
    REQUIRE(bottom);
    // A point of both frames, on the title of the one on top
    const QPointF at = top->mapToScene(QPointF{10., 6.});
    REQUIRE(bottom->contains(bottom->mapFromScene(at)));
    Process::NodeItem* first{};
    for(auto* item : documentScene(*doc).items(at))
      if(auto n = dynamic_cast<Process::NodeItem*>(item); n && !first)
        first = n;
    REQUIRE(first == top);
    doc->context().selectionStack.deselect();

    auto& gv = pr->view().view();
    gv.centerOn(at);
    run_events_for(30);
    const QPoint vp = gv.mapFromScene(at);
    bool menu = false;
    QTimer::singleShot(50, [&] {
      if(auto m = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
      {
        menu = true;
        m->close();
      }
    });
    QContextMenuEvent ev{QContextMenuEvent::Mouse, vp, gv.viewport()->mapToGlobal(vp)};
    QApplication::sendEvent(gv.viewport(), &ev);
    run_events_for(100);

    CHECK(menu);
    const auto& sel = doc->context().selectionStack.currentSelection();
    REQUIRE(sel.size() == 1);
    CHECK(sel.at(0) == over);
  });
}

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Scenario/Library/Snippets.hpp>

TEST_CASE("The snippets menu lists the library's snippets in their folders", "[integration][snippets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto* panel = app.findPanel<Library::ProcessPanel>();
    REQUIRE(panel);
    auto& model = panel->processWidget().processModel();
    model.flushPending();
    const QString path = "/somewhere/packages/user/snippets/Drums/Menu kick.scenario";
    if(auto e = Scenario::snippetEntry(path.toStdString()))
      model.publish(std::move(*e));
    model.flushPending();

    QMenu menu;
    QString inserted;
    Scenario::fillSnippetsMenu(menu, [&](const QString& p) { inserted = p; });
    QAction* found{};
    for(auto* top : menu.actions())
      if(top->menu() && top->text() == "Drums")
        for(auto* act : top->menu()->actions())
          if(act->text() == "Menu kick")
            found = act;
    REQUIRE(found);
    found->trigger();
    CHECK(inserted == path);

    // None in the library: said so
    model.rescan();
    QMenu empty;
    Scenario::fillSnippetsMenu(empty, [](const QString&) {});
    bool listed = false;
    for(auto* act : empty.actions())
      listed |= act->text() == "Menu kick";
    CHECK_FALSE(listed);
  });
}

TEST_CASE(
    "Tab on a selected node adds a process cabled after it, as the library does",
    "[integration][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    auto* first = addFloat(*doc, itv, QPointF{100, 100});
    REQUIRE(first);
    REQUIRE_FALSE(first->outlets().empty());
    doc->context().selectionStack.pushNewSelection({first});

    pressTab(*doc);

    // Only what can be cabled after its outlet, as from the outlet itself
    auto* picker = dynamic_cast<Library::ProcessPicker*>(QApplication::activePopupWidget());
    REQUIRE(picker);
    CHECK(picker->findChild<QToolButton*>("ProcessPickerChipClear"));
    const auto type = first->outlets()[0]->type();
    for(auto& f : app.interfaces<Process::ProcessFactoryList>())
    {
      const auto d = f.descriptor(QString{});
      if(!d.inlets || ossia::contains(*d.inlets, type)
         || (f.flags() & Process::ProcessFlags::Deprecated))
        continue;
      picker->setSearch(f.prettyName());
      CHECK_FALSE(picker->processes().contains(f.prettyName()));
      break;
    }
    picker->setSearch({});

    const auto before = itv.processes.size();
    const auto cablesBefore = docModel(*doc).cables.size();
    REQUIRE(pickFloat());
    run_events_for(50);
    REQUIRE(itv.processes.size() == before + 1);
    REQUIRE(docModel(*doc).cables.size() == cablesBefore + 1);

    const auto added = processesBut(itv, first);
    REQUIRE(added.size() == 1);
    auto& cable = *docModel(*doc).cables.begin();
    CHECK(&cable.source().find(doc->context()) == first->outlets()[0]);
    CHECK(&cable.sink().find(doc->context()) == added[0]->inlets()[0]);

    doc->commandStack().undo();
    CHECK(itv.processes.size() == before);
    CHECK(docModel(*doc).cables.size() == cablesBefore);
  });
}

TEST_CASE(
    "Tab over the emptiness of a scenario makes a box there with the process, "
    "started on play",
    "[integration][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    run_events_for(200);
    auto& scenario = score::test::base_scenario(*doc);
    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    auto& gv = pr->view().view();

    // The mouse in the visible part of the scenario, away from its start
    const Scenario::ScenarioView* sv{};
    for(auto item : gv.scene()->items())
      if(item->type() == Scenario::ScenarioView::Type)
        if(auto v = static_cast<const Scenario::ScenarioView*>(item);
           v->presenter() && &v->presenter()->model() == &scenario)
          sv = v;
    REQUIRE(sv);
    const QRectF visible
        = sv->mapFromScene(gv.mapToScene(gv.viewport()->rect())).boundingRect()
          & sv->boundingRect();
    REQUIRE(visible.width() > 100);
    const QPointF local{
        visible.left() + visible.width() * 0.4, visible.top() + visible.height() * 0.6};
    QCursor::setPos(gv.viewport()->mapToGlobal(gv.mapFromScene(sv->mapToScene(local))));
    run_events_for(100);
    // Where the mouse is, on a whole pixel
    const auto expected = sv->presenter()->toScenarioPoint(
        sv->mapFromScene(gv.mapToScene(gv.viewport()->mapFromGlobal(QCursor::pos()))));

    // Clicking in it selects the scenario: it is not where the process goes
    doc->context().selectionStack.pushNewSelection({&scenario});
    const auto intervalsBefore = scenario.intervals.size();
    pressTab(*doc);
    REQUIRE(pickFloat());
    run_events_for(50);
    REQUIRE(scenario.intervals.size() == intervalsBefore + 1);

    const Scenario::IntervalModel* box{};
    for(auto& i : scenario.intervals)
      for(auto& p : i.processes)
        if(p.concreteKey() == float_key)
          box = &i;
    REQUIRE(box);
    CHECK(box->processes.size() == 1);
    CHECK(std::abs(box->date().msec() - expected.date.msec()) < 1.);
    CHECK(std::abs(box->heightPercentage() - expected.y) < 0.01);
    auto& ts = Scenario::startTimeSync(*box, scenario);
    CHECK(ts.isStartPoint());
    CHECK(ts.active());

    // One undo step
    doc->commandStack().undo();
    CHECK(scenario.intervals.size() == intervalsBefore);
  });
}

TEST_CASE(
    "Replace in a node's menu picks what replaces it, as dropping on it does",
    "[integration][nodal][picker][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& itv = newInterval(*doc);
    // Audio processes: a drop keeps their signal cables
    const auto merger = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7"));
    const auto add = [&](QPointF pos) {
      Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
      auto proc = m.createProcessInNewSlot(itv, merger, {}, pos);
      m.commit();
      run_events_for(100);
      return proc;
    };
    auto* source = add(QPointF{100, 100});
    auto* old = add(QPointF{300, 100});
    REQUIRE(source);
    REQUIRE(old);
    connect(*doc, *source, *old);
    const QPointF oldPos = old->position();
    QPointer<Process::ProcessModel> gone = old;

    auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(pr);
    pr->setDisplayedInterval(&itv);
    pr->setNodalMode(true);
    run_events_for(200);
    auto* node = nodeOf(*doc, *old);
    REQUIRE(node);
    const QPointF at = node->mapToScene(QPointF{10., 6.});
    auto& gv = pr->view().view();
    gv.centerOn(at);
    run_events_for(30);
    const QPoint vp = gv.mapFromScene(at);

    // Replace, from the menu of the node
    bool found = false;
    QTimer::singleShot(50, [&] {
      auto m = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if(!m)
        return;
      for(auto* a : m->actions())
        if(a->text() == "Replace")
        {
          found = true;
          m->close();
          a->trigger();
          return;
        }
      m->close();
    });
    QContextMenuEvent ev{QContextMenuEvent::Mouse, vp, gv.viewport()->mapToGlobal(vp)};
    QApplication::sendEvent(gv.viewport(), &ev);
    run_events_for(100);
    REQUIRE(found);

    // The picker says what it will do
    auto* picker = dynamic_cast<Library::ProcessPicker*>(QApplication::activePopupWidget());
    REQUIRE(picker);
    bool says = false;
    for(auto* l : picker->findChildren<QLabel*>())
      says |= l->text().startsWith("Replace ");
    CHECK(says);

    const auto before = itv.processes.size();
    picker->setSearch(app.interfaces<Process::ProcessFactoryList>().get(merger)->prettyName());
    REQUIRE(picker->accept());
    run_events_for(100);

    // In its place, with its cable
    CHECK(gone.isNull());
    REQUIRE(itv.processes.size() == before);
    const auto others = processesBut(itv, source);
    REQUIRE(others.size() == 1);
    const auto* replacement = others.front();
    CHECK(QLineF{replacement->position(), oldPos}.length() < 1.);
    CHECK(cableBetween(*doc, *source, *replacement));

    // One undo step brings the old one back
    doc->commandStack().undo();
    run_events_for(50);
    CHECK(itv.processes.size() == before);
    CHECK(processesBut(itv, source).front()->position() == oldPos);
  });
}

namespace
{
const Scenario::ScenarioPresenter* presenterOf(score::Document& doc, const Scenario::ProcessModel& scenario)
{
  auto pr = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(doc);
  if(!pr)
    return nullptr;
  for(auto item : pr->view().view().scene()->items())
    if(item->type() == Scenario::ScenarioView::Type)
      if(auto v = static_cast<const Scenario::ScenarioView*>(item);
         v->presenter() && &v->presenter()->model() == &scenario)
        return v->presenter();
  return nullptr;
}
}

TEST_CASE(
    "A snippet added in the emptiness of a scenario starts on play, as a state made by "
    "double-clicking",
    "[integration][snippets][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    run_events_for(200);
    auto& scenario = score::test::base_scenario(*doc);

    // A box saved as a snippet
    auto& itv = newInterval(*doc);
    itv.selection.set(true);
    JSONReader r;
    Scenario::copySelectedScenarioElements(r, scenario);
    itv.selection.set(false);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto file = Scenario::writeSnippet(r.toByteArray(), dir.path(), "Box", nullptr);
    REQUIRE(file);

    auto* sp = presenterOf(*doc, scenario);
    REQUIRE(sp);
    std::vector<const Scenario::TimeSyncModel*> before;
    for(auto& ts : scenario.timeSyncs)
      before.push_back(&ts);
    REQUIRE(Scenario::insertSnippet(*sp, QPointF{400., 200.}, *file));

    std::vector<const Scenario::TimeSyncModel*> added;
    for(auto& ts : scenario.timeSyncs)
      if(std::find(before.begin(), before.end(), &ts) == before.end())
        added.push_back(&ts);
    REQUIRE(added.size() == 2);
    // Its start starts on play, not its end
    int starting = 0;
    for(auto* ts : added)
      if(ts->isStartPoint() && ts->active())
        starting++;
    CHECK(starting == 1);

    // One undo step
    doc->commandStack().undo();
    CHECK(scenario.timeSyncs.size() == before.size());
  });
}
