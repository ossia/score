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
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Scenario/Library/Snippets.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Media/Step/Model.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <Library/ProcessesItemModel.hpp>
#include <Library/PresetFile.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/Snippet.hpp>

#include <QDir>
#include <QMimeData>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QImage>
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

//! The user library in a temporary folder while it lives
struct TemporaryUserLibrary
{
  TemporaryUserLibrary()
  {
    auto& set = score::GUIAppContext().settings<Library::Settings::Model>();
    old = set.getRootPath();
    set.setRootPath(dir.path());
    snippets = set.getUserLibraryPath() + "/snippets";
  }
  ~TemporaryUserLibrary()
  {
    score::GUIAppContext().settings<Library::Settings::Model>().setRootPath(old);
  }
  QTemporaryDir dir;
  QString old;
  QString snippets;
};

void writeFile(const QString& path, const QByteArray& data)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(data);
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

TEST_CASE("A snippet is renamed in the library: its file and its place", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Built before the user library moves: it does not scan it
    Library::ProcessesItemModel model{ctx, nullptr};
    model.flushPending();
    TemporaryUserLibrary user;
    const QString folder = user.snippets + "/Drums";
    for(const char* name : {"Kick", "Snare"})
      writeFile(folder + "/" + name + ".scenario", R"({"Intervals":[],"States":[],"Cables":[]})");
    for(const char* name : {"Kick", "Snare"})
      if(auto e = Scenario::snippetEntry(
             QString{folder + "/" + name + ".scenario"}.toStdString()))
        model.publish(std::move(*e));
    model.flushPending();

    const auto kick = model.find(Library::snippetKey(), folder + "/Kick.scenario");
    REQUIRE(kick.isValid());
    const auto drums = kick.parent();
    REQUIRE(model.rowCount(drums) == 2);
    CHECK(model.flags(kick) & Qt::ItemIsEditable);

    // A name with a folder in it, an empty one, one no file can have, one
    // taken: refused.
    CHECK_FALSE(model.setData(kick, "Up/Kick", Qt::EditRole));
    CHECK_FALSE(model.setData(kick, "  ", Qt::EditRole));
    CHECK_FALSE(model.setData(kick, "Kick: hard", Qt::EditRole));
    CHECK_FALSE(model.setData(kick, ".hidden", Qt::EditRole));
    CHECK_FALSE(model.setData(kick, "Snare", Qt::EditRole));
    CHECK(QFile::exists(folder + "/Kick.scenario"));

    // Renamed: the file follows, and the entry sorts after Snare.
    QPersistentModelIndex moved{kick};
    REQUIRE(model.setData(kick, "Toms", Qt::EditRole));
    CHECK_FALSE(QFile::exists(folder + "/Kick.scenario"));
    CHECK(QFile::exists(folder + "/Toms.scenario"));
    REQUIRE(moved.isValid());
    CHECK(moved.row() == 1);
    CHECK(model.data(moved, Qt::DisplayRole).toString() == "Toms");
    CHECK(model.index(0, 0, drums).data().toString() == "Snare");
    CHECK(model.find(Library::snippetKey(), folder + "/Toms.scenario") == QModelIndex{moved});

    // A process is not renamed.
    CHECK_FALSE(model.flags(drums) & Qt::ItemIsEditable);
  });
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
    // Each box's nodes on their own, side by side: they were in separate
    // node views
    CHECK_FALSE(QRectF(new_autom->position(), new_autom->size())
                    .intersects(QRectF(new_step->position(), new_step->size())));

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

