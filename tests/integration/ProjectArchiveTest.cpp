// Integration test for archiving a project into a single zip.
//
// The interesting property is what the archive holds: it is built from the
// consolidation report rather than by listing the project folder, so it holds
// exactly the files this document uses -- not the neighbouring project, not
// last week's backup, not the untrimmed leftovers.

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <Process/ProjectArchive.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <score/tools/Zip.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <zipdownloader.hpp>

using namespace score::test;

namespace
{
std::vector<QString> archive_names(const QString& zipPath)
{
  QFile f{zipPath};
  REQUIRE(f.open(QIODevice::ReadOnly));

  std::vector<QString> names;
  for(auto& [name, data] : zdl::unzip_all_files_to_memory(f.readAll()))
    names.push_back(name);
  return names;
}

bool contains(const std::vector<QString>& names, const QString& needle)
{
  for(const auto& n : names)
    if(n == needle)
      return true;
  return false;
}
}

TEST_CASE("An archive holds the document and its media", "[integration][archive]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir, outDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());
    REQUIRE(outDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());
    const QString out = canonical(outDir.path());

    write_wav(media + "/kick.wav", 0.5);

    auto* doc = project_document(ctx, project, "MyShow.score");
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, media + "/kick.wav"));
    REQUIRE(sound != nullptr);

    // Something in the project folder this document does not use: it must not
    // end up in the archive.
    write_file(project + "/notes.txt", "unrelated");

    const auto report = Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(ctx.docManager.saveDocument(*doc));

    const auto contents = Process::projectArchiveContents(doc->context(), report);
    REQUIRE(!contents.empty());
    CHECK(Process::archiveContentsSize(contents) > 0);

    const QString zipPath = out + "/MyShow.zip";
    QString error;
    REQUIRE(score::writeZipArchive(zipPath, contents, 1, error));
    CHECK(error.isEmpty());

    const auto names = archive_names(zipPath);

    // Everything under one folder named after the document: unpacking never
    // scatters files into whatever directory the archive was opened in.
    for(const auto& n : names)
      CHECK(n.startsWith("MyShow/"));

    CHECK(contains(names, "MyShow/MyShow.score"));
    CHECK(contains(names, "MyShow/Audio/kick.wav"));
    CHECK_FALSE(contains(names, "MyShow/notes.txt"));
  });
}

TEST_CASE("Media that could not be found is not in the archive", "[integration][archive]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, mediaDir, outDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(mediaDir.isValid());
    REQUIRE(outDir.isValid());

    const QString project = canonical(projectDir.path());
    const QString media = canonical(mediaDir.path());
    const QString out = canonical(outDir.path());

    write_wav(media + "/present.wav", 0.5);
    write_wav(media + "/gone.wav", 0.5, 0x22);

    auto* doc = project_document(ctx, project, "Show.score");
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/present.wav") != nullptr);
    REQUIRE(add_process(*doc, sound_process_uuid, media + "/gone.wav") != nullptr);

    REQUIRE(QFile::remove(media + "/gone.wav"));

    const auto report = Process::consolidateProjectFiles(doc->context(), {});
    REQUIRE(report.count(Process::FileAction::Missing) == 1);
    REQUIRE(ctx.docManager.saveDocument(*doc));

    const auto contents = Process::projectArchiveContents(doc->context(), report);
    const QString zipPath = out + "/Show.zip";
    QString error;
    REQUIRE(score::writeZipArchive(zipPath, contents, 1, error));

    const auto names = archive_names(zipPath);
    CHECK(contains(names, "Show/Audio/present.wav"));
    CHECK_FALSE(contains(names, "Show/Audio/gone.wav"));

    // The count the user is warned with must match what actually happened.
    CHECK(names.size() == 2); // the document plus the one file that exists
  });
}
