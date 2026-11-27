// A slot of the full view dragged by its header: dropped on an interval of the
// scenario shown in another slot, its process moves into that interval; dropped
// between slots, the slot moves. Through the scene, as the mouse does.
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Effect/EffectLayer.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Midi/MidiProcess.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Process.hpp>

namespace
{
void sendDrag(QGraphicsScene& scene, QGraphicsItem& to, QEvent::Type type, QPointF scenePos, QMimeData& mime)
{
  QGraphicsSceneDragDropEvent e{type};
  e.setScenePos(scenePos);
  e.setPos(to.mapFromScene(scenePos));
  e.setMimeData(&mime);
  e.setPossibleActions(Qt::MoveAction | Qt::CopyAction);
  e.setProposedAction(Qt::MoveAction);
  e.setDropAction(Qt::MoveAction);
  scene.sendEvent(&to, &e);
}
}

TEST_CASE("A slot dragged by its header in the full view, dropped on an interval of the scenario, moves its process there", "[drop][process][move][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& root = score::test::base_interval(*doc);
    auto& scenar = score::test::base_scenario(*doc);
    Midi::ProcessModel* roll{};
    Scenario::IntervalModel* box{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      roll = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
          root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.2);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(8000), 0.2});
      m.commit();
    }
    REQUIRE(roll);
    box->metadata().setName(QStringLiteral("target"));
    score::test::settle();

    auto* presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(presenter);
    auto& scene = presenter->view().scene();
    const Scenario::IntervalPresenter* full{};
    Scenario::TemporalIntervalView* target{};
    for(auto* item : scene.items())
    {
      if(auto* v = dynamic_cast<Scenario::IntervalView*>(item);
         v && &v->presenter().model() == &root)
        full = &v->presenter();
      if(auto* v = dynamic_cast<Scenario::TemporalIntervalView*>(item);
         v && v->presenter().model().metadata().getName() == QStringLiteral("target"))
        target = v;
    }
    REQUIRE(full);
    REQUIRE(&full->model() == &root);
    REQUIRE(target);

    // What the roll's slot header puts in the drag
    int slot = -1;
    for(std::size_t i = 0; i < root.fullView().size(); i++)
      if(root.fullView()[i].process == roll->id())
        slot = int(i);
    REQUIRE(slot >= 0);
    QByteArray data;
    {
      JSONReader r;
      r.stream.StartObject();
      Process::copyProcess(r, *roll);
      r.obj["Path"] = score::IDocument::path(*roll);
      r.obj["Duration"] = root.duration.defaultDuration().msec();
      r.obj["SlotIndex"] = slot;
      r.obj["View"] = QStringLiteral("Full");
      r.stream.EndObject();
      data = r.toByteArray();
    }
    QMimeData mime;
    mime.setData(score::mime::layerdata(), data);

    // The drag starts: the overlay covers the full view
    full->startSlotDrag(slot, {});
    Scenario::SlotDragOverlay* overlay{};
    for(auto* item : scene.items())
      if(auto* o = dynamic_cast<Scenario::SlotDragOverlay*>(item))
        overlay = o;
    REQUIRE(overlay);

    // Onto the box's line, where an interval takes drops
    const QPointF at = target->mapToScene(QPointF{10., 2.});
    REQUIRE(overlay->intervalBelow(at) != nullptr);
    const auto root_before = root.processes.size();
    sendDrag(scene, *overlay, QEvent::GraphicsSceneDragEnter, at, mime);
    sendDrag(scene, *overlay, QEvent::GraphicsSceneDragMove, at, mime);
    sendDrag(scene, *overlay, QEvent::GraphicsSceneDrop, at, mime);
    full->stopSlotDrag();
    score::test::settle();

    CHECK(root.processes.size() == root_before - 1);
    REQUIRE(box->processes.size() == 1);
    CHECK(box->processes.begin()->concreteKey()
          == Metadata<ConcreteKey_k, Midi::ProcessModel>::get());
    doc->commandStack().undo();
    score::test::settle();
    CHECK(root.processes.size() == root_before);
    CHECK(box->processes.empty());

    // Between slots, away from any interval: the slot moves, nothing else
    full->startSlotDrag(slot, {});
    overlay = nullptr;
    for(auto* item : scene.items())
      if(auto* o = dynamic_cast<Scenario::SlotDragOverlay*>(item))
        overlay = o;
    REQUIRE(overlay);
    const QPointF top = overlay->mapToScene(QPointF{5., 0.});
    CHECK(overlay->intervalBelow(top) == nullptr);
    const int index = doc->commandStack().currentIndex();
    sendDrag(scene, *overlay, QEvent::GraphicsSceneDrop, top, mime);
    full->stopSlotDrag();
    score::test::settle();
    CHECK(root.processes.size() == root_before);
    CHECK(box->processes.empty());
    CHECK(root.fullView().front().process == roll->id());
    CHECK(doc->commandStack().currentIndex() == index + 1);
  });
}
