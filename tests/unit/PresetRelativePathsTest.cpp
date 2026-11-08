// A preset saved from the presets panel next to the sound it uses stores it
// as <PRESET>:kick.wav, so that the folder can be moved, copied or shipped in
// a package with the sound still found: the library reads it back as the file
// next to wherever the preset now is.

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/PresetItemModel.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <Process/ApplicationPlugin.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/ProjectFiles.hpp>

#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>

#include <Avnd/Factories.hpp>
#include <Crousti/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
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

//! An object that plays the sounds of a folder: its own folder, and one
//! inside it.
struct FolderKit
{
  halp_meta(name, "Folder kit test")
  halp_meta(c_name, "avnd_folder_kit_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "e1c5b0d2-3f4a-4b6e-9a8d-2c7f1e0b5a91")

  struct
  {
    halp::folder_port<"Kit"> kit;
    halp::folder_port<"Here"> here;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

Process::ControlInlet* control_named(Process::ProcessModel& proc, const QString& name)
{
  for(auto* in : proc.inlets())
    if(auto* ctrl = qobject_cast<Process::ControlInlet*>(in); ctrl && ctrl->name() == name)
      return ctrl;
  return nullptr;
}

Process::Preset looper_preset(const QString& name, const QString& sound)
{
  Process::Preset p;
  p.name = name;
  p.key.key = UuidKey<Process::ProcessModel>::fromString(looper_uuid);
  p.data = QStringLiteral(R"([[20,{"String":"%1"},"Loop file"]])").arg(sound).toUtf8();
  return p;
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

TEST_CASE(
    "a preset saved next to its folders finds them wherever the tree goes",
    "[presets][library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const_cast<Process::ProcessFactoryList&>(ctx.interfaces<Process::ProcessFactoryList>())
        .insert(std::make_unique<oscr::ProcessFactory<FolderKit>>());
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = procs.get(UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("e1c5b0d2-3f4a-4b6e-9a8d-2c7f1e0b5a91")));
    REQUIRE(factory);
    auto* handler = preset_handler(ctx);
    REQUIRE(handler);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString root = score::test::canonical(tmp.path());
    library_root lib{ctx, root + "/library"};

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& interval = score::test::base_interval(*doc);
    auto* proc = new oscr::ProcessModel<FolderKit>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{4321}, doc->context(),
        &interval};
    interval.processes.add(proc);
    auto* kit = control_named(*proc, "Kit");
    auto* here = control_named(*proc, "Here");
    REQUIRE(kit);
    REQUIRE(here);

    QString folder = lib.settings.getUserPresetsPath() + "/" + factory->prettyName();
    if(const auto desc = factory->descriptor(QString{});
       desc.prettyName != factory->prettyName())
      folder += "/" + desc.prettyName;
    score::test::write_wav(folder + "/kit/kick.wav", 0.05);
    score::test::write_wav(folder + "/kit/snare.wav", 0.05);

    kit->setValue(score::relativizeFilePath(folder + "/kit", doc->context()).toStdString());
    here->setValue(folder.toStdString());

    Library::PresetItemModel model{ctx, nullptr};
    REQUIRE(model.savePreset(*proc));
    const auto& presets = ctx.applicationPlugin<Process::ApplicationPlugin>().presets;
    const auto saved = std::find_if(presets.begin(), presets.end(), [&](auto& p) {
      return p.key.key == factory->concreteKey() && !p.path.isEmpty();
    });
    REQUIRE(saved != presets.end());
    const QString name = QFileInfo{saved->path}.fileName();

    const QByteArray written = contents(saved->path);
    INFO(written.toStdString());
    CHECK(written.contains("\"<PRESET>:kit\""));
    CHECK(written.contains("\"<PRESET>:.\""));
    CHECK_FALSE(written.contains(root.toUtf8()));

    // The preset, its folder of sounds and all moved out of the library.
    const QString moved = root + "/elsewhere/Kits";
    REQUIRE(QDir{}.mkpath(root + "/elsewhere"));
    REQUIRE(QDir{}.rename(folder, moved));

    Library::ProcessesItemModel processes{ctx, nullptr};
    handler->setup(processes, ctx);
    handler->addPath(QString{moved + "/" + name}.toStdString());
    const auto* read = preset_in(ctx, moved + "/" + name);
    REQUIRE(read);

    kit->setValue(std::string{});
    here->setValue(std::string{});
    proc->loadPreset(*read);
    CHECK(
        score::locateFilePath(score::test::control_string(*kit), doc->context())
            .toStdString()
        == QString{moved + "/kit"}.toStdString());
    CHECK(
        score::locateFilePath(score::test::control_string(*here), doc->context())
            .toStdString()
        == moved.toStdString());
  });
}

