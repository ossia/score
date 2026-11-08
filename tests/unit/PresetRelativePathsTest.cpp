// A preset saved from the presets panel next to the sound it uses stores it
// as <PRESET>:kick.wav, so that the folder can be moved, copied or shipped in
// a package with the sound still found: the library reads it back as the file
// next to wherever the preset now is.

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/ProjectFiles.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Project.hpp>

namespace
{
// Looper (audio), whose "Loop file" is a sound file port.
const auto looper_uuid = QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
// Library::PresetLibraryHandler, which reads the library's .scp files.
const auto preset_handler_uuid = QStringLiteral("7fc3a366-7792-489f-aca9-79d9f6d4415d");

Library::LibraryInterface* preset_handler(const score::GUIApplicationContext& ctx)
{
  const auto key = UuidKey<Library::LibraryInterface>::fromString(preset_handler_uuid);
  for(auto& lib : ctx.interfaces<Library::LibraryInterfaceList>())
    if(lib.concreteKey() == key)
      return &lib;
  return nullptr;
}

const Process::Preset*
preset_in(const score::GUIApplicationContext& ctx, const QString& file)
{
  for(const auto& p : ctx.applicationPlugin<Process::ApplicationPlugin>().presets)
    if(p.path == file)
      return &p;
  return nullptr;
}

QByteArray contents(const QString& file)
{
  QFile f{file};
  REQUIRE(f.open(QIODevice::ReadOnly));
  return f.readAll();
}

//! The library root of the settings for the duration of a test.
struct library_root
{
  Library::Settings::Model& settings;
  QString previous;
  library_root(const score::GUIApplicationContext& ctx, const QString& root)
      : settings{ctx.settings<Library::Settings::Model>()}
      , previous{settings.getRootPath()}
  {
    settings.setRootPath(root);
    QDir{}.mkpath(settings.getUserPresetsPath());
  }
  ~library_root() { settings.setRootPath(previous); }
};
}

TEST_CASE(
    "a preset saved next to its sound finds it wherever its folder goes",
    "[presets][library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = procs.get(UuidKey<Process::ProcessModel>::fromString(looper_uuid));
    if(!factory)
      SKIP("Fx not built");
    auto* handler = preset_handler(ctx);
    REQUIRE(handler);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString root = score::test::canonical(tmp.path());
    library_root lib{ctx, root + "/library"};

    REQUIRE(QDir{}.mkpath(root + "/project"));
    auto* doc = score::test::project_document(ctx, root + "/project");
    auto* proc = score::test::add_process(*doc, looper_uuid, {});
    REQUIRE(proc);
    auto* file = score::test::file_control(*proc);
    REQUIRE(file);

    // Where the panel saves the presets of this process.
    QString folder = lib.settings.getUserPresetsPath() + "/" + factory->prettyName();
    if(const auto desc = factory->descriptor(QString{});
       desc.prettyName != factory->prettyName())
      folder += "/" + desc.prettyName;
    const QString kick = folder + "/kick.wav";
    score::test::write_wav(kick, 0.05);

    // As the file picker stores it: the sound is in the library.
    const QString stored = score::relativizeFilePath(kick, doc->context());
    REQUIRE(score::isLibraryRelativePath(stored));
    file->setValue(stored.toStdString());

    Library::PresetItemModel model{ctx, nullptr};
    REQUIRE(model.savePreset(*proc));
    const auto& presets = ctx.applicationPlugin<Process::ApplicationPlugin>().presets;
    const auto saved = std::find_if(presets.begin(), presets.end(), [&](auto& p) {
      return p.key.key == proc->concreteKey() && !p.path.isEmpty();
    });
    REQUIRE(saved != presets.end());
    const QString savedFile = saved->path;
    REQUIRE(QFileInfo{savedFile}.absolutePath() == folder);

    const QByteArray written = contents(savedFile);
    INFO(written.toStdString());
    CHECK(written.contains("\"<PRESET>:kick.wav\""));
    CHECK_FALSE(written.contains("<LIBRARY>:"));
    CHECK_FALSE(written.contains(root.toUtf8()));

    // The whole folder moved out of the library.
    const QString moved = root + "/elsewhere/Kit";
    REQUIRE(QDir{}.mkpath(root + "/elsewhere"));
    REQUIRE(QDir{}.rename(folder, moved));
    const QString movedFile = moved + "/" + QFileInfo{savedFile}.fileName();
    const QString movedKick = moved + "/kick.wav";
    REQUIRE(QFileInfo::exists(movedFile));

    Library::ProcessesItemModel processes{ctx, nullptr};
    handler->setup(processes, ctx);
    handler->addPath(movedFile.toStdString());
    const auto* read = preset_in(ctx, movedFile);
    REQUIRE(read);
    INFO(read->data.toStdString());
    CHECK(read->data.contains(("\"" + movedKick + "\"").toUtf8()));
    CHECK_FALSE(read->data.contains("<PRESET>:"));

    // Applied to a process: its port names the moved sound.
    file->setValue(std::string{});
    proc->loadPreset(*read);
    CHECK(
        score::locateFilePath(score::test::control_string(*file), doc->context())
            .toStdString()
        == movedKick.toStdString());

    // And copied back into the library: a library path again.
    const QString copied = lib.settings.getUserLibraryPath() + "/kits/Kit";
    REQUIRE(QDir{}.mkpath(copied));
    for(const auto& name : QDir{moved}.entryList(QDir::Files))
      REQUIRE(QFile::copy(moved + "/" + name, copied + "/" + name));
    const QString copiedFile = copied + "/" + QFileInfo{savedFile}.fileName();
    handler->addPath(copiedFile.toStdString());
    const auto* fromPackage = preset_in(ctx, copiedFile);
    REQUIRE(fromPackage);
    file->setValue(std::string{});
    proc->loadPreset(*fromPackage);
    const QString value = score::test::control_string(*file);
    CHECK(value.toStdString() == "<LIBRARY>:packages/user/kits/Kit/kick.wav");
    CHECK(
        score::locateFilePath(value, doc->context()).toStdString()
        == QString{copied + "/kick.wav"}.toStdString());

    // Moved to another category: the file written again keeps <PRESET>:.
    const auto row = model.presetRowOf(copiedFile);
    REQUIRE(row >= 0);
    REQUIRE(model.movePreset(row, QStringLiteral("Drums")));
    CHECK(contents(copiedFile).contains("\"<PRESET>:kick.wav\""));
  });
}

TEST_CASE("a preset with no <PRESET>: path reads as it was written", "[presets][library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    if(!procs.get(UuidKey<Process::ProcessModel>::fromString(looper_uuid)))
      SKIP("Fx not built");
    auto* handler = preset_handler(ctx);
    REQUIRE(handler);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString root = score::test::canonical(tmp.path());
    score::test::write_wav(root + "/kick.wav", 0.05);

    Process::Preset p;
    p.name = "old";
    p.key.key = UuidKey<Process::ProcessModel>::fromString(looper_uuid);
    p.data = QStringLiteral(R"([[0,"%1/kick.wav","Loop file"],[1,0.5,"Gain"]])")
                 .arg(root)
                 .toUtf8();
    const QString file = root + "/old.scp";
    score::test::write_file(file, p.toJson());

    Library::ProcessesItemModel processes{ctx, nullptr};
    handler->setup(processes, ctx);
    handler->addPath(file.toStdString());
    const auto* read = preset_in(ctx, file);
    REQUIRE(read);
    CHECK(read->data == p.data);
  });
}
