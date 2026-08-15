// Integration test for clearing out a project folder: consolidating copies
// media in and never takes it out, so a project that has been worked on for a
// while carries the samples of every idea that was tried and dropped.
//
// Most of what is asserted here is what it refuses to do. Deciding a file is
// unused is deciding that nothing score knows about points at it, and acting
// on that decision deletes data -- so the guards matter more than the feature.
//
// Runs with the filesystem read-only outside /tmp (score_add_test ...
// SANDBOXED), so a regression in the delete path cannot reach a real file.

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <Process/MediaTrim.hpp>
#include <Process/ProjectConsolidation.hpp>
#include <Process/UnusedFiles.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <score/tools/File.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;

namespace
{
//! Absolute paths of everything the scan proposes to remove.
std::vector<QString> unused_paths(const Process::FileReport& r)
{
  std::vector<QString> out;
  for(const auto* e : r.with(Process::FileAction::Unused))
    out.push_back(e->sourcePath);
  return out;
}

bool lists(const Process::FileReport& r, const QString& needle)
{
  for(const auto* e : r.with(Process::FileAction::Unused))
    if(e->storedPath.contains(needle))
      return true;
  return false;
}
}

TEST_CASE("A file nothing points at any more is found", "[integration][unused]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/first.wav", 0.3);
    write_wav(media + "/second.wav", 0.3, 0x22);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/first.wav"));
    REQUIRE(sound != nullptr);

    Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(QFileInfo::exists(project + "/Audio/first.wav"));

    // Nothing is unused yet.
    CHECK(Process::analyzeUnusedFiles(doc->context(), {}).empty());

    // The idea changes: the process now reads another file, and the first one
    // stays behind in the project folder forever.
    sound->setFile(media + "/second.wav");
    Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(QFileInfo::exists(project + "/Audio/second.wav"));

    const auto scan = Process::analyzeUnusedFiles(doc->context(), {});
    CHECK(scan.count(Process::FileAction::Unused) == 1);
    CHECK(lists(scan, "first.wav"));
    CHECK_FALSE(lists(scan, "second.wav"));
    CHECK(scan.entries[0].size > 0);
  });
}

TEST_CASE("Moving aside keeps the file in the project", "[integration][unused]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kept.wav", 0.3);
    auto* doc = project_document(ctx, project);
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/kept.wav") != nullptr);
    Process::consolidateProjectFiles(doc->context(), {});

    // A file somebody dropped into the collected folder and never used.
    write_wav(project + "/Audio/orphan.wav", 0.3, 0x33);

    const auto scan = Process::analyzeUnusedFiles(doc->context(), {});
    REQUIRE(scan.count(Process::FileAction::Unused) == 1);
    REQUIRE(lists(scan, "orphan.wav"));

    const auto report
        = Process::removeUnusedFiles(doc->context(), unused_paths(scan), {});
    REQUIRE(report.count(Process::FileAction::Removed) == 1);

    CHECK_FALSE(QFileInfo::exists(project + "/Audio/orphan.wav"));
    // Out of the way, out of archives, and one drag from being back.
    CHECK(QFileInfo::exists(project + "/Unused/Audio/orphan.wav"));
    // The file that is used is exactly where it was.
    CHECK(QFileInfo::exists(project + "/Audio/kept.wav"));

    SECTION("and what was set aside is not offered again")
    {
      const auto again = Process::analyzeUnusedFiles(doc->context(), {});
      CHECK(again.count(Process::FileAction::Unused) == 0);
    }
  });
}

TEST_CASE("Deleting removes the file for good", "[integration][unused]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kept.wav", 0.3);
    auto* doc = project_document(ctx, project);
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/kept.wav") != nullptr);
    Process::consolidateProjectFiles(doc->context(), {});

    write_wav(project + "/Audio/orphan.wav", 0.3, 0x33);

    const auto scan = Process::analyzeUnusedFiles(doc->context(), {});
    REQUIRE(scan.count(Process::FileAction::Unused) == 1);

    Process::UnusedFilesOptions opts;
    opts.disposal = Process::UnusedDisposal::Delete;
    const auto report
        = Process::removeUnusedFiles(doc->context(), unused_paths(scan), opts);

    CHECK(report.count(Process::FileAction::Removed) == 1);
    CHECK_FALSE(QFileInfo::exists(project + "/Audio/orphan.wav"));
    CHECK_FALSE(QFileInfo::exists(project + "/Unused/Audio/orphan.wav"));
    CHECK(QFileInfo::exists(project + "/Audio/kept.wav"));
  });
}

