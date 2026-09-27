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
