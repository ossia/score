// The library's presets and their files: read once per scan, renamed and
// deleted in the user library only, where they are.

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/PresetTreeView.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QDir>
#include <QElapsedTimer>
#include <QAction>
#include <QFile>
#include <QMenu>
#include <QMimeData>

#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
// Pattern sequencer: a process the fx plugin provides
const QString pattern_uuid = QStringLiteral("49047204-5c1e-4b54-9f43-2b583f664b2e");

void writePreset(const QString& file, const QString& name)
{
  QDir{}.mkpath(QFileInfo{file}.absolutePath());
  QFile f{file};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(QStringLiteral(R"({"Key":{"Uuid":"%1","Effect":""},"Name":"%2","Preset":[]})")
              .arg(pattern_uuid, name)
              .toUtf8());
}

//! A library on disk: a preset in an installed package, one in the user's
//! own presets
struct TestLibrary
{
  const score::GUIApplicationContext& ctx;
  QString root = QDir::tempPath() + "/score-tests/library-preset-files";
  QString package = root + "/packages/somepack/presets/Packaged.scp";
  QString user = root + "/packages/user/presets/Pattern/Mine.scp";
  QString oldRoot;

  explicit TestLibrary(const score::GUIApplicationContext& c)
      : ctx{c}
  {
    QDir{root}.removeRecursively();
    writePreset(package, "Packaged");
    writePreset(user, "Mine");
    auto& set = ctx.settings<Library::Settings::Model>();
    oldRoot = set.getRootPath();
    set.setRootPath(root);
  }
  ~TestLibrary()
  {
    ctx.settings<Library::Settings::Model>().setRootPath(oldRoot);
    QDir{root}.removeRecursively();
  }

  std::vector<Process::Preset>& presets()
  {
    return ctx.applicationPlugin<Process::ApplicationPlugin>().presets;
  }
  int count(const QString& name)
  {
    return int(std::count_if(presets().begin(), presets().end(), [&](const auto& p) {
      return p.name == name;
    }));
  }
  int row(const QString& name)
  {
    for(int i = 0; i < int(presets().size()); i++)
      if(presets()[i].name == name)
        return i;
    return -1;
  }
  //! The scan runs on a worker; its results arrive through the event loop
  void waitFor(const QStringList& names)
  {
    QElapsedTimer timeout;
    timeout.start();
    auto all = [&] {
      return std::all_of(
          names.begin(), names.end(), [&](const auto& n) { return count(n) > 0; });
    };
    while(!all() && timeout.elapsed() < 30000)
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    REQUIRE(all());
    // And whatever else this scan would still deliver
    timeout.restart();
    while(timeout.elapsed() < 300)
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
};
}

TEST_CASE("a rescan of the library reads each preset once", "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel model{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    CHECK(lib.count("Packaged") == 1);
    CHECK(lib.count("Mine") == 1);
    // Each knows its file
    CHECK(lib.presets()[lib.row("Mine")].path == lib.user);
    CHECK(lib.presets()[lib.row("Packaged")].path == lib.package);

    for(int i = 0; i < 2; i++)
    {
      ctx.settings<Library::Settings::Model>().rescanLibrary();
      lib.waitFor({"Packaged", "Mine"});
    }
    CHECK(lib.count("Packaged") == 1);
    CHECK(lib.count("Mine") == 1);
  });
}

TEST_CASE(
    "a preset is renamed in its own folder, only in the user library",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel processes{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    Library::PresetItemModel presets{ctx, nullptr};
    presets.setProcess(Process::ProcessIdentifier{
        UuidKey<Process::ProcessModel>::fromString(pattern_uuid), {}});
    QAbstractItemModel& model = presets;

    // Where the renamed preset is now, for the view to keep it current
    QModelIndex renamedTo;
    QObject::connect(&presets, &Library::PresetItemModel::renamed, [&](const QModelIndex& i) {
      renamedTo = i;
    });

    // An installed package's preset: not editable, its file untouched
    {
      const auto idx = presets.indexOfPreset(lib.row("Packaged"));
      REQUIRE(idx.isValid());
      CHECK(!(model.flags(idx) & Qt::ItemIsEditable));
      CHECK(!model.setData(idx, "Renamed", Qt::EditRole));
      CHECK(QFile::exists(lib.package));
      CHECK(lib.count("Packaged") == 1);
    }

    // The user's: renamed beside where it was, the old file gone
    {
      const auto idx = presets.indexOfPreset(lib.row("Mine"));
      REQUIRE(idx.isValid());
      CHECK(model.flags(idx) & Qt::ItemIsEditable);
      REQUIRE(model.setData(idx, "Yours", Qt::EditRole));
      const QString renamed = lib.root + "/packages/user/presets/Pattern/Yours.scp";
      CHECK(QFile::exists(renamed));
      CHECK(!QFile::exists(lib.user));
      REQUIRE(lib.row("Yours") >= 0);
      CHECK(lib.presets()[lib.row("Yours")].path == renamed);
      REQUIRE(presets.preset(renamedTo));
      CHECK(presets.preset(renamedTo)->name == "Yours");

      // The file says its new name too: read again, it is the same preset
      QFile f{renamed};
      REQUIRE(f.open(QIODevice::ReadOnly));
      auto p = Process::Preset::fromJson(
          ctx.interfaces<Process::ProcessFactoryList>(), f.readAll());
      REQUIRE(p);
      CHECK(p->name == "Yours");
    }

    // Onto a name a file already has: the new one gets a number
    {
      writePreset(lib.root + "/packages/user/presets/Pattern/Taken.scp", "Taken");
      const auto idx = presets.indexOfPreset(lib.row("Yours"));
      REQUIRE(idx.isValid());
      REQUIRE(model.setData(idx, "Taken", Qt::EditRole));
      CHECK(QFile::exists(lib.root + "/packages/user/presets/Pattern/Taken.scp"));
      REQUIRE(lib.row("Taken (1)") >= 0);
      CHECK(QFile::exists(lib.presets()[lib.row("Taken (1)")].path));
    }
  });
}

