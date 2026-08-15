// Integration test for trimming media down to the part a document reads.
//
// This is the only operation in score's file handling that can destroy audio,
// so most of what is asserted here is about what it refuses to do: it never
// touches a file outside the project folder, never writes over its source,
// never keeps a result that is not smaller, and keeps the untrimmed file
// unless explicitly told otherwise.
//
// The test itself runs with the filesystem read-only outside /tmp (see
// score_add_test ... SANDBOXED and tests/tools/sandboxed-test.sh), so a
// regression in any of that cannot reach anything real.

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <Process/MediaTrim.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace score::test;

namespace
{
constexpr double file_seconds = 60.;

//! A sound process reading `[start, start + length]` of its file.
void use_region(Media::Sound::ProcessModel& sound, double start, double length)
{
  sound.setStartOffset(TimeVal::fromMsecs(start * 1000.));
  sound.setLoops(true);
  sound.setLoopDuration(TimeVal::fromMsecs(length * 1000.));
}

qint64 size_of(const QString& path)
{
  return QFileInfo{path}.size();
}
}

TEST_CASE("Trimming shortens a collected file", "[integration][trim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/long.wav", file_seconds);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/long.wav"));
    REQUIRE(sound != nullptr);

    // Only the media inside the project is ever trimmed, so collect first.
    Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(sound->userFilePath() == "<PROJECT>:Audio/long.wav");

    const QString collected = project + "/Audio/long.wav";
    REQUIRE(QFileInfo::exists(collected));
    const qint64 originalSize = size_of(collected);

    use_region(*sound, /*start=*/10., /*length=*/2.);

    const auto range = sound->usedFileRange();
    REQUIRE(range.has_value());
    // The mapping into the file's own timeline is the one the waveform is
    // drawn with; assert it rather than assume it, so a tempo-related change
    // shows up here instead of as truncated audio.
    CHECK(std::abs(range->start - 10.) < 0.01);
    CHECK(std::abs(range->duration - 2.) < 0.01);

    Process::TrimOptions opts;
    opts.handles = 1.0;

    SECTION("the plan says what will shrink and by how much")
    {
      const auto plan = Process::analyzeMediaTrim(doc->context(), opts);
      const auto* entry = entry_for(plan, "long.wav");
      REQUIRE(entry != nullptr);
      CHECK(entry->action == Process::FileAction::Trimmed);
      CHECK(entry->newSize < entry->size);
      CHECK(plan.bytesSaved() > 0);

      // A plan writes nothing.
      CHECK(size_of(collected) == originalSize);
      CHECK(sound->userFilePath() == "<PROJECT>:Audio/long.wav");
    }

    SECTION("trimming writes a smaller file and repoints the document")
    {
      const auto report = Process::trimProjectMedia(doc->context(), opts);
      REQUIRE(report.count(Process::FileAction::Failed) == 0);
      REQUIRE(report.count(Process::FileAction::Trimmed) == 1);

      const auto* entry = entry_for(report, "long.wav");
      REQUIRE(entry != nullptr);
      REQUIRE(QFileInfo::exists(entry->destinationPath));
      CHECK(size_of(entry->destinationPath) < originalSize);
      CHECK(entry->newSize == size_of(entry->destinationPath));

      // The trimmed file lives in the project and the reference is relative.
      CHECK(sound->userFilePath().startsWith("<PROJECT>:"));
      CHECK(sound->userFilePath() != "<PROJECT>:Audio/long.wav");

      // The untrimmed file is still there: this is what undo falls back on.
      CHECK(QFileInfo::exists(collected));
      CHECK(size_of(collected) == originalSize);

      SECTION("and undo puts the reference back")
      {
        doc->commandStack().undo();
        CHECK(sound->userFilePath() == "<PROJECT>:Audio/long.wav");
      }

      SECTION("running it again finds nothing left to do")
      {
        const auto again = Process::trimProjectMedia(doc->context(), opts);
        CHECK(again.count(Process::FileAction::Trimmed) == 0);
      }
    }
  });
}

