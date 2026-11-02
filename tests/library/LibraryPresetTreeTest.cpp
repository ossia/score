// The library's presets of a process, in folders by category: the model, and
// the view keeping what is open and the current preset across changes.

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>

#include <Library/PresetItemModel.hpp>
#include <Library/PresetTreeView.hpp>

#include <QCoreApplication>

#include <score/application/GUIApplicationContext.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
// Pattern sequencer, and an automation: two kinds of process
const auto pattern = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("49047204-5c1e-4b54-9f43-2b583f664b2e"));
const auto automation = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"));

Process::Preset preset(
    const QString& name, const QString& category,
    UuidKey<Process::ProcessModel> k = pattern)
{
  Process::Preset p;
  p.name = name;
  p.category = category;
  p.key = {k, {}};
  p.data = "[]";
  return p;
}

QStringList rows(const QAbstractItemModel& m, const QModelIndex& parent = {})
{
  QStringList out;
  for(int i = 0; i < m.rowCount(parent); i++)
    out.push_back(m.index(i, 0, parent).data().toString());
  return out;
}

struct Presets
{
  Process::ApplicationPlugin& plug;
  explicit Presets(const score::GUIApplicationContext& ctx)
      : plug{ctx.applicationPlugin<Process::ApplicationPlugin>()}
  {
    plug.clearPresets();
    for(auto p :
        {preset("root", ""), preset("a2", "A"), preset("a1", "A"), preset("b", "A/B"),
         preset("z", "Z"), preset("other", "A", automation)})
      plug.addPreset(std::move(p));
  }
  ~Presets() { plug.clearPresets(); }
  int row(const QString& name)
  {
    for(int i = 0; i < int(plug.presets.size()); i++)
      if(plug.presets[i].name == name)
        return i;
    return -1;
  }
};
}

TEST_CASE("the library shows a process's presets in folders", "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Presets lib{ctx};
    Library::PresetItemModel model{ctx, nullptr};
    QAbstractItemModel& m = model;

    // No process: nothing
    CHECK(m.rowCount({}) == 0);

    model.setProcess(Process::ProcessIdentifier{pattern, {}});
    CHECK(rows(m) == (QStringList{"root", "A", "Z"}));
    const auto a = m.index(1, 0, {});
    CHECK(model.preset(a) == nullptr);
    CHECK(rows(m, a) == (QStringList{"a1", "a2", "B"}));
    CHECK(rows(m, m.index(2, 0, a)) == QStringList{"b"});
    CHECK(m.parent(m.index(0, 0, a)) == a);

    // Each row refers to its preset
    const auto a1 = m.index(0, 0, a);
    REQUIRE(model.preset(a1));
    CHECK(model.preset(a1)->name == "a1");
    CHECK(model.folderPath(a1) == QStringList{"A"});
    CHECK(model.folderPath(m.index(2, 0, a)) == (QStringList{"A", "B"}));
    CHECK(model.indexOfFolder({"A", "B"}) == m.index(2, 0, a));
    CHECK(model.indexOfPreset(lib.row("b")) == m.index(0, 0, m.index(2, 0, a)));

    // Folders are not dragged nor renamed
    CHECK(!(m.flags(a) & Qt::ItemIsDragEnabled));
    CHECK(m.flags(a1) & Qt::ItemIsDragEnabled);
  });
}

TEST_CASE(
    "presets of other processes do not rebuild the tree, and rows follow theirs",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Presets lib{ctx};
    Library::PresetItemModel model{ctx, nullptr};
    QAbstractItemModel& m = model;
    model.setProcess(Process::ProcessIdentifier{pattern, {}});

    int resets = 0;
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { resets++; });

    // Some sort before this process's presets, some after: the rows shift
    for(int i = 0; i < 5; i++)
      lib.plug.addPreset(preset(QString("o%1").arg(i), "", automation));
    CHECK(resets == 0);
    const auto a = m.index(1, 0, {});
    CHECK(model.preset(m.index(0, 0, a))->name == "a1");
    CHECK(model.preset(m.index(0, 0, {}))->name == "root");

    lib.plug.removePreset(lib.row("o2"));
    CHECK(resets == 0);
    CHECK(model.preset(m.index(1, 0, a))->name == "a2");

    // One of its own: rebuilt
    lib.plug.addPreset(preset("a0", "A"));
    CHECK(resets == 1);
    CHECK(rows(m, m.index(1, 0, {})) == (QStringList{"a0", "a1", "a2", "B"}));
    lib.plug.removePreset(lib.row("a1"));
    CHECK(resets == 2);
    CHECK(rows(m, m.index(1, 0, {})) == (QStringList{"a0", "a2", "B"}));
  });
}

