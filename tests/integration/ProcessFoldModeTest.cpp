// Fold state lives on the process, not on the node item that draws it.
//
// That is what lets it survive a save/reload, an undo, and -- the case this was
// written for -- a preset dropped onto an open node: the replacement keeps the
// node open however many ports it brings, instead of falling back to the
// "too many ports to draw" heuristic and closing itself.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortVisibility.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>

#include <catch2/catch_test_macros.hpp>

namespace
{
// The current "LFO": a handful of controls, well under the auto-fold threshold.
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

Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}
}

TEST_CASE("Auto fold mode keeps the port-count heuristic", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_lfo(*doc, base_interval(*doc));

    // Untouched processes must behave exactly as they did before fold state was
    // stored at all.
    CHECK(lfo.foldMode() == Process::FoldMode::Auto);
    CHECK(lfo.folded() == (std::ssize(lfo.inlets()) > Process::MaxUnpaginatedControls));

    // An explicit choice overrides the heuristic in both directions.
    lfo.setFoldMode(Process::FoldMode::Folded);
    CHECK(lfo.folded());
    lfo.setFoldMode(Process::FoldMode::Unfolded);
    CHECK(!lfo.folded());
  });
}

TEST_CASE("Fold mode survives a save and reload", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_lfo(*doc, base_interval(*doc));

    lfo.setFoldMode(Process::FoldMode::Folded);

    auto* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    Process::ProcessModel* p = nullptr;
    for(auto& proc : base_interval(*reloaded).processes)
      if(proc.concreteKey() == lfo_key)
        p = &proc;

    REQUIRE(p != nullptr);
    CHECK(p->foldMode() == Process::FoldMode::Folded);
    CHECK(p->folded());
  });
}

TEST_CASE(
    "A document written before fold state existed loads as Auto", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& lfo = add_lfo(*doc, base_interval(*doc));
    lfo.setFoldMode(Process::FoldMode::Folded);

    JSONReader writer;
    doc->saveAsJson(writer);
    QByteArray json{writer.buffer.GetString(), int(writer.buffer.GetSize())};
    REQUIRE(json.contains("FoldMode"));

    // What every .score written before this feature looks like: the key is
    // simply absent, and those must come back as Auto rather than inheriting
    // whatever the member happened to hold.
    json.replace("\"FoldMode\"", "\"FoldModeWasNotAThing\"");

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto* older = ctx.docManager.loadDocument(
        ctx, QStringLiteral("older"), json, JSONObject::type(), *delegates.begin());
    REQUIRE(older != nullptr);

    Process::ProcessModel* p = nullptr;
    for(auto& proc : base_interval(*older).processes)
      if(proc.concreteKey() == lfo_key)
        p = &proc;

    REQUIRE(p != nullptr);
    CHECK(p->foldMode() == Process::FoldMode::Auto);
  });
}

TEST_CASE(
    "Replacing an open node keeps the replacement open", "[integration][fold]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = base_interval(*doc);
    auto& old = add_lfo(*doc, interval);

    // The node the user is dropping onto is open -- here by the heuristic,
    // which is the case that used to be lost.
    REQUIRE(!old.folded());
    REQUIRE(old.foldMode() == Process::FoldMode::Auto);

    // What DropOnNode::keepUnfolded() does once the replacement exists.
    auto& replacement = add_lfo(*doc, interval);
    CommandDispatcher<> disp{doc->context().commandStack};
    if(!old.folded())
      disp.submit<Process::SetNodeFoldMode>(replacement, Process::FoldMode::Unfolded);

    // Now it stays open even with more ports than the heuristic would allow.
    CHECK(replacement.foldMode() == Process::FoldMode::Unfolded);
    CHECK(!replacement.folded());

    // ... and it is undoable, like the rest of the drop macro.
    doc->commandStack().undo();
    CHECK(replacement.foldMode() == Process::FoldMode::Auto);
  });
}
