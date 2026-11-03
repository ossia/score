// score::OnlineExamples against a manifest and example files on disk, reached
// through file:// URLs: what the start screen relies on to list a downloaded
// example online too, to tell whether the online version changed since, and to
// download it again over the local copy.

#include <score/tools/Zip.hpp>

#include <core/document/OnlineExamples.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <optional>

namespace
{
QCoreApplication& app()
{
  // On every platform: an ini file under a folder of the test's own, rather
  // than the registry or the user's preferences. The cache goes to Qt's test
  // locations.
  static QTemporaryDir config;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
  QStandardPaths::setTestModeEnabled(true);

  static int argc = 1;
  static char arg0[] = "test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication a{argc, argv};
  return a;
}

bool waitFor(const std::function<bool()>& pred, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  return pred();
}

void writeFile(const QString& path, const QByteArray& data)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(data);
}

QByteArray readFile(const QString& path)
{
  QFile f{path};
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{};
}

//! The manifest entry of the looper example, downloaded from `file`.
QJsonObject looperEntry(const QString& file, const QString& format = "score")
{
  return QJsonObject{
      {"id", "examples/audio/looper"},
      {"name", "Looper"},
      {"section", "examples"},
      {"group", "audio"},
      {"file", QUrl::fromLocalFile(file).toString()},
      {"format", format},
      {"size", double(QFileInfo{file}.size())},
      {"platforms", QJsonArray{}}};
}

void writeManifest(const QString& manifest, const std::vector<QJsonObject>& entries)
{
  QJsonArray arr;
  for(const auto& e : entries)
    arr.push_back(e);
  writeFile(
      manifest,
      QJsonDocument{QJsonObject{{"version", 1}, {"examples", arr}}}.toJson());
}

//! A manifest listing one .score example, whose size is that of `file`.
void writeManifest(const QString& manifest, const QString& file)
{
  writeManifest(manifest, std::vector<QJsonObject>{looperEntry(file)});
}

//! A zip at `archive` holding `files`, as {name in the archive, content}.
void writeZip(
    const QString& archive, const std::vector<std::pair<QString, QByteArray>>& files)
{
  QTemporaryDir src;
  REQUIRE(src.isValid());
  std::vector<score::ZipEntry> entries;
  int i = 0;
  for(const auto& [name, content] : files)
  {
    const auto path = src.filePath(QString::number(i++));
    writeFile(path, content);
    entries.push_back({path, name});
  }
  QDir{}.mkpath(QFileInfo{archive}.absolutePath());
  QFile::remove(archive);
  QString error;
  REQUIRE(score::writeZipArchive(archive, entries, 0, error));
}

//! Every file under `folder`, relative to it, hidden ones included.
QStringList filesIn(const QString& folder)
{
  QStringList out;
  QDirIterator it{
      folder, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
      QDirIterator::Subdirectories};
  while(it.hasNext())
    out.push_back(QDir{folder}.relativeFilePath(it.next()));
  out.sort();
  return out;
}

//! Fetches the manifest as the start screen does, and waits for it.
void refresh(score::OnlineExamples& online)
{
  bool updated = false;
  auto c = QObject::connect(
      &online, &score::OnlineExamples::updated, [&] { updated = true; });
  online.refresh();
  REQUIRE(waitFor([&] { return updated; }));
  QObject::disconnect(c);
}

QString install(score::OnlineExamples& online, const score::OnlineExample& ex, bool replace)
{
  std::optional<QString> result;
  online.install(ex, [&](QString path) { result = std::move(path); }, replace);
  REQUIRE(waitFor([&] { return result.has_value(); }));
  return *result;
}

//! Library, cache and settings of their own, under a temporary folder.
struct Sandbox
{
  QTemporaryDir dir;
  QString library;
  QString source;
  QString manifest;

  Sandbox()
  {
    app();
    REQUIRE(dir.isValid());
    library = dir.filePath("library");
    source = dir.filePath("site/looper.score");
    manifest = dir.filePath("site/index.json");
    QDir{}.mkpath(library);

    qputenv("SCORE_EXAMPLES_MANIFEST", QUrl::fromLocalFile(manifest).toString().toUtf8());
    QCoreApplication::setOrganizationName("ossia");
    QCoreApplication::setApplicationName("score-test-online-examples");
    // The manifest cached by the previous test.
    QDir{QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
         + "/score-docs"}
        .removeRecursively();
    QSettings s;
    s.setValue("Library/RootPath", library);
    s.remove("score-docs");
  }
};
}

TEST_CASE("an online example is installed once, then opened from its copy", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");
  writeManifest(sb.manifest, sb.source);

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto ex = online.examples().front();

  CHECK(score::OnlineExamples::localPath(ex).isEmpty());
  CHECK_FALSE(score::OnlineExamples::changedOnline(ex));

  const auto path = install(online, ex, false);
  REQUIRE_FALSE(path.isEmpty());
  CHECK(path.startsWith(sb.library));
  CHECK(readFile(path) == "first version");
  CHECK(score::OnlineExamples::localPath(ex) == path);
  CHECK_FALSE(score::OnlineExamples::changedOnline(ex));

  // The manifest keeps listing it: nothing about the install removes it.
  refresh(online);
  CHECK(online.examples().size() == 1);

  // Installed: opening it again does not download it again.
  writeFile(sb.source, "second version, longer");
  CHECK(install(online, ex, false) == path);
  CHECK(readFile(path) == "first version");
}

