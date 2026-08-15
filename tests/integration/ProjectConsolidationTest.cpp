// Integration test for project consolidation: a document referencing media
// scattered outside its folder must, after consolidating, own copies of that
// media and refer to them through <PROJECT>:.
//
// Three processes are exercised on purpose, one per way a process can hold a
// file:
//  * Media::Sound, whose path is a plain member relocated by a dedicated
//    command,
//  * an avendish process with a file port, whose path lives in a control
//    inlet and goes through the generic Process::ProcessModel traversal --
//    the path every avendish process and add-on takes, and
//  * Gfx::Video, whose path is a Q_PROPERTY rewritten generically.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/ProjectFiles.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
const auto sound_process_uuid = QStringLiteral("63174570-d608-44bf-a9cb-e6f5a11f73cc");
const auto obj_loader_uuid = QStringLiteral("5df71765-505f-4ab7-98c1-f305d10a01ef");
const auto video_process_uuid = QStringLiteral("32dc5341-7748-4c31-a226-82e6bd685744");

Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

void write_file(const QString& path, const QByteArray& content)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(content);
}

//! A real (if tiny) 16-bit mono WAV, so that the decoders do not have to be
//! stubbed out for the sound process to accept the file.
void write_wav(const QString& path, char sample_byte)
{
  constexpr int frames = 128;
  const int data_size = frames * 2;

  QByteArray out;
  const auto u32 = [&](quint32 v) {
    out.append(char(v & 0xff));
    out.append(char((v >> 8) & 0xff));
    out.append(char((v >> 16) & 0xff));
    out.append(char((v >> 24) & 0xff));
  };
  const auto u16 = [&](quint16 v) {
    out.append(char(v & 0xff));
    out.append(char((v >> 8) & 0xff));
  };

  out.append("RIFF");
  u32(36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(16);
  u16(1);     // PCM
  u16(1);     // mono
  u32(44100); // rate
  u32(88200); // byte rate
  u16(2);     // block align
  u16(16);    // bits
  out.append("data");
  u32(data_size);
  out.append(QByteArray(data_size, sample_byte));

  write_file(path, out);
}

Process::ProcessModel* add_process(
    score::Document& doc, const QString& uuid, const QString& data)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(uuid);
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(key);
  if(!factory)
    return nullptr;

  auto& interval = base_interval(doc);
  auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
      interval, factory->concreteKey(), data, QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);

  auto it = interval.processes.find(cmd->processId());
  return it != interval.processes.end() ? &(*it) : nullptr;
}

Process::ControlInlet* file_control(Process::ProcessModel& proc)
{
  for(auto* inlet : proc.inlets())
    if(auto* file = qobject_cast<Process::FileChooserBase*>(inlet))
      return file;
  return nullptr;
}

QString control_string(const Process::ControlInlet& inlet)
{
  return QString::fromStdString(ossia::convert<std::string>(inlet.value()));
}

const Process::ConsolidationEntry*
entry_for(const Process::ConsolidationReport& r, const QString& needle)
{
  for(const auto& e : r.entries)
    if(e.storedPath.contains(needle))
      return &e;
  return nullptr;
}
}

TEST_CASE(
    "Consolidating collects the media and repoints the document",
    "[integration][consolidation]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = QFileInfo{projectDir.path()}.canonicalFilePath();
    const QString media = QFileInfo{mediaDir.path()}.canonicalFilePath();

    write_wav(media + "/kick.wav", 0x11);
    write_file(media + "/model.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    // Not a decodable video: only the path handling is under test here.
    write_file(media + "/clip.mp4", "not really a video");

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    // A document has to live somewhere before anything can be relative to it.
    REQUIRE(ctx.docManager.saveDocumentAs(*doc, project + "/project.score"));
    REQUIRE(doc->metadata().projectFolder() == project);

    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    REQUIRE(sound != nullptr);
    REQUIRE(sound->userFilePath() == media + "/kick.wav");

    // The avendish process is optional: it lives in a plug-in that may not be
    // built. Its absence must not make this test lie about the rest.
    auto* loader = add_process(*doc, obj_loader_uuid, {});
    Process::ControlInlet* objPort = loader ? file_control(*loader) : nullptr;
    if(!objPort)
      WARN("no file-port process available: the generic control-port path is "
           "not covered by this run");
    if(objPort)
    {
      CommandDispatcher<> disp{doc->context().commandStack};
      disp.submit<Process::SetControlValue>(
          *objPort, ossia::value{QString{media + "/model.obj"}.toStdString()});
      REQUIRE(control_string(*objPort) == media + "/model.obj");
    }

    // A path held in a Q_PROPERTY rather than in a port: the third and last
    // way a process can store a file.
    auto* video = add_process(*doc, video_process_uuid, media + "/clip.mp4");
    if(video)
      REQUIRE(video->property("path").toString() == media + "/clip.mp4");

    const score::ConsolidateOptions opts{};

    SECTION("analysis reports what will happen without touching anything")
    {
      const auto report = Process::analyzeProjectFiles(doc->context(), opts);

      CHECK(report.projectFolder == project);
      const auto* kick = entry_for(report, "kick.wav");
      REQUIRE(kick != nullptr);
      CHECK(kick->action == Process::ConsolidationAction::Collect);
      CHECK(kick->kind == score::FileKind::Audio);
      CHECK(kick->destinationPath == project + "/Audio/kick.wav");
      CHECK(kick->newStoredPath == "<PROJECT>:Audio/kick.wav");
      CHECK(kick->copyNeeded);
      CHECK(report.bytesToCopy() > 0);

      // Nothing was written and the model still points at the original.
      CHECK_FALSE(QFileInfo::exists(project + "/Audio/kick.wav"));
      CHECK(sound->userFilePath() == media + "/kick.wav");
    }

    SECTION("consolidating copies the files and rewrites the references")
    {
      const auto report = Process::consolidateProjectFiles(doc->context(), opts);
      CHECK(report.count(Process::ConsolidationAction::Failed) == 0);

      CHECK(QFileInfo::exists(project + "/Audio/kick.wav"));
      CHECK(score::sameFileContents(
          media + "/kick.wav", project + "/Audio/kick.wav"));
      // The original is copied, never moved.
      CHECK(QFileInfo::exists(media + "/kick.wav"));

      CHECK(sound->userFilePath() == "<PROJECT>:Audio/kick.wav");

      if(objPort)
      {
        CHECK(QFileInfo::exists(project + "/Models/model.obj"));
        CHECK(control_string(*objPort) == "<PROJECT>:Models/model.obj");
      }

      if(video)
      {
        CHECK(QFileInfo::exists(project + "/Video/clip.mp4"));
        CHECK(video->property("path").toString() == "<PROJECT>:Video/clip.mp4");
      }

      SECTION("running it again changes nothing")
      {
        const auto again = Process::consolidateProjectFiles(doc->context(), opts);
        CHECK(again.count(Process::ConsolidationAction::Collect) == 0);
        CHECK(again.bytesToCopy() == 0);
        for(const auto& e : again.entries)
          CHECK_FALSE(e.copyNeeded);

        // No stray "kick (1).wav" next to the first copy.
        CHECK(QDir{project + "/Audio"}.entryList(QDir::Files).size() == 1);
        CHECK(sound->userFilePath() == "<PROJECT>:Audio/kick.wav");
      }

      SECTION("undo puts the references back")
      {
        doc->commandStack().undo();
        CHECK(sound->userFilePath() == media + "/kick.wav");
        if(objPort)
          CHECK(control_string(*objPort) == media + "/model.obj");
        if(video)
          CHECK(video->property("path").toString() == media + "/clip.mp4");

        // The copies are left on disk: consolidation is not a filesystem
        // transaction, and deleting files on undo would be far worse than
        // leaving a few behind.
        CHECK(QFileInfo::exists(project + "/Audio/kick.wav"));
      }
    }
  });
}