TEST_CASE("a preset dropped in a library folder stores the files beside it", "[presets][library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    if(!procs.get(UuidKey<Process::ProcessModel>::fromString(looper_uuid)))
      SKIP("Fx not built");
    auto* handler = preset_handler(ctx);
    REQUIRE(handler);
    Library::ProcessesItemModel processes{ctx, nullptr};
    handler->setup(processes, ctx);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString root = score::test::canonical(tmp.path());
    const QString dest = root + "/library/Kits";
    score::test::write_wav(dest + "/kick.wav", 0.05);
    score::test::write_wav(root + "/elsewhere/other.wav", 0.05);

    SECTION("a sound in the folder it is dropped in")
    {
      QMimeData mime;
      mime.setData(
          score::mime::processpreset(), looper_preset("Beside", dest + "/kick.wav").toJson());
      REQUIRE(handler->onDrop(mime, 0, 0, QDir{dest}));
      const QByteArray written = contents(dest + "/Beside.scp");
      INFO(written.toStdString());
      CHECK(written.contains("\"<PRESET>:kick.wav\""));
      CHECK_FALSE(written.contains(root.toUtf8()));
    }

    SECTION("a sound elsewhere: the file is the preset dropped, byte for byte")
    {
      // With spaces score would not write, so that a rewrite would show.
      const QByteArray data
          = QStringLiteral(
                R"({ "Key": {"Uuid": "%1", "Effect": ""}, "Name": "Away", )"
                R"("Preset": [ [20, {"String": "%2/elsewhere/other.wav"}, "Loop file"] ] })")
                .arg(looper_uuid, root)
                .toUtf8();
      QMimeData mime;
      mime.setData(score::mime::processpreset(), data);
      REQUIRE(handler->onDrop(mime, 0, 0, QDir{dest}));
      CHECK(contents(dest + "/Away.scp") == data);
    }
  });
}

TEST_CASE("a preset file is read with its paths resolved", "[presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    if(!procs.get(UuidKey<Process::ProcessModel>::fromString(looper_uuid)))
      SKIP("Fx not built");

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString root = score::test::canonical(tmp.path());
    const QString file = root + "/kit/Kick.scp";
    score::test::write_wav(root + "/kit/kick.wav", 0.05);
    score::test::write_file(file, looper_preset("Kick", "<PRESET>:kick.wav").toJson());

    auto p = Process::Preset::fromFile(procs, file);
    REQUIRE(p);
    CHECK(p->path.toStdString() == file.toStdString());
    CHECK(p->name == "Kick");
    CHECK(p->data == QStringLiteral(R"([[20,{"String":"%1/kit/kick.wav"},"Loop file"]])").arg(root).toUtf8());

    // What cannot be read is no preset.
    CHECK_FALSE(Process::Preset::fromFile(procs, root + "/kit/missing.scp"));
    score::test::write_file(root + "/broken.scp", "{ not json");
    CHECK_FALSE(Process::Preset::fromFile(procs, root + "/broken.scp"));
    auto unknown = looper_preset("Unknown", "<PRESET>:kick.wav");
    unknown.key.key
        = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("00000000-0000-0000-0000-000000000001"));
    score::test::write_file(root + "/kit/unknown.scp", unknown.toJson());
    CHECK_FALSE(Process::Preset::fromFile(procs, root + "/kit/unknown.scp"));
  });
}
