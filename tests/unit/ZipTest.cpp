// Unit tests for score::writeZipArchive: what ends up in the archive, and
// what is left behind when it does not finish.

#include <score/tools/Zip.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <zipdownloader.hpp>

#include <utility>
#include <vector>

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

// Extraction back out of an archive: score installs third-party packages with
// this, so an entry naming a path outside the destination folder is an archive
// writing wherever it pleases on the machine that opens it.

namespace
{
QByteArray read_file(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::ReadOnly));
  return f.readAll();
}

//! An archive built member by member, with any name the writer allows:
//! anything but a leading slash.
QByteArray make_archive(const std::vector<std::pair<QByteArray, QByteArray>>& members)
{
  QTemporaryDir src;
  REQUIRE(src.isValid());

  std::vector<ZipEntry> entries;
  int i = 0;
  for(const auto& [name, content] : members)
  {
    const QString file = src.path() + QStringLiteral("/%1.bin").arg(i++);
    write_file(file, content);
    entries.push_back({file, QString::fromUtf8(name)});
  }

  const QString archive = src.path() + "/archive.zip";
  QString error;
  REQUIRE(writeZipArchive(archive, entries, 1, error));
  return read_file(archive);
}

//! Every file under `root`, as paths relative to it.
QStringList tree(const QString& root)
{
  QStringList res;
  QDirIterator it{
      root, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories};
  while(it.hasNext())
    res.push_back(QDir{root}.relativeFilePath(it.next()));
  res.sort();
  return res;
}
}

TEST_CASE("A wrapped archive extracts under its own folder", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString dest = QFileInfo{tmp.path()}.canonicalFilePath() + "/dest";
  REQUIRE(QDir{}.mkpath(dest));

  QString error;
  const auto files = zdl::unzip_all_files_to_folder(
      make_archive(
          {{"MyPkg/package.json", "{}"},
           {"MyPkg/sub/a.txt", "a"},
           {"MyPkg/.hidden", "h"}}),
      dest, error);

  CHECK(error.isEmpty());
  CHECK(files.size() == 3);
  // The archive's own top-level folder survives: the installer decides from it
  // whether the archive is wrapped.
  CHECK(
      tree(dest)
      == QStringList{"MyPkg/.hidden", "MyPkg/package.json", "MyPkg/sub/a.txt"});
  CHECK(read_file(dest + "/MyPkg/sub/a.txt") == QByteArray{"a"});
}

TEST_CASE("A \"..\" that stays inside the destination is kept", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString dest = QFileInfo{tmp.path()}.canonicalFilePath() + "/dest";
  REQUIRE(QDir{}.mkpath(dest));

  QString error;
  const auto files = zdl::unzip_all_files_to_folder(
      make_archive({{"a/b/../c.txt", "c"}, {"./a/d.txt", "d"}}), dest, error);

  CHECK(error.isEmpty());
  CHECK(files.size() == 2);
  CHECK(tree(dest) == QStringList{"a/c.txt", "a/d.txt"});
}

TEST_CASE(
    "An archive pointing outside the destination is refused", "[unit][zip][security]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();
  const QString dest = root + "/dest";
  REQUIRE(QDir{}.mkpath(dest));

  QByteArray evil = GENERATE(
      QByteArray{"../evil.txt"}, QByteArray{"a/../../evil.txt"},
      QByteArray{"../../../.config/autostart/evil.desktop"},
      QByteArray{"C:/evil.txt"}, QByteArray{"c:evil.txt"},
      QByteArray{"..\\evil.txt"}, QByteArray{"sub\\..\\..\\evil.txt"},
      QByteArray{"<absolute>"});

  // The writer is the one thing that refuses a leading slash, so the
  // absolute name goes in under a placeholder of the same length: nothing in
  // the archive is checksummed over a member name.
  QByteArray placeholder;
  if(evil == "<absolute>")
  {
    evil = QByteArray{"@"} + (root + "/evil.txt").toUtf8().mid(1);
    placeholder = evil;
    evil[0] = '/';
  }

  // A good entry comes first, so what is checked is that the whole archive is
  // refused rather than the bad member merely skipped.
  auto zip = make_archive(
      {{"MyPkg/package.json", "{}"},
       {placeholder.isEmpty() ? evil : placeholder, "pwned"}});
  if(!placeholder.isEmpty())
    zip.replace(placeholder, evil);

  QString error;
  const auto files = zdl::unzip_all_files_to_folder(zip, dest, error);

  CHECK(files.empty());
  CHECK_FALSE(error.isEmpty());

  // Nothing was written: not outside the destination, and not inside it either.
  CHECK(tree(root).isEmpty());
}

