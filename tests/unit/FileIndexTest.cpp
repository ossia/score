// Unit tests for Process::FileIndex, the "search a folder for my missing
// files" half of relinking.
//
// The complaints these guard against are the ones every application with this
// feature collects: it only matches exact names, it picks the wrong one of
// several, and it hangs when pointed at a large drive.

#include <Process/MissingFiles.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
void write_file(const QString& path, const QByteArray& content)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(content);
}
}

TEST_CASE("The index finds files by name, at any depth", "[unit][missingfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  write_file(root + "/kick.wav", "one");
  write_file(root + "/deep/deeper/snare.wav", "two");

  Process::FileIndex index;
  index.scan(root);

  CHECK(index.fileCount() == 2);
  CHECK_FALSE(index.truncated());

  const auto kick = index.candidates("/gone/kick.wav");
  REQUIRE(kick.size() == 1);
  CHECK(kick[0] == root + "/kick.wav");

  const auto snare = index.candidates("Z:/elsewhere/snare.wav");
  REQUIRE(snare.size() == 1);
  CHECK(snare[0] == root + "/deep/deeper/snare.wav");

  CHECK(index.candidates("/gone/nothere.wav").empty());
}

TEST_CASE("Name matching ignores case", "[unit][missingfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  write_file(root + "/Kick.WAV", "one");

  Process::FileIndex index;
  index.scan(root);

  // A project authored on Windows or macOS carries whatever case it carries.
  const auto found = index.candidates("/gone/kick.wav");
  REQUIRE(found.size() == 1);
  CHECK(found[0] == root + "/Kick.WAV");
}

TEST_CASE("Among several candidates, size decides", "[unit][missingfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  write_file(root + "/near/kick.wav", QByteArray(10, 'x'));
  write_file(root + "/far/away/kick.wav", QByteArray(999, 'x'));

  Process::FileIndex index;
  index.scan(root);

  // Without a size to go on, the shallower path wins -- it is the likelier
  // one when the user pointed at the folder themselves.
  const auto blind = index.candidates("/gone/kick.wav");
  REQUIRE(blind.size() == 2);
  CHECK(blind[0] == root + "/near/kick.wav");

  // With a size, the file that actually is the missing one wins, however
  // deeply it is buried. This is the difference between "a file called
  // kick.wav" and "this kick.wav".
  const auto sized = index.candidates("/gone/kick.wav", 999);
  REQUIRE(sized.size() == 2);
  CHECK(sized[0] == root + "/far/away/kick.wav");

  // Both are still offered: guessing silently is how the wrong take ends up
  // in someone's show.
  CHECK(sized[1] == root + "/near/kick.wav");
}

TEST_CASE("A huge folder stops rather than hangs", "[unit][missingfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = QFileInfo{tmp.path()}.canonicalFilePath();

  for(int i = 0; i < 20; i++)
    write_file(root + QStringLiteral("/f%1.wav").arg(i), "x");

  Process::FileIndex index;
  index.scan(root, /*maxFiles=*/5);

  CHECK(index.fileCount() == 5);
  CHECK(index.truncated());
}

TEST_CASE("An empty or missing folder is not an error", "[unit][missingfiles]")
{
  Process::FileIndex index;
  index.scan({});
  CHECK(index.fileCount() == 0);

  index.scan("/definitely/not/a/folder/here");
  CHECK(index.fileCount() == 0);
  CHECK(index.candidates("/gone/kick.wav").empty());
}