TEST_CASE(
    "a preset is deleted to the trash, only in the user library",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel processes{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    Library::PresetItemModel model{ctx, nullptr};
    model.setProcess(Process::ProcessIdentifier{
        UuidKey<Process::ProcessModel>::fromString(pattern_uuid), {}});
    QAbstractItemModel& m = model;
    CHECK(m.rowCount({}) == 2);

    CHECK(!model.removePreset(lib.row("Packaged")));
    CHECK(QFile::exists(lib.package));
    CHECK(lib.count("Packaged") == 1);

    REQUIRE(model.removePreset(lib.row("Mine")));
    CHECK(!QFile::exists(lib.user));
    CHECK(lib.count("Mine") == 0);
    CHECK(m.rowCount({}) == 1);

    // Gone for good from the library: a rescan does not bring it back
    ctx.settings<Library::Settings::Model>().rescanLibrary();
    lib.waitFor({"Packaged"});
    CHECK(lib.count("Mine") == 0);
  });
}

TEST_CASE("a saved preset knows its file", "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel processes{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    Library::PresetItemModel model{ctx, nullptr};

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, pattern_uuid, {});
    REQUIRE(proc);

    const auto before = lib.presets().size();
    REQUIRE(model.savePreset(*proc));
    REQUIRE(lib.presets().size() == before + 1);

    // The new one is the preset whose file is not one of the two above
    int row = -1;
    for(int i = 0; i < int(lib.presets().size()); i++)
      if(const auto& p = lib.presets()[i].path;
         !p.isEmpty() && p != lib.package && p != lib.user)
        row = i;
    REQUIRE(row >= 0);
    const auto& saved = lib.presets()[row];
    CHECK(QFile::exists(saved.path));
    CHECK(saved.path.startsWith(lib.root + "/packages/user/presets/"));
    CHECK(model.canEdit(row));
  });
}

TEST_CASE(
    "a preset is moved to another category, by its menu or by dragging it",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel processes{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    Library::PresetItemModel presets{ctx, nullptr};
    presets.setProcess(Process::ProcessIdentifier{
        UuidKey<Process::ProcessModel>::fromString(pattern_uuid), {}});
    QAbstractItemModel& model = presets;

    // An installed package's preset stays where it is
    CHECK(!presets.movePreset(lib.row("Packaged"), "Elsewhere"));

    // A new category is shown empty, among the folders, until something
    // goes in it
    const auto drums = presets.addCategory({"Drums"});
    REQUIRE(drums.isValid());
    CHECK(model.rowCount(drums) == 0);
    CHECK(presets.categories().contains("Drums"));
    const auto sub = presets.addCategory({"Drums", "Kicks"});
    REQUIRE(sub.isValid());
    CHECK(presets.folderPath(sub) == (QStringList{"Drums", "Kicks"}));
    CHECK(!presets.addCategory({}).isValid());

    // Moved by its menu: the file says so, and a rescan keeps it there
    REQUIRE(presets.movePreset(lib.row("Mine"), "Drums/Kicks"));
    CHECK(presets.folderPath(presets.indexOfPreset(lib.row("Mine")))
          == (QStringList{"Drums", "Kicks"}));
    {
      QFile f{lib.user};
      REQUIRE(f.open(QIODevice::ReadOnly));
      auto p = Process::Preset::fromJson(
          ctx.interfaces<Process::ProcessFactoryList>(), f.readAll());
      REQUIRE(p);
      CHECK(p->category == "Drums/Kicks");
      CHECK(p->name == "Mine");
    }
    ctx.settings<Library::Settings::Model>().rescanLibrary();
    lib.waitFor({"Packaged", "Mine"});
    CHECK(lib.count("Mine") == 1);
    CHECK(lib.presets()[lib.row("Mine")].category == "Drums/Kicks");

    // Dragged onto another folder: moved there
    {
      const auto from = presets.indexOfPreset(lib.row("Mine"));
      std::unique_ptr<QMimeData> mime{model.mimeData({from})};
      REQUIRE(mime);
      const auto to = presets.indexOfFolder({"Drums"});
      REQUIRE(to.isValid());
      CHECK(model.canDropMimeData(mime.get(), Qt::MoveAction, -1, 0, to));
      REQUIRE(model.dropMimeData(mime.get(), Qt::MoveAction, -1, 0, to));
      CHECK(lib.presets()[lib.row("Mine")].category == "Drums");
    }

    // And onto the top level: no category
    {
      std::unique_ptr<QMimeData> mime{
          model.mimeData({presets.indexOfPreset(lib.row("Mine"))})};
      REQUIRE(model.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, {}));
      CHECK(lib.presets()[lib.row("Mine")].category.isEmpty());
    }

    // A package's preset dragged is not moved
    {
      std::unique_ptr<QMimeData> mime{
          model.mimeData({presets.indexOfPreset(lib.row("Packaged"))})};
      REQUIRE(mime);
      CHECK(!model.canDropMimeData(
          mime.get(), Qt::MoveAction, -1, 0, presets.indexOfFolder({"Drums"})));
    }
  });
}