TEST_CASE("A snippet keeps its description, tags and thumbnail", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QByteArray json = R"({"Intervals":[],"States":[],"Cables":[]})";
    Library::SnippetInfo info;
    info.description = "Four on the floor";
    info.tags = {"drums", "techno"};
    info.thumbnail = QImage{32, 18, QImage::Format_ARGB32};
    info.thumbnail.fill(Qt::red);

    const auto saved = Library::withSnippetInfo(json, info);
    const auto back = Library::readSnippetInfo(saved);
    CHECK(back.description == info.description);
    CHECK(back.tags == info.tags);
    REQUIRE(!back.thumbnail.isNull());
    CHECK(back.thumbnail.size() == info.thumbnail.size());
    CHECK(back.thumbnail.pixelColor(5, 5) == QColor(Qt::red));

    // The content is still what pasting reads.
    const auto doc = readJson(saved);
    CHECK(doc.HasMember("Intervals"));
    CHECK(doc.HasMember("Cables"));

    // A file from before has none, and is no less a snippet.
    CHECK(Library::readSnippetInfo(json).description.isEmpty());
    CHECK(Library::readSnippetInfo(json).thumbnail.isNull());

    // Its tags are found by the library's search.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto file = Scenario::writeSnippet(saved, dir.path(), "Kick", nullptr);
    REQUIRE(file);
    const QByteArray path = file->toUtf8();
    const auto entry = Scenario::snippetEntry(std::string_view{path.data(), size_t(path.size())});
    REQUIRE(entry);
    CHECK(entry->node.data.searchText.contains("techno"));
    CHECK(entry->node.data.searchText.contains("Four on the floor"));
  });
}

TEST_CASE("A snippet put on a cable goes in between its two ends", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->context().document);

    // The snippet: a box with a step sequencer, its inlets and outlet free.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString file;
    {
      Scenario::IntervalModel* box{};
      {
        Scenario::Command::Macro m{
            new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
        box = &m.createBox(
            scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(2000), 0.1);
        REQUIRE(m.createProcessInNewSlot(
            *box, Metadata<ConcreteKey_k, Media::Step::Model>::get(), QString{}));
        m.commit();
      }
      box->selection.set(true);
      JSONReader r;
      Scenario::copySelectedScenarioElements(r, scenario);
      box->selection.set(false);
      auto saved = Scenario::writeSnippet(r.toByteArray(), dir.path(), "Step", nullptr);
      REQUIRE(saved);
      file = *saved;
    }

    // An automation drives a step sequencer.
    Scenario::IntervalModel* target{};
    const Process::ProcessModel* autom{};
    const Process::ProcessModel* step{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      target
          = &m.createBox(scenario, TimeVal::fromMsecs(3000), TimeVal::fromMsecs(6000), 0.5);
      autom = m.createProcessInNewSlot(
          *target, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{});
      step = m.createProcessInNewSlot(
          *target, Metadata<ConcreteKey_k, Media::Step::Model>::get(), QString{});
      REQUIRE(autom);
      REQUIRE(step);
      m.createCable(
          document, *autom->outlets().front(), *step->inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.commit();
    }
    REQUIRE(autom->outlets().front()->cables().size() == 1);
    const auto& cable = autom->outlets().front()->cables().front().find(doc->context());
    const auto cables_before = document.cables.size();

    REQUIRE(Scenario::insertSnippetInCable(doc->context(), cable, file));

    REQUIRE(target->processes.size() == 3);
    const Process::ProcessModel* inserted{};
    for(auto& p : target->processes)
      if(&p != autom && &p != step)
        inserted = &p;
    REQUIRE(inserted);

    // automation -> inserted -> step, and nothing from the automation to the
    // step directly any more.
    CHECK(document.cables.size() == cables_before + 1);
    const auto& a_out = *autom->outlets().front();
    REQUIRE(a_out.cables().size() == 1);
    const auto& first = a_out.cables().front().find(doc->context());
    CHECK(Process::parentProcess(&first.sink().find(doc->context())) == inserted);

    const auto& i_out = *inserted->outlets().front();
    REQUIRE(i_out.cables().size() == 1);
    const auto& second = i_out.cables().front().find(doc->context());
    CHECK(&second.sink().find(doc->context()) == step->inlets().front());

    // One undo: the cable as it was.
    doc->commandStack().undo();
    CHECK(target->processes.size() == 2);
    CHECK(document.cables.size() == cables_before);
    REQUIRE(a_out.cables().size() == 1);
    CHECK(
        &a_out.cables().front().find(doc->context()).sink().find(doc->context())
        == step->inlets().front());
  });
}

