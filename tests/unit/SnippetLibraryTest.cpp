// Snippets: .scenario files the process library lists under its Snippets
// branch, in the folders they are saved in.

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Scenario/Library/Snippets.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Media/Step/Model.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <Library/Snippet.hpp>

#include <QDir>
#include <QMimeData>
#include <QUrl>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>

namespace
{
std::string utf8(const QString& s)
{
  return s.toStdString();
}
}

TEST_CASE("A snippet is listed under the folders below its snippets folder", "[snippets]")
{
  using Scenario::snippetCategories;
  CHECK(snippetCategories("/root/packages/user/snippets").isEmpty());
  CHECK(snippetCategories("/root/packages/user/snippets/Drums") == QStringList{"Drums"});
  CHECK(
      snippetCategories("/root/packages/user/snippets/Drums/Kits")
      == (QStringList{"Drums", "Kits"}));
  // A package's own snippets folder.
  CHECK(
      snippetCategories("/root/packages/a-package/snippets/Fx") == QStringList{"Fx"});
  // Saved elsewhere, by a drop in the library's file view: its folder.
  CHECK(snippetCategories("/root/packages/user/sketches") == QStringList{"sketches"});
}

TEST_CASE("Only .scenario files are snippets", "[snippets]")
{
  const auto entry = Scenario::snippetEntry("/root/packages/user/snippets/Drums/Kick.scenario");
  REQUIRE(entry);
  CHECK(entry->rootKey == Library::snippetKey());
  CHECK(entry->categoryPath == QStringList{"Drums"});
  CHECK(entry->node.data.key == Library::snippetKey());
  CHECK(entry->node.data.prettyName == "Kick");
  CHECK(entry->node.data.customData == "/root/packages/user/snippets/Drums/Kick.scenario");

  CHECK_FALSE(Scenario::snippetEntry("/root/packages/user/cues/Messages.cues"));
  CHECK_FALSE(Scenario::snippetEntry("/root/packages/user/snippets/Song.score"));
}

TEST_CASE("Saving a snippet writes it below the snippets folder", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QByteArray json = R"({"Intervals":[],"States":[],"Cables":[]})";

    const auto file = Scenario::writeSnippet(json, dir.path(), "Drums/Kick", nullptr);
    REQUIRE(file);
    CHECK(*file == dir.path() + "/snippets/Drums/Kick.scenario");
    QFile f{*file};
    REQUIRE(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == json);

    // No way out of the snippets folder, and no empty name.
    const auto escaped = Scenario::writeSnippet(json, dir.path(), "../../Out", nullptr);
    if(escaped)
      CHECK(escaped->startsWith(dir.path() + "/snippets/"));
    CHECK_FALSE(Scenario::writeSnippet(json, dir.path(), "  ", nullptr));
  });
}

TEST_CASE(
    "A snippet's processes join an interval with the cables between them, even "
    "across its boxes",
    "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->context().document);

    // Two boxes: an automation in one drives the step sequencer in the other.
    Scenario::IntervalModel* a{};
    Scenario::IntervalModel* b{};
    Scenario::IntervalModel* target{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      a = &m.createBox(scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(3000), 0.1);
      b = &m.createBox(scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(3000), 0.5);
      target
          = &m.createBox(scenario, TimeVal::fromMsecs(5000), TimeVal::fromMsecs(8000), 0.3);
      auto autom = m.createProcessInNewSlot(
          *a, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{});
      auto step = m.createProcessInNewSlot(
          *b, Metadata<ConcreteKey_k, Media::Step::Model>::get(), QString{});
      REQUIRE(autom);
      REQUIRE(step);
      m.createCable(
          document, *autom->outlets().front(), *step->inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.commit();
    }
    const auto cables_before = document.cables.size();

    // Saved from a selection of the two boxes.
    a->selection.set(true);
    b->selection.set(true);
    JSONReader r;
    Scenario::copySelectedScenarioElements(r, scenario);
    a->selection.set(false);
    b->selection.set(false);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto file = Scenario::writeSnippet(r.toByteArray(), dir.path(), "Two", nullptr);
    REQUIRE(file);

    REQUIRE(Scenario::insertSnippetInInterval(
        doc->context(), *target, QPointF{500., 300.}, *file));

    REQUIRE(target->processes.size() == 2);
    const Process::ProcessModel* new_autom{};
    const Process::ProcessModel* new_step{};
    for(auto& p : target->processes)
    {
      if(qobject_cast<const Automation::ProcessModel*>(&p))
        new_autom = &p;
      if(qobject_cast<const Media::Step::Model*>(&p))
        new_step = &p;
    }
    REQUIRE(new_autom);
    REQUIRE(new_step);

    // The layout moved as a whole, its top-left corner where it was put.
    const double left = std::min(new_autom->position().x(), new_step->position().x());
    const double top = std::min(new_autom->position().y(), new_step->position().y());
    CHECK(left == 500.);
    CHECK(top == 300.);

    // The cable joins the copies, not the originals.
    REQUIRE(document.cables.size() == cables_before + 1);
    const auto& out = *new_autom->outlets().front();
    const auto& in = *new_step->inlets().front();
    REQUIRE(out.cables().size() == 1);
    REQUIRE(in.cables().size() == 1);
    CHECK(out.cables().front() == in.cables().front());

    // One undo removes it all.
    doc->commandStack().undo();
    CHECK(target->processes.size() == 0);
    CHECK(document.cables.size() == cables_before);

    // Dropped on the box or in the node view: the same, through the drop
    // handlers of an interval.
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(*file)});
    CHECK(ctx.interfaces<Scenario::IntervalDropHandlerList>().drop(
        doc->context(), *target, QPointF{10., 10.}, mime));
    CHECK(target->processes.size() == 2);
    CHECK(document.cables.size() == cables_before + 1);
  });
}