TEST_CASE("Media outside the project folder is never touched", "[integration][trim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    const QString outside = media + "/library/long.wav";
    write_wav(outside, file_seconds);
    const qint64 before = size_of(outside);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, outside));
    REQUIRE(sound != nullptr);
    use_region(*sound, 10., 2.);

    // Not consolidated: the file is somebody's sample library, and a document
    // pointing at it must not start editing it.
    Process::TrimOptions opts;
    opts.removeOriginal = true; // even asked to delete, it must not
    const auto report = Process::trimProjectMedia(doc->context(), opts);

    const auto* entry = entry_for(report, "long.wav");
    REQUIRE(entry != nullptr);
    CHECK(entry->action == Process::FileAction::Skipped);
    CHECK(entry->note.contains("outside"));

    CHECK(QFileInfo::exists(outside));
    CHECK(size_of(outside) == before);
    CHECK(sound->userFilePath() == outside);
    // Nothing was written beside it either.
    CHECK(QDir{media + "/library"}.entryList(QDir::Files).size() == 1);
  });
}

TEST_CASE("A file read by two processes keeps both regions", "[integration][trim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/long.wav", file_seconds);

    auto* doc = project_document(ctx, project);
    auto* early = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/long.wav"));
    auto* late = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/long.wav"));
    REQUIRE(early != nullptr);
    REQUIRE(late != nullptr);

    Process::consolidateProjectFiles(doc->context(), {});

    use_region(*early, 5., 1.);
    use_region(*late, 20., 1.);

    Process::TrimOptions opts;
    opts.handles = 0.5;

    const auto report = Process::trimProjectMedia(doc->context(), opts);
    REQUIRE(report.count(Process::FileAction::Failed) == 0);

    // One file, one trim, both references landing on it.
    const auto trimmed = report.with(Process::FileAction::Trimmed);
    REQUIRE(trimmed.size() == 2);
    CHECK(trimmed[0]->destinationPath == trimmed[1]->destinationPath);
    CHECK(early->userFilePath() == late->userFilePath());

    // The kept region spans both readers: 4.5s to 21.5s, so about 17s of the
    // original 60 -- not just the 1s either of them reads on its own.
    const double kept
        = double(size_of(trimmed[0]->destinationPath)) / (44100. * 4.);
    CHECK(kept > 15.);
    CHECK(kept < 19.);
  });
}

TEST_CASE("A file that is almost entirely used is left alone", "[integration][trim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    write_wav(media + "/short.wav", 4.);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/short.wav"));
    REQUIRE(sound != nullptr);

    Process::consolidateProjectFiles(doc->context(), {});
    use_region(*sound, 0., 3.5);

    const auto report = Process::trimProjectMedia(doc->context(), {});
    const auto* entry = entry_for(report, "short.wav");
    REQUIRE(entry != nullptr);
    CHECK(entry->action == Process::FileAction::Skipped);
    // And it says why, rather than doing nothing quietly.
    CHECK_FALSE(entry->note.isEmpty());

    CHECK(sound->userFilePath() == "<PROJECT>:Audio/short.wav");
  });
}

TEST_CASE(
    "Deleting the untrimmed file only ever touches the project",
    "[integration][trim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());

    const QString source = media + "/long.wav";
    write_wav(source, file_seconds);

    auto* doc = project_document(ctx, project);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, source));
    REQUIRE(sound != nullptr);

    Process::consolidateProjectFiles(doc->context(), {});
    const QString collected = project + "/Audio/long.wav";
    REQUIRE(QFileInfo::exists(collected));

    use_region(*sound, 10., 2.);

    Process::TrimOptions opts;
    opts.handles = 1.0;
    opts.removeOriginal = true;

    const auto report = Process::trimProjectMedia(doc->context(), opts);
    REQUIRE(report.count(Process::FileAction::Trimmed) == 1);

    const auto* entry = entry_for(report, "long.wav");
    REQUIRE(entry != nullptr);

    // The copy inside the project is gone...
    CHECK_FALSE(QFileInfo::exists(collected));
    CHECK(QFileInfo::exists(entry->destinationPath));

    // ...and the file it was copied from is untouched. Consolidation copies,
    // so the user's own media survives even the destructive option.
    CHECK(QFileInfo::exists(source));
    CHECK(size_of(source) > size_of(entry->destinationPath));
  });
}