TEST_CASE(
    "Removal refuses anything it should not touch, whatever it is told",
    "[integration][unused]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kept.wav", 0.3);
    write_wav(media + "/elsewhere.wav", 0.3, 0x44);

    auto* doc = project_document(ctx, project, "show.score");
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/kept.wav") != nullptr);
    Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(ctx.docManager.saveDocument(*doc));

    // A caller that has gone wrong, naming three things it must never get.
    const std::vector<QString> hostile{
        project + "/Audio/kept.wav",  // the project uses it
        media + "/elsewhere.wav",     // not in the project at all
        project + "/show.score"};     // the document itself

    const auto report = Process::removeUnusedFiles(doc->context(), hostile, {});

    CHECK(report.count(Process::FileAction::Removed) == 0);
    CHECK(report.count(Process::FileAction::Skipped) == 3);

    CHECK(QFileInfo::exists(project + "/Audio/kept.wav"));
    CHECK(QFileInfo::exists(media + "/elsewhere.wav"));
    CHECK(QFileInfo::exists(project + "/show.score"));
  });
}

TEST_CASE("Only the folders score fills are searched by default", "[integration][unused]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kept.wav", 0.3);
    auto* doc = project_document(ctx, project);
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/kept.wav") != nullptr);
    Process::consolidateProjectFiles(doc->context(), {});

    // Somebody's render, notes and stems, sitting in the project folder.
    write_file(project + "/notes.txt", "do not delete me");
    write_file(project + "/Renders/mixdown.wav", "rendered");

    // score did not put those there and does not propose them.
    const auto conservative = Process::analyzeUnusedFiles(doc->context(), {});
    CHECK(conservative.count(Process::FileAction::Unused) == 0);

    // Asked explicitly, it finds them.
    Process::UnusedFilesOptions everywhere;
    everywhere.onlyCollectedFolders = false;
    const auto wide = Process::analyzeUnusedFiles(doc->context(), everywhere);
    CHECK(lists(wide, "notes.txt"));
    CHECK(lists(wide, "mixdown.wav"));
    // Still never the document.
    CHECK_FALSE(lists(wide, ".score"));
  });
}

TEST_CASE(
    "What trimming leaves behind is what cleaning up finds",
    "[integration][unused][trim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/long.wav", 60.);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/long.wav"));
    REQUIRE(sound != nullptr);

    Process::consolidateProjectFiles(doc->context(), {});
    const QString collected = project + "/Audio/long.wav";
    REQUIRE(QFileInfo::exists(collected));

    // Only a couple of seconds are played.
    sound->setStartOffset(TimeVal::fromMsecs(10000.));
    sound->setLoops(true);
    sound->setLoopDuration(TimeVal::fromMsecs(2000.));

    Process::TrimOptions trim;
    trim.handles = 1.0;
    const auto trimReport = Process::trimProjectMedia(doc->context(), trim);
    REQUIRE(trimReport.count(Process::FileAction::Trimmed) == 1);

    // The two commands are meant to be used in this order, and the untrimmed
    // file becoming unused is the whole reason trimming can afford to keep it.
    REQUIRE(QFileInfo::exists(collected));

    const auto scan = Process::analyzeUnusedFiles(doc->context(), {});
    REQUIRE(scan.count(Process::FileAction::Unused) == 1);
    CHECK(scan.entries[0].sourcePath == collected);
    // The shortened file the document now reads is not in the list.
    CHECK_FALSE(lists(scan, "(trimmed)"));

    const auto removed
        = Process::removeUnusedFiles(doc->context(), unused_paths(scan), {});
    CHECK(removed.count(Process::FileAction::Removed) == 1);
    CHECK_FALSE(QFileInfo::exists(collected));
    CHECK(QFileInfo::exists(project + "/Unused/Audio/long.wav"));

    // And the document still plays what it played.
    CHECK(sound->userFilePath().startsWith("<PROJECT>:"));
    CHECK(QFileInfo::exists(
        score::locateFilePath(sound->userFilePath(), doc->context())));
  });
}

TEST_CASE("The reasons to hesitate are spelled out", "[integration][unused]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kept.wav", 0.3);
    write_wav(media + "/gone.wav", 0.3, 0x55);

    auto* doc = project_document(ctx, project);
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/kept.wav") != nullptr);
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/gone.wav") != nullptr);
    Process::consolidateProjectFiles(doc->context(), {});

    write_wav(project + "/Audio/orphan.wav", 0.3, 0x33);

    // A reference score cannot resolve is a reference whose file it cannot
    // recognise either -- the orphan might be exactly what it wants.
    REQUIRE(QFile::remove(project + "/Audio/gone.wav"));

    const auto scan = Process::analyzeUnusedFiles(doc->context(), {});
    const auto warnings = Process::unusedFilesWarnings(doc->context(), scan);

    REQUIRE(!warnings.isEmpty());
    bool mentionsMissing = false;
    bool mentionsUndo = false;
    for(const auto& w : warnings)
    {
      mentionsMissing |= w.contains("cannot be found");
      mentionsUndo |= w.contains("Undo");
    }
    CHECK(mentionsMissing);
    CHECK(mentionsUndo);
  });
}