TEST_CASE("a changed online version is noticed and replaces the copy on request", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");
  writeManifest(sb.manifest, sb.source);

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto path = install(online, online.examples().front(), false);
  REQUIRE_FALSE(path.isEmpty());

  // The site publishes a new version: another file, another size.
  writeFile(sb.source, "second version, longer");
  writeManifest(sb.manifest, sb.source);
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto updated = online.examples().front();
  CHECK(score::OnlineExamples::changedOnline(updated));

  CHECK(install(online, updated, true) == path);
  CHECK(readFile(path) == "second version, longer");
  CHECK_FALSE(score::OnlineExamples::changedOnline(updated));
}

TEST_CASE("a copy downloaded without a record is not reported as changed", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");
  writeManifest(sb.manifest, sb.source);

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto ex = online.examples().front();

  // As the start screen found copies before it recorded what it downloaded.
  writeFile(score::OnlineExamples::installFolder(ex) + ".score", "edited by the user");
  CHECK(score::OnlineExamples::localPath(ex) == score::OnlineExamples::installFolder(ex) + ".score");
  CHECK_FALSE(score::OnlineExamples::changedOnline(ex));
}

TEST_CASE("a failed download keeps the copy", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");
  writeManifest(sb.manifest, sb.source);

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto ex = online.examples().front();
  const auto path = install(online, ex, false);
  REQUIRE_FALSE(path.isEmpty());

  QFile::remove(sb.source);
  CHECK(install(online, ex, true).isEmpty());
  CHECK(readFile(path) == "first version");
}

// The site publishes each zip example with its files in a folder named as the
// example: voxels.zip holds voxels/voxels.score and voxels/Files/monu0.vox.
TEST_CASE("a zip example in its own folder installs as the site publishes it", "[online-examples]")
{
  Sandbox sb;
  const auto zip = sb.dir.filePath("site/looper.zip");
  writeZip(zip, {{"looper/looper.score", "first version"}, {"looper/Files/kick.wav", "kick"}});
  writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto ex = online.examples().front();
  const auto folder = score::OnlineExamples::installFolder(ex);

  const auto path = install(online, ex, false);
  REQUIRE(path == folder + "/looper.score");
  CHECK(readFile(path) == "first version");
  CHECK(filesIn(folder) == QStringList{"Files", "Files/kick.wav", "looper.score"});
  CHECK(score::OnlineExamples::localPath(ex) == path);

  // Replacing it works the same
  writeZip(zip, {{"looper/looper.score", "second version"}});
  writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});
  refresh(online);
  CHECK(install(online, online.examples().front(), true) == path);
  CHECK(readFile(path) == "second version");
  CHECK(filesIn(QFileInfo{folder}.absolutePath()) == QStringList{"looper", "looper/looper.score"});
}

// Copies installed by earlier versions kept the archive's folder inside the
// example's: they are still found.
TEST_CASE("a copy with the archive's folder inside is found", "[online-examples]")
{
  Sandbox sb;
  const auto zip = sb.dir.filePath("site/looper.zip");
  writeZip(zip, {{"looper/looper.score", "v"}});
  writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});
  score::OnlineExamples online;
  refresh(online);
  const auto ex = online.examples().front();
  const auto folder = score::OnlineExamples::installFolder(ex);
  writeFile(folder + "/looper/looper.score", "old layout");
  CHECK(score::OnlineExamples::localPath(ex) == folder + "/looper/looper.score");
}

TEST_CASE("a zip example replaces its whole folder, or nothing of it", "[online-examples]")
{
  Sandbox sb;
  const auto zip = sb.dir.filePath("site/looper.zip");
  writeZip(zip, {{"looper.score", "first version"}, {"media/kick.wav", "kick"}});
  writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  const auto first = online.examples().front();
  const auto folder = score::OnlineExamples::installFolder(first);
  const auto parent = QFileInfo{folder}.absolutePath();

  const auto path = install(online, first, false);
  REQUIRE(path == folder + "/looper.score");
  CHECK(filesIn(folder) == QStringList{"looper.score", "media", "media/kick.wav"});

  // What the user adds to the copy.
  writeFile(folder + "/notes.txt", "mine");

  SECTION("a new version takes the place of the folder")
  {
    writeZip(zip, {{"looper.score", "second version"}, {"media/snare.wav", "snare"}});
    writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});
    refresh(online);
    const auto second = online.examples().front();
    CHECK(score::OnlineExamples::changedOnline(second));

    CHECK(install(online, second, true) == path);
    CHECK(readFile(path) == "second version");
    CHECK(filesIn(folder) == QStringList{"looper.score", "media", "media/snare.wav"});
    // Nothing of the swap stays next to it.
    CHECK(filesIn(parent) == QStringList{"looper", "looper/looper.score", "looper/media", "looper/media/snare.wav"});
    CHECK_FALSE(score::OnlineExamples::changedOnline(second));
  }

  SECTION("an archive that cannot be written whole leaves the copy as it was")
  {
    // "data" is a file, so "data/take.wav" cannot be written.
    writeZip(
        zip, {{"looper.score", "second version"},
              {"data", "a file"},
              {"data/take.wav", "take"}});
    writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});
    refresh(online);
    const auto broken = online.examples().front();

    CHECK(install(online, broken, true).isEmpty());
    CHECK(readFile(path) == "first version");
    CHECK(
        filesIn(folder)
        == QStringList{"looper.score", "media", "media/kick.wav", "notes.txt"});
    CHECK(filesIn(parent).size() == 4 + 1);
    // Still what was downloaded.
    CHECK(score::OnlineExamples::changedOnline(broken));
  }

  SECTION("an archive without a score leaves the copy as it was")
  {
    writeZip(zip, {{"media/kick.wav", "kick"}});
    writeManifest(sb.manifest, std::vector<QJsonObject>{looperEntry(zip, "zip")});
    refresh(online);

    CHECK(install(online, online.examples().front(), true).isEmpty());
    CHECK(readFile(path) == "first version");
    CHECK(QFile::exists(folder + "/notes.txt"));
    CHECK(filesIn(parent).size() == 4 + 1);
  }
}

