// Unit tests for score::writeZipArchive: what ends up in the archive, and
// what is left behind when it does not finish.

#include <score/tools/Zip.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <zipdownloader.hpp>

using namespace score;

namespace
{
void write_file(const QString& path, const QByteArray& content)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(content);
}

//! Names of the members of an archive, and their contents.
//!
//! Read back through score's own unzip path, so this also checks that what
//! score writes is what score can open.
struct ArchiveContents
{
  std::vector<QString> names;
  QByteArray read(const QString& name) const { return contents.value(name); }
  QHash<QString, QByteArray> contents;
};

ArchiveContents read_archive(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::ReadOnly));

  ArchiveContents out;
  for(auto& [name, data] : zdl::unzip_all_files_to_memory(f.readAll()))
  {
    out.names.push_back(name);
    out.contents.insert(name, data);
  }
  return out;
}
}

TEST_CASE("An archive holds exactly what it was given", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  write_file(root + "/project.score", "{\"a\":1}");
  write_file(root + "/Audio/kick.wav", QByteArray(50000, 'k'));

  const QString zipPath = root + "/out/archive.zip";
  QString error;
  const std::vector<ZipEntry> entries{
      {root + "/project.score", "MyShow/project.score"},
      {root + "/Audio/kick.wav", "MyShow/Audio/kick.wav"}};

  REQUIRE(writeZipArchive(zipPath, entries, 1, error));
  CHECK(error.isEmpty());
  REQUIRE(QFileInfo::exists(zipPath));

  const auto archive = read_archive(zipPath);
  CHECK(archive.names.size() == 2);
  CHECK(archive.read("MyShow/project.score") == QByteArray{"{\"a\":1}"});
  CHECK(archive.read("MyShow/Audio/kick.wav") == QByteArray(50000, 'k'));

  // Nothing half-written is left next to it.
  CHECK_FALSE(QFileInfo::exists(zipPath + ".part"));
}

TEST_CASE("A cancelled archive leaves nothing behind", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  write_file(root + "/a.bin", QByteArray(1000, 'a'));
  write_file(root + "/b.bin", QByteArray(1000, 'b'));

  const QString zipPath = root + "/archive.zip";
  QString error;
  const std::vector<ZipEntry> entries{
      {root + "/a.bin", "a.bin"}, {root + "/b.bin", "b.bin"}};

  // Give up after the first file.
  CHECK_FALSE(writeZipArchive(
      zipPath, entries, 1, error, [](int done, int) { return done < 1; }));
  CHECK_FALSE(error.isEmpty());

  // Neither a usable archive nor a misleading one.
  CHECK_FALSE(QFileInfo::exists(zipPath));
  CHECK_FALSE(QFileInfo::exists(zipPath + ".part"));
}

TEST_CASE("Archiving a file that is not there fails cleanly", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  const QString zipPath = root + "/archive.zip";
  QString error;
  CHECK_FALSE(writeZipArchive(zipPath, {{root + "/nope.bin", "nope.bin"}}, 1, error));
  CHECK_FALSE(error.isEmpty());
  CHECK_FALSE(QFileInfo::exists(zipPath));

  // And an empty request is an error, not an empty archive nobody asked for.
  CHECK_FALSE(writeZipArchive(zipPath, {}, 1, error));
}
