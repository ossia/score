// An interval's nodal slot goes with its last node, and only then, whatever
// the slot's own list of processes says.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

int nodalSlot(const Scenario::IntervalModel& itv)
{
  const auto& sv = itv.smallView();
  for(int i = 0; i < int(sv.size()); i++)
    if(sv[i].nodal)
      return i;
  return -1;
}

bool hasFullNodalSlot(const Scenario::IntervalModel& itv)
{
  for(const auto& slt : itv.fullView())
    if(slt.nodal)
      return true;
  return false;
}

void remove(score::Document& doc, Scenario::IntervalModel& itv, Process::ProcessModel& p)
{
  CommandDispatcher<>{doc.context().commandStack}.submit(
      new Scenario::Command::RemoveProcessFromInterval{itv, p.id()});
}
}

TEST_CASE(
    "removing the last node removes the interval's nodal slot",
    "[integration][scenario][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = score::test::base_interval(*doc);
    auto* lfo = score::test::add_process(*doc, lfo_uuid, {});
    if(!lfo)
      SKIP("LFO not built");
    const int slot = nodalSlot(itv);
    REQUIRE(slot >= 0);

    // A list that names a process the interval no longer has.
    auto sv = itv.smallView();
    sv[slot].processes.push_back(Id<Process::ProcessModel>{12345});
    itv.replaceSmallView(sv);

    remove(*doc, itv, *lfo);
    CHECK(nodalSlot(itv) == -1);
    CHECK_FALSE(hasFullNodalSlot(itv));

    doc->commandStack().undo();
    CHECK(nodalSlot(itv) >= 0);
  });
}

TEST_CASE(
    "removing a node keeps the nodal slot while others remain",
    "[integration][scenario][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = score::test::base_interval(*doc);
    auto* a = score::test::add_process(*doc, lfo_uuid, {});
    if(!a)
      SKIP("LFO not built");
    auto* b = score::test::add_process(*doc, lfo_uuid, {});
    REQUIRE(b);
    const int slot = nodalSlot(itv);
    REQUIRE(slot >= 0);

    // What undoing a paste does: the slots of before the paste come back first,
    // without the pasted node, which is removed after.
    auto sv = itv.smallView();
    std::erase(sv[slot].processes, b->id());
    itv.replaceSmallView(sv);

    remove(*doc, itv, *b);
    CHECK(nodalSlot(itv) >= 0);
    CHECK(hasFullNodalSlot(itv));

    remove(*doc, itv, *a);
    CHECK(nodalSlot(itv) == -1);
    CHECK_FALSE(hasFullNodalSlot(itv));
  });
}
