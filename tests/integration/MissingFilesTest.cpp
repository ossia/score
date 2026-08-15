// Integration test for the missing-file half of score's file handling: a
// document whose media moved must say so, and must be able to find it again.
//
// The behaviours asserted here are the ones users of other applications
// complain about the loudest: a scan that only matches exact names, a relink
// that guesses silently, and a relink that leaves the document just as fragile
// as it found it.

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <Process/MissingFiles.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;

TEST_CASE("A document says what it cannot find", "[integration][missingfiles]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kick.wav", 0.2);
    write_wav(media + "/snare.wav", 0.2, 0x22);

    auto* doc = project_document(ctx, project);
    auto* kick = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    auto* snare = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/snare.wav"));
    REQUIRE(kick != nullptr);
    REQUIRE(snare != nullptr);

    SECTION("nothing missing when everything is there")
    {
      const auto report = Process::scanMissingFiles(doc->context());
      CHECK(report.count(Process::FileAction::Missing) == 0);
      // The scan is also the answer to "how many files does this use".
      CHECK(report.count(Process::FileAction::Unchanged) >= 2);
    }

    SECTION("a file that went away is reported, with who uses it")
    {
      REQUIRE(QFile::remove(media + "/kick.wav"));

      const auto report = Process::scanMissingFiles(doc->context());
      REQUIRE(report.count(Process::FileAction::Missing) == 1);

      const auto* gone = entry_for(report, "kick.wav");
      REQUIRE(gone != nullptr);
      CHECK(gone->action == Process::FileAction::Missing);
      CHECK_FALSE(gone->owner.isEmpty());

      // The one that is still there is not dragged into it.
      CHECK(entry_for(report, "snare.wav")->action == Process::FileAction::Unchanged);
    }
  });
}

TEST_CASE(
    "A document loaded from disk reports its missing media",
    "[integration][missingfiles]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());
    const QString scorePath = project + "/onload.score";

    write_wav(media + "/kick.wav", 0.2);

    {
      auto* doc = project_document(ctx, project, "onload.score");
      REQUIRE(add_process(*doc, sound_process_uuid, media + "/kick.wav") != nullptr);
      REQUIRE(ctx.docManager.saveDocument(*doc));
      ctx.docManager.forceCloseDocument(ctx, *doc);
    }

    // The media goes away between two sessions, which is the situation the
    // whole feature exists for.
    REQUIRE(QFile::remove(media + "/kick.wav"));

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    REQUIRE(!delegates.empty());
    auto* reloaded
        = ctx.docManager.loadFile(ctx, scorePath);
    REQUIRE(reloaded != nullptr);

    // Loading fires the on_loadedDocument hook that surfaces this to the user;
    // let it run, then check the document itself knows.
    QApplication::processEvents();
    QApplication::processEvents();

    const auto report = Process::scanMissingFiles(reloaded->context());
    REQUIRE(report.count(Process::FileAction::Missing) == 1);
    CHECK(entry_for(report, "kick.wav") != nullptr);
  });
}

TEST_CASE("Relinking finds media that moved", "[integration][missingfiles]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, oldDir, newDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(oldDir.isValid());
    REQUIRE(newDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString oldMedia = canonical(oldDir.path());
    const QString newMedia = canonical(newDir.path());

    write_wav(oldMedia + "/kick.wav", 0.2);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, oldMedia + "/kick.wav"));
    REQUIRE(sound != nullptr);

    // The whole media folder was moved elsewhere, which is what actually
    // happens: a drive is remounted, a share is reorganised.
    write_wav(newMedia + "/Samples/kick.wav", 0.2);
    REQUIRE(QFile::remove(oldMedia + "/kick.wav"));

    const auto missing = Process::scanMissingFiles(doc->context());
    REQUIRE(missing.count(Process::FileAction::Missing) == 1);
    const auto* entry = entry_for(missing, "kick.wav");
    REQUIRE(entry != nullptr);

    Process::FileIndex index;
    index.scan(newMedia);
    const auto candidates = index.candidates(entry->storedPath);
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0] == newMedia + "/Samples/kick.wav");

    QHash<QString, QString> chosen;
    chosen.insert(entry->storedPath, candidates[0]);
    const auto report = Process::relinkFiles(doc->context(), chosen);

    CHECK(report.count(Process::FileAction::Relinked) == 1);
    CHECK(sound->userFilePath() == newMedia + "/Samples/kick.wav");
    CHECK(Process::scanMissingFiles(doc->context()).count(Process::FileAction::Missing)
          == 0);

    SECTION("and it is undoable like anything else")
    {
      doc->commandStack().undo();
      CHECK(sound->userFilePath() == oldMedia + "/kick.wav");
    }
  });
}

TEST_CASE(
    "Every process using a missing file is relinked at once",
    "[integration][missingfiles]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, oldDir, newDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(oldDir.isValid());
    REQUIRE(newDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString oldMedia = canonical(oldDir.path());
    const QString newMedia = canonical(newDir.path());

    write_wav(oldMedia + "/loop.wav", 0.2);

    auto* doc = project_document(ctx, project);
    auto* a = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, oldMedia + "/loop.wav"));
    auto* b = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, oldMedia + "/loop.wav"));
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    write_wav(newMedia + "/loop.wav", 0.2);
    REQUIRE(QFile::remove(oldMedia + "/loop.wav"));

    const auto missing = Process::scanMissingFiles(doc->context());
    // Two references, one file.
    CHECK(missing.count(Process::FileAction::Missing) == 2);

    QHash<QString, QString> chosen;
    chosen.insert(oldMedia + "/loop.wav", newMedia + "/loop.wav");
    const auto report = Process::relinkFiles(doc->context(), chosen);

    CHECK(report.count(Process::FileAction::Relinked) == 2);
    CHECK(a->userFilePath() == newMedia + "/loop.wav");
    CHECK(b->userFilePath() == newMedia + "/loop.wav");
  });
}

TEST_CASE(
    "Relinking to a file inside the project makes it portable",
    "[integration][missingfiles]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kick.wav", 0.2);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    REQUIRE(sound != nullptr);

    // Someone has already put a copy in the project folder by hand -- which
    // is exactly what a user does before asking score to find it.
    write_wav(project + "/Audio/kick.wav", 0.2);
    REQUIRE(QFile::remove(media + "/kick.wav"));

    QHash<QString, QString> chosen;
    chosen.insert(media + "/kick.wav", project + "/Audio/kick.wav");
    Process::relinkFiles(doc->context(), chosen);

    // Stored relative, not as the absolute path it was picked at: a relink is
    // a chance to leave the document more portable than it was found.
    CHECK(sound->userFilePath() == "<PROJECT>:Audio/kick.wav");
  });
}

TEST_CASE("Relinking to something that is not there fails", "[integration][missingfiles]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/kick.wav", 0.2);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    REQUIRE(sound != nullptr);
    REQUIRE(QFile::remove(media + "/kick.wav"));

    QHash<QString, QString> chosen;
    chosen.insert(media + "/kick.wav", media + "/not-here-either.wav");
    const auto report = Process::relinkFiles(doc->context(), chosen);

    CHECK(report.count(Process::FileAction::Failed) == 1);
    CHECK(report.count(Process::FileAction::Relinked) == 0);
    // And the reference is left as it was rather than pointed somewhere worse.
    CHECK(sound->userFilePath() == media + "/kick.wav");
  });
}
