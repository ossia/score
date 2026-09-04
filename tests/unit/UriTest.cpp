// How a path is stored in a document, and what it resolves back to.
//
// Documents move between machines, so a path is only useful if it says what it
// is relative to. score already wrote "<PROJECT>:" and "<LIBRARY>:"; these pin
// the round-trip and the containment rule that decides which one applies.

#include <score/tools/Uri.hpp>

#include <QDir>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

TEST_CASE("A path under a directory is told from one merely spelled like it", "[uri]")
{
  using score::isUnder;

  CHECK(isUnder("/a/proj/sound.wav", "/a/proj"));
  CHECK(isUnder("/a/proj/sub/sound.wav", "/a/proj"));
  CHECK(isUnder("/a/proj", "/a/proj"));

  // The one that mattered: a prefix test says yes here, and the path then
  // relativizes to something that resolves to a different file.
  CHECK_FALSE(isUnder("/a/proj2/sound.wav", "/a/proj"));
  CHECK_FALSE(isUnder("/a/projector/sound.wav", "/a/proj"));

  CHECK_FALSE(isUnder("/b/other/sound.wav", "/a/proj"));
  CHECK_FALSE(isUnder("/a/proj/sound.wav", ""));
  CHECK_FALSE(isUnder("", "/a/proj"));

  // A trailing slash on the root is not a different root.
  CHECK(isUnder("/a/proj/sound.wav", "/a/proj/"));
}

TEST_CASE("Stored paths round-trip through parse and toString", "[uri]")
{
  using score::Uri;
  using score::UriScheme;

  const auto check = [](const QString& stored, UriScheme scheme, const QString& path) {
    const auto uri = Uri::parse(stored);
    INFO(stored.toStdString());
    CHECK(uri.scheme == scheme);
    CHECK(uri.path == path);
    CHECK(uri.toString() == stored);
  };

  check("<PROJECT>:sounds/a.wav", UriScheme::Project, "sounds/a.wav");
  check("<LIBRARY>:Presets/b.wav", UriScheme::Library, "Presets/b.wav");
  check("<CACHE>:sha256-abc/c.wav", UriScheme::Cache, "sha256-abc/c.wav");
  check("sounds/a.wav", UriScheme::Relative, "sounds/a.wav");
#if !defined(_WIN32)
  check("/abs/a.wav", UriScheme::Absolute, "/abs/a.wav");
#endif
}

TEST_CASE("Only an absolute path fails to travel", "[uri]")
{
  using score::Uri;
  using score::UriScheme;

  CHECK(Uri::parse("<PROJECT>:a.wav").isPortable());
  CHECK(Uri::parse("<LIBRARY>:a.wav").isPortable());
  CHECK(Uri::parse("<CACHE>:a.wav").isPortable());
  CHECK(Uri::parse("a.wav").isPortable());
#if !defined(_WIN32)
  CHECK_FALSE(Uri::parse("/abs/a.wav").isPortable());
#endif
}

TEST_CASE("An empty path stays empty rather than becoming the current directory",
          "[uri]")
{
  using score::Uri;
  CHECK(Uri::parse("").toString().isEmpty());
}
