// The library's presets and their files: read once per scan, renamed and
// deleted in the user library only, where they are.

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>

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
    QAbstractItemModel& model = presets;

    int changed = 0;
    QObject::connect(
        &model, &QAbstractItemModel::dataChanged, [&](auto&&...) { changed++; });

    // An installed package's preset: not editable, its file untouched
    {
      const auto idx = model.index(lib.row("Packaged"), 0, {});
      CHECK(!(model.flags(idx) & Qt::ItemIsEditable));
      CHECK(!model.setData(idx, "Renamed", Qt::EditRole));
      CHECK(QFile::exists(lib.package));
      CHECK(lib.count("Packaged") == 1);
    }

    // The user's: renamed beside where it was, the old file gone
    {
      const auto idx = model.index(lib.row("Mine"), 0, {});
      CHECK(model.flags(idx) & Qt::ItemIsEditable);
      REQUIRE(model.setData(idx, "Yours", Qt::EditRole));
      const QString renamed = lib.root + "/packages/user/presets/Pattern/Yours.scp";
      CHECK(QFile::exists(renamed));
      CHECK(!QFile::exists(lib.user));
      REQUIRE(lib.row("Yours") >= 0);
      CHECK(lib.presets()[lib.row("Yours")].path == renamed);
      CHECK(changed == 1);

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
      const auto idx = model.index(lib.row("Yours"), 0, {});
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

    int removed = 0;
    QObject::connect(
        &model, &QAbstractItemModel::rowsRemoved, [&](auto&&...) { removed++; });

    CHECK(!model.removePreset(lib.row("Packaged")));
    CHECK(QFile::exists(lib.package));
    CHECK(lib.count("Packaged") == 1);

    REQUIRE(model.removePreset(lib.row("Mine")));
    CHECK(!QFile::exists(lib.user));
    CHECK(lib.count("Mine") == 0);
    CHECK(removed == 1);

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