TEST_CASE(
    "A snippet saved from nodes goes in a box with its cables, or in the timeline "
    "in a new box",
    "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->context().document);

    // An automation driving a step sequencer, in one box.
    const Process::ProcessModel* autom{};
    const Process::ProcessModel* step{};
    Scenario::IntervalModel* target{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& box
          = m.createBox(scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(3000), 0.1);
      target
          = &m.createBox(scenario, TimeVal::fromMsecs(5000), TimeVal::fromMsecs(8000), 0.5);
      autom = m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{});
      step = m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Media::Step::Model>::get(), QString{});
      REQUIRE(autom);
      REQUIRE(step);
      m.createCable(
          document, *autom->outlets().front(), *step->inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.commit();
    }

    // Selected as nodes, and saved.
    doc->context().selectionStack.pushNewSelection(
        Selection{const_cast<Process::ProcessModel*>(autom),
                  const_cast<Process::ProcessModel*>(step)});
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, doc->context()));
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto file = Scenario::writeSnippet(r.toByteArray(), dir.path(), "Nodes", nullptr);
    REQUIRE(file);
    {
      QFile f{*file};
      REQUIRE(f.open(QIODevice::ReadOnly));
      const auto json = readJson(f.readAll());
      CHECK_FALSE(json.HasMember("Intervals"));
      CHECK(json.HasMember("Processes"));
    }

    // In a box: both, and the cable between the copies.
    const auto cables_before = document.cables.size();
    REQUIRE(Scenario::insertSnippetInInterval(
        doc->context(), *target, QPointF{100., 100.}, *file));
    REQUIRE(target->processes.size() == 2);
    CHECK(document.cables.size() == cables_before + 1);
    for(auto& p : target->processes)
      if(qobject_cast<const Automation::ProcessModel*>(&p))
      {
        REQUIRE(p.outlets().front()->cables().size() == 1);
        const auto& c = p.outlets().front()->cables().front().find(doc->context());
        CHECK(Process::parentProcess(&c.sink().find(doc->context()))->parent() == target);
      }

    // In the timeline: a new box holding them.
    const auto boxes_before = scenario.intervals.size();
    QFile f{*file};
    REQUIRE(f.open(QIODevice::ReadOnly));
    auto json = readJson(f.readAll());
    REQUIRE(Scenario::pasteProcessesInNewBox(
        scenario, Scenario::Point{TimeVal::fromMsecs(9000), 0.7}, json, doc->context()));
    REQUIRE(scenario.intervals.size() == boxes_before + 1);
    bool found = false;
    for(auto& itv : scenario.intervals)
      if(itv.date() == TimeVal::fromMsecs(9000))
      {
        found = true;
        CHECK(itv.processes.size() == 2);
      }
    CHECK(found);
  });
}