TEST_CASE(
    "a preset whose file cannot be written stays as it was",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel processes{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    Library::PresetItemModel presets{ctx, nullptr};
    presets.setProcess(Process::ProcessIdentifier{
        UuidKey<Process::ProcessModel>::fromString(pattern_uuid), {}});
    QAbstractItemModel& model = presets;

    // A name is not a path
    {
      const auto idx = presets.indexOfPreset(lib.row("Mine"));
      CHECK(!model.setData(idx, "../Out", Qt::EditRole));
      CHECK(!model.setData(idx, "a\\b", Qt::EditRole));
      CHECK(QFile::exists(lib.user));
    }

    QFile before{lib.user};
    REQUIRE(before.open(QIODevice::ReadOnly));
    const QByteArray contents = before.readAll();
    before.close();

    // Its folder read-only: nothing can be written in it
    const QString folder = QFileInfo{lib.user}.absolutePath();
    const auto perms = QFile::permissions(folder);
    REQUIRE(QFile::setPermissions(
        folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    if(QFile f{folder + "/probe"}; f.open(QIODevice::WriteOnly))
    {
      f.remove();
      QFile::setPermissions(folder, perms);
      SKIP("the folder stays writable (running as root?)");
    }

    CHECK(!presets.movePreset(lib.row("Mine"), "Elsewhere"));
    CHECK(lib.presets()[lib.row("Mine")].category.isEmpty());

    const auto idx = presets.indexOfPreset(lib.row("Mine"));
    CHECK(!model.setData(idx, "Renamed", Qt::EditRole));
    CHECK(lib.row("Renamed") < 0);

    REQUIRE(QFile::setPermissions(folder, perms));

    // The file is whole, where it was
    QFile after{lib.user};
    REQUIRE(after.open(QIODevice::ReadOnly));
    CHECK(after.readAll() == contents);
  });
}

TEST_CASE(
    "a preset's menu acts on that preset when the library changed meanwhile",
    "[library][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    TestLibrary lib{ctx};
    Library::ProcessesItemModel processes{ctx, nullptr};
    lib.waitFor({"Packaged", "Mine"});
    Library::PresetItemModel presets{ctx, nullptr};
    presets.setProcess(Process::ProcessIdentifier{
        UuidKey<Process::ProcessModel>::fromString(pattern_uuid), {}});
    Library::PresetTreeView view;
    view.setPresetModel(&presets);
    REQUIRE(presets.addCategory({"Drums"}).isValid());

    auto menu = view.contextMenu(presets.indexOfPreset(lib.row("Mine")));
    REQUIRE(menu);
    const auto action = [&](QMenu& m, const QString& text) -> QAction* {
      for(auto* a : m.actions())
        if(a->text() == text)
          return a;
      return nullptr;
    };

    // While it is open, a scan brings a preset that sorts before it
    const int mine = lib.row("Mine");
    auto& plug = ctx.applicationPlugin<Process::ApplicationPlugin>();
    Process::Preset earlier = lib.presets()[mine];
    earlier.name = "AAA";
    earlier.path.clear();
    plug.addPreset(std::move(earlier));
    REQUIRE(lib.row("Mine") != mine);

    auto* move = action(*menu, "Move to");
    REQUIRE(move);
    REQUIRE(move->menu());
    auto* drums = action(*move->menu(), "Drums");
    REQUIRE(drums);
    drums->trigger();
    CHECK(lib.presets()[lib.row("Mine")].category == "Drums");
    CHECK(lib.presets()[lib.row("AAA")].category.isEmpty());

    // Gone from the library while the menu was open: nothing is done
    auto again = view.contextMenu(presets.indexOfPreset(lib.row("Mine")));
    REQUIRE(again);
    plug.removePreset(lib.row("Mine"));
    auto* remove = action(*again, "Delete");
    REQUIRE(remove);
    remove->trigger();
    CHECK(QFile::exists(lib.user));
    CHECK(lib.row("AAA") >= 0);
  });
}
