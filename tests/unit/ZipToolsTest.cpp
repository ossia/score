// The zip helpers behind project archives: what the start screen lists, reads
// and extracts. isSafeMemberName is a security boundary, hence the table of
// hostile names.

#include <score/tools/Zip.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct Fixture
{
  QTemporaryDir dir;
  QString root = dir.path();

  QString write(const QString& name, const QByteArray& content)
  {
    const QString path = root + "/" + name;
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(content);
    return path;
  }

  QString archive(const QString& name, const std::vector<score::ZipEntry>& entries)
  {
    const QString path = root + "/" + name;
    QString error;
    REQUIRE(score::writeZipArchive(path, entries, 1, error));
    return path;
  }
};
}

TEST_CASE("A project archive is summarized from its table of contents", "[zip]")
{
  Fixture fx;
  const auto score = fx.write("src/foo.score", R"({"Plugins":[]})");
  const auto media = fx.write("src/kick.wav", QByteArray(1000, 'x'));
  const auto zip = fx.archive("foo.zip", {{score, "foo.score"}, {media, "kick.wav"}});

  const auto summary = score::summarizeZipArchive(zip);
  REQUIRE(summary.has_value());
  CHECK(summary->scoreFile == "foo.score");
  CHECK(summary->files == 2);
  CHECK(summary->uncompressedSize == 1000 + 14);

  SECTION("a member is read back verbatim, and refused when too large")
  {
    QString error;
    CHECK(score::readZipMember(zip, "foo.score", error) == R"({"Plugins":[]})");
    CHECK(score::readZipMember(zip, "kick.wav", error, 100).isEmpty());
    CHECK(!error.isEmpty());
    CHECK(score::readZipMember(zip, "FOO.SCORE", error).isEmpty());
  }

  SECTION("extraction recreates the files")
  {
    const QString out = fx.root + "/out";
    QString error;
    REQUIRE(score::extractZipArchive(zip, out, error));
    CHECK(QFile::exists(out + "/foo.score"));
    CHECK(QFileInfo{out + "/kick.wav"}.size() == 1000);
  }

  SECTION("a cancelled extraction leaves nothing behind")
  {
    const QString out = fx.root + "/cancelled";
    QString error;
    CHECK(!score::extractZipArchive(zip, out, error, [](int, int) { return false; }));
    CHECK(!QFile::exists(out + "/foo.score"));
    CHECK(!QDir{out}.exists());
  }
}

TEST_CASE(
    "The score of an archive: shallowest first, then named like the archive", "[zip]")
{
  Fixture fx;
  const auto a = fx.write("a.score", "{}");

  CHECK(
      score::summarizeZipArchive(fx.archive("deep.zip", {{a, "x/y/deep.score"}}))
      == std::nullopt);
  CHECK(
      score::summarizeZipArchive(fx.archive("nested.zip", {{a, "nested/nested.score"}}))
          ->scoreFile
      == "nested/nested.score");
  CHECK(
      score::summarizeZipArchive(
          fx.archive("named.zip", {{a, "other.score"}, {a, "named.score"}}))
          ->scoreFile
      == "named.score");
  CHECK(
      score::summarizeZipArchive(
          fx.archive("root.zip", {{a, "sub/root.score"}, {a, "other.score"}}))
          ->scoreFile
      == "other.score");
  CHECK(
      score::summarizeZipArchive(fx.archive("none.zip", {{a, "readme.txt"}}))
      == std::nullopt);
  CHECK(score::summarizeZipArchive(a) == std::nullopt); // not a zip at all
}

TEST_CASE("Members that would escape the destination are refused", "[zip][security]")
{
  for(const char* name :
      {"", "/tmp/evil.score", "../evil.score", "sub/../../evil.score", "C:/evil.score",
       "c:evil.score", "sub\\evil.score", "..", "a/.."})
    CHECK(!score::isSafeZipMemberName(name));
  for(const char* name : {"evil.score", "sub/ok.score", "a..b/ok.score", ".hidden/x"})
    CHECK(score::isSafeZipMemberName(name));

  // The writer itself only refuses a leading slash: everything else can be
  // put in an archive and must be caught when reading it back
  Fixture fx;
  const auto a = fx.write("a.score", "{}");
  for(const char* name :
      {"../evil.score", "sub/../../evil.score", "C:/evil.score", "sub\\evil.score"})
  {
    const auto zip = fx.archive("hostile.zip", {{a, name}});
    CHECK(score::summarizeZipArchive(zip) == std::nullopt);

    const QString out = fx.root + "/hostile-out";
    QString error;
    CHECK(!score::extractZipArchive(zip, out, error));
    CHECK(!error.isEmpty());
    CHECK(!QFile::exists(fx.root + "/evil.score"));
    CHECK(!QFile::exists("/tmp/evil.score"));
  }
}