TEST_CASE("A snippet with several cables goes in with all of them", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->context().document);

    // A chain of three: automation -> step -> step, two cables.
    std::vector<const Process::ProcessModel*> chain;
    Scenario::IntervalModel* target{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& box
          = m.createBox(scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(3000), 0.1);
      target
          = &m.createBox(scenario, TimeVal::fromMsecs(5000), TimeVal::fromMsecs(8000), 0.5);
      chain.push_back(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{}));
      chain.push_back(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Media::Step::Model>::get(), QString{}));
      chain.push_back(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Media::Step::Model>::get(), QString{}));
      for(auto p : chain)
        REQUIRE(p);
      m.createCable(
          document, *chain[0]->outlets().front(), *chain[1]->inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.createCable(
          document, *chain[1]->outlets().front(), *chain[2]->inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.commit();
    }

    doc->context().selectionStack.pushNewSelection(Selection{
        const_cast<Process::ProcessModel*>(chain[0]),
        const_cast<Process::ProcessModel*>(chain[1]),
        const_cast<Process::ProcessModel*>(chain[2])});
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, doc->context()));
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto file = Scenario::writeSnippet(r.toByteArray(), dir.path(), "Chain", nullptr);
    REQUIRE(file);

    const auto cables_before = document.cables.size();
    REQUIRE(Scenario::insertSnippetInInterval(
        doc->context(), *target, QPointF{0., 0.}, *file));
    CHECK(target->processes.size() == 3);
    CHECK(document.cables.size() == cables_before + 2);

    // In the timeline, in a new box: both cables too.
    QFile f{*file};
    REQUIRE(f.open(QIODevice::ReadOnly));
    auto json = readJson(f.readAll());
    const auto cables_mid = document.cables.size();
    REQUIRE(Scenario::pasteProcessesInNewBox(
        scenario, Scenario::Point{TimeVal::fromMsecs(9000), 0.7}, json, doc->context()));
    CHECK(document.cables.size() == cables_mid + 2);
  });
}


TEST_CASE("Only the snippets of the user's library are renamed", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel model{ctx, nullptr};
    model.flushPending();
    TemporaryUserLibrary user;
    // A package's snippet, next to the user's library
    const QString package = QDir::cleanPath(
        QFileInfo{user.snippets}.absolutePath() + "/../a-package/snippets/Kick.scenario");
    writeFile(package, R"({"Intervals":[]})");
    if(auto e = Scenario::snippetEntry(utf8(QFileInfo{package}.absoluteFilePath())))
      model.publish(std::move(*e));
    model.flushPending();

    const auto idx
        = model.find(Library::snippetKey(), QFileInfo{package}.absoluteFilePath());
    REQUIRE(idx.isValid());
    CHECK_FALSE(model.flags(idx) & Qt::ItemIsEditable);
    CHECK_FALSE(model.setData(idx, "Renamed", Qt::EditRole));
    CHECK(QFile::exists(package));
  });
}

TEST_CASE(
    "A snippet's description is written in its file, the rest of it untouched",
    "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    TemporaryUserLibrary user;
    const QString path = user.snippets + "/Kit.scenario";
    Library::SnippetInfo info;
    info.description = "old";
    info.tags = {"kick"};
    info.thumbnail = QImage{4, 4, QImage::Format_ARGB32};
    info.thumbnail.fill(Qt::red);
    writeFile(path, Library::withSnippetInfo(R"({"Intervals":[{"id":1}]})", info));

    REQUIRE(Library::writeSnippetDescription(path, "new words"));
    QFile f{path};
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QByteArray json = f.readAll();
    const auto read = Library::readSnippetInfo(json);
    CHECK(read.description == "new words");
    CHECK(read.tags == QStringList{"kick"});
    CHECK(read.thumbnail.size() == QSize{4, 4});
    CHECK(json.contains(R"("Intervals")"));

    // Without the thumbnail when it is not needed
    CHECK(Library::readSnippetInfo(json, false).thumbnail.isNull());

    // A file that is not a snippet is left alone
    const QString other = user.snippets + "/Broken.scenario";
    writeFile(other, "not json");
    CHECK_FALSE(Library::writeSnippetDescription(other, "x"));
    QFile o{other};
    REQUIRE(o.open(QIODevice::ReadOnly));
    CHECK(o.readAll() == "not json");
    CHECK_FALSE(Library::writeSnippetDescription(user.snippets + "/Missing.scenario", "x"));

    CHECK(Library::isUserLibraryFile(path));
    CHECK_FALSE(Library::isUserLibraryFile(user.dir.path() + "/elsewhere/Kit.scenario"));
  });
}

