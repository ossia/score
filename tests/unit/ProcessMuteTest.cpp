// A process can be muted on its own: it does not run (ossia::time_process::mute
// mutes its node, which the graph then leaves out). Alternative takes of a
// recording are processes of the same box of which only one is unmuted.
//
// What has to hold:
//  * the mute is a document edit, undoable, saved in both formats, and old
//    documents (without the key) load unmuted;
//  * it reaches the running process, and its undo too;
//  * a muted box mutes every process, and unmuting it keeps the processes
//    muted on their own muted (ossia unmutes them all).
//
// The execution queue is drained by hand (runAllCommands): nothing plays.

#include <Process/Commands/Properties.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_process.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

namespace
{
Process::ProcessModel& add_automation(score::Document& doc, Scenario::IntervalModel& itv)
{
  const auto key = Metadata<ConcreteKey_k, Automation::ProcessModel>::get();
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::Command::AddOnlyProcessToInterval>(
          itv, key, QString{}, QPointF{});
  Process::ProcessModel* added = nullptr;
  for(auto& p : itv.processes)
    if(p.concreteKey() == key)
      added = &p;
  REQUIRE(added != nullptr);
  return *added;
}

void set_muted(score::Document& doc, Process::ProcessModel& p, bool b)
{
  CommandDispatcher<>{doc.context().commandStack}.submit<Process::SetProcessMuted>(p, b);
}

std::shared_ptr<ossia::time_process>
exec_process(Execution::DocumentPlugin& plug, const Process::ProcessModel& proc)
{
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(proc.id());
  REQUIRE(it != procs.end());
  REQUIRE(it->second);
  REQUIRE(it->second->OSSIAProcessPtr());
  return it->second->OSSIAProcessPtr();
}

bool runs(const ossia::time_process& p)
{
  return p.unmuted() && !(p.node && p.node->muted());
}
}

TEST_CASE(
    "A process mute is an undoable edit, saved, absent from old documents",
    "[process][mute]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = score::test::base_interval(*doc);
    auto& proc = add_automation(*doc, itv);
    CHECK(!proc.muted());

    set_muted(*doc, proc, true);
    CHECK(proc.muted());
    doc->commandStack().undo();
    CHECK(!proc.muted());
    doc->commandStack().redo();
    CHECK(proc.muted());

    const auto id = proc.id();
    for(auto* reloaded :
        {score::test::reload_via_json(ctx, *doc),
         score::test::reload_via_bytes(ctx, *doc)})
    {
      REQUIRE(reloaded);
      auto& ritv = score::test::base_interval(*reloaded);
      auto it = ritv.processes.find(id);
      REQUIRE(it != ritv.processes.end());
      CHECK(it->muted());
    }

    // A document written before the key existed: unmuted.
    {
      const QByteArray json = score::test::save_as_json(*doc);
      QByteArray old = json;
      old.replace("\"Muted\":true,", "");
      old.replace(",\"Muted\":true", "");
      REQUIRE(!old.contains("\"Muted\""));
      auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
      auto* loaded = ctx.docManager.loadDocument(
          ctx, "old", old, JSONObject::type(), *delegates.begin());
      REQUIRE(loaded);
      QApplication::processEvents();
      auto& litv = score::test::base_interval(*loaded);
      auto it = litv.processes.find(id);
      REQUIRE(it != litv.processes.end());
      CHECK(!it->muted());
    }
  });
}

TEST_CASE("A process mute reaches the running process", "[process][mute][exec]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = score::test::base_interval(*doc);
    auto& a = add_automation(*doc, itv);
    auto& b = add_automation(*doc, itv);
    set_muted(*doc, b, true);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, itv);
    score::test::run_exec(plug);

    auto pa = exec_process(plug, a);
    auto pb = exec_process(plug, b);
    CHECK(runs(*pa));
    CHECK(!runs(*pb));

    // Muting, and undoing it
    set_muted(*doc, a, true);
    score::test::run_exec(plug);
    CHECK(!runs(*pa));
    doc->commandStack().undo();
    score::test::run_exec(plug);
    CHECK(runs(*pa));

    // A muted box mutes everything; unmuting it keeps b muted
    itv.setMuted(true);
    score::test::run_exec(plug);
    CHECK(!runs(*pa));
    CHECK(!runs(*pb));
    itv.setMuted(false);
    score::test::run_exec(plug);
    CHECK(runs(*pa));
    CHECK(!runs(*pb));

    // Unmuting b while the box is muted leaves it muted until the box is
    itv.setMuted(true);
    score::test::run_exec(plug);
    set_muted(*doc, b, false);
    score::test::run_exec(plug);
    CHECK(!runs(*pb));
    itv.setMuted(false);
    score::test::run_exec(plug);
    CHECK(runs(*pb));

    plug.clear();
  });
}

#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

TEST_CASE("A mute holds through nested containers", "[ossia][mute]")
{
  // A scenario (a process of an outer box) holds a box, which holds a
  // process: each can be muted on its own, and unmuting a container never
  // unmutes what is muted on its own inside it.
  auto make_proc = [] {
    return std::make_shared<ossia::node_process>(
        std::make_shared<ossia::nodes::forward_node>());
  };

  auto outer_start = std::make_shared<ossia::time_sync>();
  auto outer_end = std::make_shared<ossia::time_sync>();
  auto& os = **outer_start->emplace(outer_start->get_time_events().begin(), {});
  auto& oe = **outer_end->emplace(outer_end->get_time_events().begin(), {});
  auto outer = ossia::time_interval::create({}, os, oe, ossia::time_value{1000});

  auto scenar = std::make_shared<ossia::scenario>();
  outer->add_time_process(scenar);

  auto s1 = std::make_shared<ossia::time_sync>();
  auto s2 = std::make_shared<ossia::time_sync>();
  auto& e1 = **s1->emplace(s1->get_time_events().begin(), {});
  auto& e2 = **s2->emplace(s2->get_time_events().begin(), {});
  auto inner = ossia::time_interval::create({}, e1, e2, ossia::time_value{500});
  scenar->add_time_sync(s1);
  scenar->add_time_sync(s2);
  scenar->add_time_interval(inner);

  auto take = make_proc();
  auto other = make_proc();
  inner->add_time_process(take);
  inner->add_time_process(other);

  auto runs
      = [](const ossia::time_process& p) { return p.unmuted() && !p.node->muted(); };

  take->mute(true);
  CHECK(!runs(*take));
  CHECK(runs(*other));

  // The outer box, muted then unmuted
  outer->mute(true);
  CHECK(!runs(*other));
  CHECK(!runs(*take));
  CHECK(inner->node->muted());
  outer->mute(false);
  CHECK(runs(*other));
  CHECK(!runs(*take));
  CHECK(!inner->node->muted());

  // The inner box muted on its own stays muted when the outer one is unmuted
  inner->mute(true);
  outer->mute(true);
  outer->mute(false);
  CHECK(inner->node->muted());
  CHECK(!runs(*other));
  inner->mute(false);
  CHECK(runs(*other));
  CHECK(!runs(*take));

  // Unmuting the take while its box is muted leaves it muted until the box is
  inner->mute(true);
  take->mute(false);
  CHECK(!runs(*take));
  inner->mute(false);
  CHECK(runs(*take));

  // A process added to a muted box starts muted, and runs once it is unmuted
  inner->mute(true);
  auto late = make_proc();
  inner->add_time_process(late);
  CHECK(!runs(*late));
  inner->mute(false);
  CHECK(runs(*late));
}