TEST_CASE("a search keeps the presets it matches and their folders", "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Presets lib{ctx};
    Library::PresetItemModel model{ctx, nullptr};
    QAbstractItemModel& m = model;
    model.setProcess(Process::ProcessIdentifier{pattern, {}});

    model.setFilterText("B");
    // "b" by name, in A/B; nothing in Z or the root matches
    CHECK(rows(m) == QStringList{"A"});
    CHECK(rows(m, m.index(0, 0, {})) == QStringList{"B"});

    // A category matches too: every preset in it
    model.setFilterText("z");
    CHECK(rows(m) == QStringList{"Z"});
    CHECK(rows(m, m.index(0, 0, {})) == QStringList{"z"});

    model.setFilterText({});
    CHECK(rows(m) == (QStringList{"root", "A", "Z"}));
  });
}

TEST_CASE(
    "the view keeps open folders and the current preset when the tree changes",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Presets lib{ctx};
    Library::PresetItemModel model{ctx, nullptr};
    Library::PresetTreeView view;
    view.setPresetModel(&model);
    model.setProcess(Process::ProcessIdentifier{pattern, {}});

    int selected = 0;
    QObject::connect(&view, &Library::PresetTreeView::selected, [&](auto&&...) {
      selected++;
    });

    const auto a = model.indexOfFolder({"A"});
    view.expand(a);
    view.setCurrentIndex(model.indexOfPreset(lib.row("a2")));
    CHECK(selected == 1);

    // A preset added to this process rebuilds it
    lib.plug.addPreset(preset("a0", "A"));
    CHECK(view.isExpanded(model.indexOfFolder({"A"})));
    CHECK(!view.isExpanded(model.indexOfFolder({"A", "B"})));
    REQUIRE(model.preset(view.currentIndex()));
    CHECK(model.preset(view.currentIndex())->name == "a2");
    // Which is not the user choosing a preset
    CHECK(selected == 1);

    // Another process, then back: its folders as they were
    model.setProcess(Process::ProcessIdentifier{automation, {}});
    CHECK(rows(model) == QStringList{"A"});
    CHECK(!view.isExpanded(model.indexOfFolder({"A"})));
    model.setProcess(Process::ProcessIdentifier{pattern, {}});
    CHECK(view.isExpanded(model.indexOfFolder({"A"})));

    // A search opens what it finds, and is not remembered
    model.setFilterText("b");
    CHECK(view.isExpanded(model.indexOfFolder({"A", "B"})));
    model.setFilterText({});
    CHECK(!view.isExpanded(model.indexOfFolder({"A", "B"})));
    CHECK(view.isExpanded(model.indexOfFolder({"A"})));
  });
}

TEST_CASE(
    "presets added together rebuild the tree once more, at the end",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Presets lib{ctx};
    Library::PresetItemModel model{ctx, nullptr};
    model.setProcess(Process::ProcessIdentifier{pattern, {}});
    int resets = 0;
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { resets++; });

    // As a scan delivers them: in one turn of the event loop
    lib.plug.addPreset(preset("n1", "N"));
    CHECK(resets == 1);
    lib.plug.addPreset(preset("n2", "N"));
    lib.plug.addPreset(preset("n3", "N"));
    CHECK(resets == 1);
    // What is shown is consistent meanwhile: rows refer to their presets
    CHECK(rows(model, model.indexOfFolder({"N"})) == QStringList{"n1"});
    CHECK(model.preset(model.index(0, 0, model.indexOfFolder({"N"})))->name == "n1");

    QCoreApplication::processEvents();
    CHECK(resets == 2);
    CHECK(rows(model, model.indexOfFolder({"N"})) == (QStringList{"n1", "n2", "n3"}));

    // And a lone one later is shown at once again
    QCoreApplication::processEvents();
    lib.plug.addPreset(preset("n4", "N"));
    CHECK(resets == 3);
  });
}