TEST_CASE("A new description is searched for at once", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Library::ProcessesItemModel model{ctx, nullptr};
    model.flushPending();
    TemporaryUserLibrary user;
    const QString path = user.snippets + "/Kit.scenario";
    writeFile(path, R"({"Intervals":[]})");
    if(auto e = Scenario::snippetEntry(utf8(path)))
      model.publish(std::move(*e));
    model.flushPending();
    const auto idx = model.find(Library::snippetKey(), path);
    REQUIRE(idx.isValid());

    // The search proxies filter again on dataChanged
    int changed = 0;
    QObject::connect(
        &model, &QAbstractItemModel::dataChanged, &model,
        [&](const QModelIndex& a, const QModelIndex&) { changed += (a == idx); });
    model.setSearchText(Library::snippetKey(), path, "zebra");
    CHECK(changed == 1);
    CHECK(model.nodeFromModelIndex(idx).searchText == "zebra");
  });
}

TEST_CASE("A malformed snippet is refused, not half inserted", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);
    Scenario::IntervalModel* target{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      target
          = &m.createBox(scenario, TimeVal::fromMsecs(0), TimeVal::fromMsecs(1000), 0.3);
      m.commit();
    }
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto steps = doc->commandStack().size();
    for(const char* content :
        {R"({"Intervals":[{"id":"one","Processes":[{"id":1,"Pos":["a","b"]}]}]})",
         R"({"Intervals":[{"id":1,"Processes":[{"id":1,"Pos":[1]}]}],"Cables":[{}]})",
         R"({"Intervals":7})", R"([1,2,3])"})
    {
      const QString path = dir.path() + "/Bad.scenario";
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(content);
      f.close();
      CHECK_FALSE(Scenario::insertSnippetInInterval(
          doc->context(), *target, QPointF{}, path));
      CHECK(target->processes.empty());
    }
    CHECK(doc->commandStack().size() == steps);
  });
}

TEST_CASE(
    "A snippet added from a state goes in a new box starting there",
    "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenario = score::test::base_scenario(*doc);

    Scenario::IntervalModel* a{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      a = &m.createBox(scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(3000), 0.1);
      REQUIRE(m.createProcessInNewSlot(
          *a, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{}));
      m.commit();
    }
    a->selection.set(true);
    JSONReader r;
    Scenario::copySelectedScenarioElements(r, scenario);
    a->selection.set(false);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto file = Scenario::writeSnippet(r.toByteArray(), dir.path(), "Swell", nullptr);
    REQUIRE(file);

    const auto& end = scenario.state(a->endState());
    const auto newBoxes = [&](const std::vector<const Scenario::IntervalModel*>& before) {
      std::vector<const Scenario::IntervalModel*> res;
      for(auto& i : scenario.intervals)
        if(std::find(before.begin(), before.end(), &i) == before.end())
          res.push_back(&i);
      return res;
    };
    std::vector<const Scenario::IntervalModel*> before;
    for(auto& i : scenario.intervals)
      before.push_back(&i);

    // From a state nothing starts from: that state starts the box
    REQUIRE(Scenario::insertSnippetAfter(doc->context(), scenario, end, 0.5, *file));
    auto added = newBoxes(before);
    REQUIRE(added.size() == 1);
    const auto& box = *added.front();
    CHECK(box.startState() == end.id());
    CHECK(box.date() == a->date() + a->duration.defaultDuration());
    CHECK(box.metadata().getName() == "Swell");
    REQUIRE(box.processes.size() == 1);
    CHECK(qobject_cast<const Automation::ProcessModel*>(&*box.processes.begin()));

    // From it again: a new state on the same event starts another box
    for(auto& i : scenario.intervals)
      before.push_back(&i);
    REQUIRE(Scenario::insertSnippetAfter(doc->context(), scenario, end, 0.7, *file));
    added = newBoxes(before);
    REQUIRE(added.size() == 1);
    const auto& second = scenario.state(added.front()->startState());
    CHECK(second.id() != end.id());
    CHECK(second.eventId() == end.eventId());

    // One undo step each
    doc->commandStack().undo();
    CHECK(newBoxes(before).empty());
    CHECK(end.nextInterval());
  });
}