TEST_CASE(
    "Files that cannot be found are reported, not rewritten",
    "[integration][consolidation]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = QFileInfo{projectDir.path()}.canonicalFilePath();
    const QString media = QFileInfo{mediaDir.path()}.canonicalFilePath();

    write_wav(media + "/gone.wav", 0x22);

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    REQUIRE(ctx.docManager.saveDocumentAs(*doc, project + "/project.score"));

    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/gone.wav"));
    REQUIRE(sound != nullptr);

    // The file disappears after the document was built, which is exactly the
    // situation consolidation exists to surface.
    REQUIRE(QFile::remove(media + "/gone.wav"));

    const auto report = Process::consolidateProjectFiles(doc->context(), {});

    const auto* gone = entry_for(report, "gone.wav");
    REQUIRE(gone != nullptr);
    CHECK(gone->action == Process::ConsolidationAction::Missing);
    CHECK(gone->destinationPath.isEmpty());
    CHECK(report.missing().size() == 1);

    // A broken reference is left exactly as it was: rewriting it would only
    // move the breakage somewhere harder to diagnose.
    CHECK(sound->userFilePath() == media + "/gone.wav");
  });
}

TEST_CASE(
    "Project-relative references can be re-anchored", "[integration][consolidation]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = QFileInfo{projectDir.path()}.canonicalFilePath();
    const QString media = QFileInfo{mediaDir.path()}.canonicalFilePath();
    write_wav(media + "/kick.wav", 0x44);

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    REQUIRE(ctx.docManager.saveDocumentAs(*doc, project + "/project.score"));

    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    REQUIRE(sound != nullptr);

    CHECK(Process::countProjectRelativeFiles(doc->context()) == 0);

    Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(sound->userFilePath() == "<PROJECT>:Audio/kick.wav");
    CHECK(Process::countProjectRelativeFiles(doc->context()) == 1);

    // This is what has to happen when the document is saved into another
    // folder without its media: the reference is pinned to the file that
    // exists, instead of pointing into a folder that has none.
    CHECK(Process::reanchorProjectFiles(doc->context()) == 1);
    CHECK(sound->userFilePath() == project + "/Audio/kick.wav");
    CHECK(Process::countProjectRelativeFiles(doc->context()) == 0);

    // And it is undoable like everything else.
    doc->commandStack().undo();
    CHECK(sound->userFilePath() == "<PROJECT>:Audio/kick.wav");
  });
}

TEST_CASE(
    "Consolidating into another folder leaves the original alone",
    "[integration][consolidation]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, targetDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(targetDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = QFileInfo{projectDir.path()}.canonicalFilePath();
    const QString target = QFileInfo{targetDir.path()}.canonicalFilePath();
    const QString media = QFileInfo{mediaDir.path()}.canonicalFilePath();

    write_wav(media + "/kick.wav", 0x33);

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    REQUIRE(ctx.docManager.saveDocumentAs(*doc, project + "/project.score"));

    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    REQUIRE(sound != nullptr);

    // "Collect into a new project folder": references still resolve from the
    // document's current location, destinations are computed for the new one.
    const auto report = Process::consolidateProjectFiles(doc->context(), {}, target);

    CHECK(report.projectFolder == target);
    CHECK(QFileInfo::exists(target + "/Audio/kick.wav"));
    CHECK_FALSE(QFileInfo::exists(project + "/Audio/kick.wav"));
    CHECK(sound->userFilePath() == "<PROJECT>:Audio/kick.wav");
  });
}
