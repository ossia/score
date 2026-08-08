// A dropped file is named by a path, and a path only means something on the
// machine holding it. While the score runs here that is fine; when it runs on
// another machine the process created there points at a file it does not have,
// which is what "dropping a file does nothing" turned out to be.
//
// score::importFile takes the bytes in: into the media cache here, named by
// content, and over to the other machine when there is one.

#include <score/tools/Environment.hpp>
#include <score/tools/File.hpp>
#include <score/tools/Uri.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct RecordingEnvironment final : score::Environment
{
  bool local{};
  std::vector<std::pair<score::Uri, QByteArray>> writes;

  explicit RecordingEnvironment(bool isLocal)
      : local{isLocal}
  {
  }

  bool isLocal() const noexcept override { return local; }
  QString resolve(const score::Uri&) const override { return {}; }
  void list(const score::Uri&, Callback<std::vector<score::DirEntry>>, Callback<Failure>)
      override
  {
  }
  void read(const score::Uri&, Callback<QByteArray>, Callback<Failure>) override { }
  void write(
      const score::Uri& uri, QByteArray data, Done onWritten, Callback<Failure>) override
  {
    writes.emplace_back(uri, std::move(data));
    if(onWritten)
      onWritten();
  }
};

//! Leaves the cache as it was found: this writes into the real one, since where
//! it is is exactly what is under test.
struct CacheEntry
{
  QString path;
  ~CacheEntry()
  {
    if(!path.isEmpty())
      QFile::remove(path);
  }
};
}

TEST_CASE("An imported file lands in the cache, named by content", "[import]")
{
  RecordingEnvironment env{true};
  const QByteArray data = "RIFF....some audio bytes";

  CacheEntry staged{score::importFile("kick drum.wav", data, env)};
  REQUIRE(!staged.path.isEmpty());

  // Under the cache, so relativizing gives "<CACHE>:" -- which is what makes
  // the stored path mean the same thing on both machines.
  CHECK(score::isUnder(staged.path, score::mediaCacheRoot()));
  CHECK(QFile::exists(staged.path));

  QFile f{staged.path};
  REQUIRE(f.open(QIODevice::ReadOnly));
  CHECK(f.readAll() == data);

  // The original name survives, so a process is not called after a hash.
  CHECK(staged.path.contains("kick_drum.wav"));

  // Nothing sent: the score runs here, the file is already where it is needed.
  CHECK(env.writes.empty());
}

TEST_CASE("Importing for another machine sends the bytes there", "[import]")
{
  RecordingEnvironment env{false};
  const QByteArray data = "RIFF....some audio bytes";

  CacheEntry staged{score::importFile("kick.wav", data, env)};
  REQUIRE(!staged.path.isEmpty());

  REQUIRE(env.writes.size() == 1);
  const auto& [uri, sent] = env.writes.front();

  // Addressed by the one scheme that means the same thing on both machines.
  CHECK(uri.scheme == score::UriScheme::Cache);
  CHECK(sent == data);

  // The very same entry the local copy went to, or the two machines disagree
  // about what the document refers to.
  CHECK(staged.path.endsWith(uri.path));
}

TEST_CASE("The same bytes are the same cache entry", "[import]")
{
  RecordingEnvironment env{true};
  const QByteArray data = "the same bytes";

  CacheEntry first{score::importFile("a.wav", data, env)};
  CacheEntry second{score::importFile("a.wav", data, env)};
  REQUIRE(!first.path.isEmpty());
  CHECK(first.path == second.path);

  // Different bytes under the same name must not collide.
  CacheEntry other{score::importFile("a.wav", QByteArray{"other bytes"}, env)};
  REQUIRE(!other.path.isEmpty());
  CHECK(other.path != first.path);
}

TEST_CASE("A file too large to send is refused, not half-imported", "[import]")
{
  RecordingEnvironment env{false};
  const QByteArray huge(score::maxInlineTransferBytes() + 1, 'x');

  // Nothing: a process naming a file the other machine will never have is
  // worse than a drop that visibly does nothing.
  CHECK(score::importFile("huge.wav", huge, env).isEmpty());
  CHECK(env.writes.empty());

  // The same file is fine when the score runs here.
  RecordingEnvironment localEnv{true};
  CacheEntry staged{score::importFile("huge.wav", huge, localEnv)};
  CHECK(!staged.path.isEmpty());
}

TEST_CASE("A picked file is left where it is when the score runs here", "[import]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString chosen = dir.path() + "/song.wav";
  {
    QFile f{chosen};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("some audio");
  }

  RecordingEnvironment env{true};

  // Copying every file a user ever picks would be a copy for nothing.
  CHECK(score::importPickedFile(chosen, env) == chosen);
  CHECK(env.writes.empty());
}

TEST_CASE("A picked file follows the score to the other machine", "[import]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString chosen = dir.path() + "/song.wav";
  const QByteArray data = "some audio";
  {
    QFile f{chosen};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(data);
  }

  RecordingEnvironment env{false};
  CacheEntry imported{score::importPickedFile(chosen, env)};

  // Not the path the user picked: that one names nothing over there.
  REQUIRE(!imported.path.isEmpty());
  CHECK(imported.path != chosen);
  CHECK(score::isUnder(imported.path, score::mediaCacheRoot()));

  REQUIRE(env.writes.size() == 1);
  CHECK(env.writes.front().first.scheme == score::UriScheme::Cache);
  CHECK(env.writes.front().second == data);
}

TEST_CASE("Cancelling the picker imports nothing", "[import]")
{
  RecordingEnvironment env{false};
  CHECK(score::importPickedFile({}, env).isEmpty());
  CHECK(env.writes.empty());
}