// Extraction of what Unix archives record beyond file contents. The fixtures
// were made with Info-ZIP's `zip -y`, the way the GPU runtime packages are:
//   lib/libfoo.so.9.27.0      a file, 0644
//   lib/libfoo.so.9  -> libfoo.so.9.27.0
//   lib/chain        -> libfoo.so.9          (a link to a link)
//   bin/tool                  a file, 0755
//   up               -> lib/../bin           (".." which stays inside)
#if !defined(_WIN32)
TEST_CASE("Symbolic links and permissions are restored", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString out = tmp.path();

  QString error;
  const auto files
      = zdl::unzip_all_files_to_folder(QStringLiteral(SCORE_TEST_ZIP_DIR "/links.zip"), out, error);
  CHECK(error.isEmpty());
  CHECK(files.size() == 5);

  const QFileInfo link{out + "/lib/libfoo.so.9"};
  REQUIRE(link.isSymLink());
  CHECK(link.symLinkTarget() == QFileInfo{out + "/lib/libfoo.so.9.27.0"}.absoluteFilePath());
  CHECK(QFileInfo{out + "/lib/chain"}.isSymLink());
  CHECK(QFileInfo{out + "/up"}.isSymLink());

  // Reading through the links reaches the file
  QFile f{out + "/lib/chain"};
  REQUIRE(f.open(QIODevice::ReadOnly));
  CHECK(f.readAll() == "real\n");

  CHECK(QFileInfo{out + "/bin/tool"}.isExecutable());
  CHECK(!QFileInfo{out + "/lib/libfoo.so.9.27.0"}.isExecutable());
}

TEST_CASE("The archive read from a file or from memory extracts the same", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());

  QFile f{QStringLiteral(SCORE_TEST_ZIP_DIR "/links.zip")};
  REQUIRE(f.open(QIODevice::ReadOnly));
  QString error;
  const auto files = zdl::unzip_all_files_to_folder(f.readAll(), tmp.path(), error);
  CHECK(error.isEmpty());
  CHECK(files.size() == 5);
  CHECK(QFileInfo{tmp.path() + "/lib/libfoo.so.9"}.isSymLink());
}

// alias/libfoo.so -> ../lib/libfoo.so.1: a folder holding nothing but a link
TEST_CASE("A link which is the only entry of its folder is created", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());

  QString error;
  const auto files = zdl::unzip_all_files_to_folder(
      QStringLiteral(SCORE_TEST_ZIP_DIR "/linkonly.zip"), tmp.path(), error);
  CHECK(error.isEmpty());
  CHECK(files.size() == 2);

  QFile f{tmp.path() + "/alias/libfoo.so"};
  REQUIRE(f.open(QIODevice::ReadOnly));
  CHECK(f.readAll() == "real\n");
}

// w: 0666, s: 04777
TEST_CASE("Nobody but the owner can write what is extracted", "[unit][zip]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());

  QString error;
  zdl::unzip_all_files_to_folder(
      QStringLiteral(SCORE_TEST_ZIP_DIR "/modes.zip"), tmp.path(), error);
  REQUIRE(error.isEmpty());

  for(const char* name : {"w", "s"})
  {
    const auto p = QFileInfo{tmp.path() + '/' + name}.permissions();
    CHECK((p & QFileDevice::WriteOwner));
    CHECK(!(p & (QFileDevice::WriteGroup | QFileDevice::WriteOther)));
  }
}
#endif

// Links which only point inside the destination as long as the links before
// them are read as text. Created in the archive's order, each one is followed
// by those after it:
//   chain.zip:   s/t/u -> ..,  s/t/u/k -> ../../../x,  s/t/u/k/victim.txt -> pwned
//   through.zip: d/u -> ..,  e -> d/u/../x
//   case.zip:    d/Lib -> ..,  d/lib/k -> ../../x  (d/lib is d/Lib on macOS and Windows)
// The destination is a folder of its own, next to an x/victim.txt which must
// stay as it is.
TEST_CASE("A link reaching outside through another link fails the whole archive", "[unit][zip]")
{
  const auto archive = GENERATE(
      QStringLiteral(SCORE_TEST_ZIP_DIR "/chain.zip"),
      QStringLiteral(SCORE_TEST_ZIP_DIR "/through.zip"),
      QStringLiteral(SCORE_TEST_ZIP_DIR "/case.zip"));

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString dest = tmp.path() + "/a/b/dest";
  REQUIRE(QDir{}.mkpath(dest));
  write_file(tmp.path() + "/a/b/x/victim.txt", "intact");
  write_file(tmp.path() + "/a/x/victim.txt", "intact");
  write_file(tmp.path() + "/x/victim.txt", "intact");

  QString error;
  const auto files = zdl::unzip_all_files_to_folder(archive, dest, error);
  CHECK(files.empty());
  CHECK_FALSE(error.isEmpty());
  CHECK(QDir{dest}.isEmpty());

  for(const char* victim : {"/a/b/x/victim.txt", "/a/x/victim.txt", "/x/victim.txt"})
  {
    const QFileInfo info{tmp.path() + victim};
    CHECK(!info.isSymLink());
    QFile f{info.filePath()};
    REQUIRE(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == "intact");
  }
}

TEST_CASE("A link pointing outside the destination fails the whole archive", "[unit][zip]")
{
  const auto archive = GENERATE(
      QStringLiteral(SCORE_TEST_ZIP_DIR "/escape.zip"),   // evil -> ../../etc/passwd
      QStringLiteral(SCORE_TEST_ZIP_DIR "/absolute.zip")); // evil -> /etc/passwd

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());

  QString error;
  const auto files = zdl::unzip_all_files_to_folder(archive, tmp.path(), error);
  CHECK(files.empty());
  CHECK(error.contains("evil"));
  CHECK(QDir{tmp.path()}.isEmpty());
}