TEST_CASE("ids that name the library's own folders are refused", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");

  std::vector<QJsonObject> entries;
  for(const auto id :
      {".", "..", "examples/", "/examples", "examples//looper", "examples/./looper",
       "examples/../looper", "examples\\looper", "C:/looper", "examples/looper.",
       "examples/looper "})
  {
    auto e = looperEntry(sb.source);
    e["id"] = QString{id};
    entries.push_back(e);
  }
  // An extension that is not one.
  {
    auto e = looperEntry(sb.source);
    e["id"] = "examples/audio/sampler";
    e["format"] = "score/../../sampler";
    entries.push_back(e);
  }
  // A zip whose folder holds another example.
  {
    auto e = looperEntry(sb.source, "zip");
    e["id"] = "examples/audio";
    entries.push_back(e);
  }
  entries.push_back(looperEntry(sb.source));
  writeManifest(sb.manifest, entries);

  score::OnlineExamples online;
  refresh(online);
  REQUIRE(online.examples().size() == 1);
  CHECK(online.examples().front().id == "examples/audio/looper");

  // Built by hand rather than from the manifest: still refused.
  for(const auto id : {".", "", "examples/", "examples/.."})
  {
    score::OnlineExample ex;
    ex.id = id;
    ex.format = "zip";
    ex.file = QUrl::fromLocalFile(sb.source).toString();
    CHECK(score::OnlineExamples::installFolder(ex).isEmpty());
    CHECK(install(online, ex, true).isEmpty());
  }
  CHECK(filesIn(sb.library).isEmpty());
}

TEST_CASE("only a change of what describes the file marks the copy as changed", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");
  auto entry = looperEntry(sb.source);
  entry["sha256"] = "aaaa";
  writeManifest(sb.manifest, std::vector<QJsonObject>{entry});

  score::OnlineExamples online;
  refresh(online);
  REQUIRE_FALSE(install(online, online.examples().front(), false).isEmpty());

  // The page of the example is rewritten.
  entry["description"] = "A looper, explained better";
  entry["image"] = "https://example.com/looper.png";
  writeManifest(sb.manifest, std::vector<QJsonObject>{entry});
  refresh(online);
  CHECK_FALSE(score::OnlineExamples::changedOnline(online.examples().front()));

  // Another file of the same size.
  entry["sha256"] = "bbbb";
  writeManifest(sb.manifest, std::vector<QJsonObject>{entry});
  refresh(online);
  const auto changed = online.examples().front();
  CHECK(score::OnlineExamples::changedOnline(changed));

  // What copies downloaded before the fingerprint have: the size.
  const auto key = "score-docs/downloaded/" + changed.id;
  QSettings{}.setValue(key, changed.size);
  CHECK_FALSE(score::OnlineExamples::changedOnline(changed));
  QSettings{}.setValue(key, changed.size + 1);
  CHECK(score::OnlineExamples::changedOnline(changed));
}

TEST_CASE("asking again for an example being downloaded waits for that download", "[online-examples]")
{
  Sandbox sb;
  writeFile(sb.source, "first version");
  writeManifest(sb.manifest, sb.source);

  score::OnlineExamples online;
  refresh(online);
  const auto ex = online.examples().front();

  std::vector<QString> results;
  online.install(ex, [&](QString p) { results.push_back(p); }, true);
  CHECK(online.isDownloading(ex));
  online.install(ex, [&](QString p) { results.push_back(p); }, true);
  online.install(ex, [&](QString p) { results.push_back(p); }, false);

  REQUIRE(waitFor([&] { return results.size() == 3; }));
  CHECK_FALSE(online.isDownloading(ex));
  CHECK_FALSE(results[0].isEmpty());
  CHECK(results[1] == results[0]);
  CHECK(results[2] == results[0]);
}
