// A process moved into a box of the scenario: dragged by its slot header in
// the full view of its own box, or from the object list.
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Effect/EffectLayer.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/ObjectTree/ObjectItemModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Midi/MidiProcess.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QMimeData>


#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Process.hpp>

namespace
{
struct Boxes
{
  score::Document* doc{};
  Scenario::IntervalModel* root{};
  Scenario::IntervalModel* box{};
  Midi::ProcessModel* roll{};
};

//! A piano roll in the root, and an empty box of the scenario
Boxes make(const score::GUIApplicationContext& ctx)
{
  Boxes b;
  b.doc = score::test::new_document(ctx);
  REQUIRE(b.doc);
  b.root = &score::test::base_interval(*b.doc);
  auto& scenar = score::test::base_scenario(*b.doc);
  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, b.doc->context()};
  b.roll = static_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
      *b.root, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), {}));
  auto& from = m.createState(scenar, scenar.startEvent().id(), 0.4);
  b.box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(4000), 0.4});
  m.commit();
  REQUIRE(b.roll);
  return b;
}

//! What SlotHeader::dragMimeData puts in a drag of a full view's slot
QByteArray slotHeaderDrag(const Scenario::IntervalModel& itv, const Process::ProcessModel& proc)
{
  int slot = -1;
  for(std::size_t i = 0; i < itv.fullView().size(); i++)
    if(itv.fullView()[i].process == proc.id())
      slot = int(i);
  REQUIRE(slot >= 0);
  JSONReader r;
  r.stream.StartObject();
  Process::copyProcess(r, proc);
  r.obj["Path"] = score::IDocument::path(proc);
  r.obj["Duration"] = itv.duration.defaultDuration().msec();
  r.obj["SlotIndex"] = slot;
  r.obj["View"] = QStringLiteral("Full");
  r.stream.EndObject();
  return r.toByteArray();
}

}

TEST_CASE("A process dragged by its slot header in the full view moves into the box it is dropped on", "[drop][process][move]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto b = make(ctx);
    const auto root_before = b.root->processes.size();
    const int index = b.doc->commandStack().currentIndex();

    QMimeData mime;
    mime.setData(score::mime::layerdata(), slotHeaderDrag(*b.root, *b.roll));
    // As the interval of the scenario does on a drop
    REQUIRE(ctx.interfaces<Scenario::IntervalDropHandlerList>().drop(
        b.doc->context(), *b.box, {}, mime));
    score::test::settle();

    // Moved, the same process, in one step
    CHECK(b.root->processes.size() == root_before - 1);
    REQUIRE(b.box->processes.size() == 1);
    CHECK(b.box->processes.begin()->concreteKey() == Metadata<ConcreteKey_k, Midi::ProcessModel>::get());
    CHECK(b.doc->commandStack().currentIndex() == index + 1);
    CHECK(b.box->smallView().size() == 1);

    b.doc->commandStack().undo();
    CHECK(b.root->processes.size() == root_before);
    CHECK(b.box->processes.empty());
  });
}

TEST_CASE("A process dragged from the object list moves into the box it is dropped on", "[drop][process][move][objects]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto b = make(ctx);
    const auto root_before = b.root->processes.size();

    Scenario::ObjectItemModel model{b.doc->context(), nullptr};
    model.setSelected({b.root});
    // The roll's row, under its interval
    QModelIndex roll_index;
    std::function<void(const QModelIndex&)> find = [&](const QModelIndex& parent) {
      for(int r = 0; r < model.rowCount(parent); r++)
      {
        auto i = model.index(r, 0, parent);
        if(i.internalPointer() == b.roll)
          roll_index = i;
        find(i);
      }
    };
    find({});
    REQUIRE(roll_index.isValid());
    std::unique_ptr<QMimeData> mime{model.mimeData({roll_index})};
    REQUIRE(mime);

    // Dropped on a box of the scenario view
    REQUIRE(ctx.interfaces<Scenario::IntervalDropHandlerList>().drop(
        b.doc->context(), *b.box, {}, *mime));
    score::test::settle();
    CHECK(b.root->processes.size() == root_before - 1);
    REQUIRE(b.box->processes.size() == 1);
    CHECK(b.box->processes.begin()->concreteKey() == Metadata<ConcreteKey_k, Midi::ProcessModel>::get());
    b.doc->commandStack().undo();
    CHECK(b.root->processes.size() == root_before);
    CHECK(b.box->processes.empty());

    // Within the list: dropped onto the box's row
    QModelIndex box_index;
    std::function<void(const QModelIndex&)> find_box = [&](const QModelIndex& parent) {
      for(int r = 0; r < model.rowCount(parent); r++)
      {
        auto i = model.index(r, 0, parent);
        if(i.internalPointer() == b.box)
          box_index = i;
        find_box(i);
      }
    };
    // The list shows the tree of what is selected
    model.setSelected({b.box});
    find_box({});
    REQUIRE(box_index.isValid());
    CHECK(model.canDropMimeData(mime.get(), Qt::MoveAction, -1, -1, box_index));
    REQUIRE(model.dropMimeData(mime.get(), Qt::MoveAction, -1, -1, box_index));
    score::test::settle();
    REQUIRE(b.box->processes.size() == 1);
    CHECK(b.box->processes.begin()->concreteKey() == Metadata<ConcreteKey_k, Midi::ProcessModel>::get());
    CHECK(b.root->processes.size() == root_before - 1);
  });
}
