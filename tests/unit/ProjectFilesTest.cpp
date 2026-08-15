// Unit tests for the primitives project consolidation is built on:
// path math against explicit roots, cross-platform file-name sanitizing,
// content comparison, and destination placement (dedup, collisions, reuse).
//
// No application context: everything here takes its roots as arguments, which
// is exactly why score::locateFilePath / relativizeFilePath were split into a
// DocumentContext-free layer.

#include <score/tools/ProjectFiles.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

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

//! Canonical path of a temporary directory: QTemporaryDir hands out a path
//! under /tmp, which is a symlink on some distributions.
QString canonical(const QTemporaryDir& d)
{
  return QFileInfo{d.path()}.canonicalFilePath();
}
}

TEST_CASE("File kinds are guessed from the extension", "[unit][projectfiles]")
{
  CHECK(guessFileKind("/a/b/kick.wav") == FileKind::Audio);
  CHECK(guessFileKind("kick.WAV") == FileKind::Audio);
  CHECK(guessFileKind("clip.MP4") == FileKind::Video);
  CHECK(guessFileKind("logo.png") == FileKind::Image);
  CHECK(guessFileKind("song.mid") == FileKind::Midi);
  CHECK(guessFileKind("head.obj") == FileKind::Model3D);
  CHECK(guessFileKind("blur.frag") == FileKind::Shader);
  CHECK(guessFileKind("patch.pd") == FileKind::Script);
  CHECK(guessFileKind("points.json") == FileKind::Data);

  CHECK(guessFileKind("mystery.qwerty") == FileKind::Unknown);
  CHECK(guessFileKind("noextension") == FileKind::Unknown);

  // Every kind lands in a folder, including the ones we cannot identify.
  CHECK(mediaSubfolder(FileKind::Audio) == "Audio");
  CHECK(mediaSubfolder(FileKind::Video) == "Video");
  CHECK(mediaSubfolder(FileKind::Image) == "Images");
  CHECK_FALSE(mediaSubfolder(FileKind::Unknown).isEmpty());
}

TEST_CASE("File names are made safe on every platform", "[unit][projectfiles]")
{
  // Any directory part is dropped, whichever separator was used: a document
  // authored on Windows carries backslashes on a Unix machine.
  CHECK(sanitizeFileName("/home/me/kick.wav") == "kick.wav");
  CHECK(sanitizeFileName("C:\\media\\kick.wav") == "kick.wav");

  // Characters Windows rejects.
  CHECK(sanitizeFileName("a<b>c:d\"e|f?g*h.wav") == "a_b_c_d_e_f_g_h.wav");

  // Trailing dots and spaces are not creatable on Windows.
  CHECK(sanitizeFileName("track. ") == "track");
  CHECK(sanitizeFileName("  track.wav") == "track.wav");

  // Reserved DOS device names, with or without an extension.
  CHECK(sanitizeFileName("CON") == "_CON");
  CHECK(sanitizeFileName("nul.wav") == "_nul.wav");
  CHECK(sanitizeFileName("console.wav") == "console.wav");

  // Nothing left to work with.
  CHECK(sanitizeFileName("") == "file");
  CHECK(sanitizeFileName("...") == "file");

  // Very long names are shortened but keep their extension, so the decoders
  // that dispatch on the suffix still work.
  const QString long_name = QString{300, QChar('a')} + ".wav";
  const QString shortened = sanitizeFileName(long_name);
  CHECK(shortened.size() < 130);
  CHECK(shortened.endsWith(".wav"));
}

TEST_CASE("Stored paths resolve against explicit roots", "[unit][projectfiles]")
{
  QTemporaryDir project, library;
  REQUIRE(project.isValid());
  REQUIRE(library.isValid());

  const QString proj = canonical(project);
  const QString lib = canonical(library);

  write_file(proj + "/doc.score", "{}");
  write_file(proj + "/Audio/kick.wav", "kick");
  write_file(lib + "/Samples/snare.wav", "snare");

  const PathRoots roots{.documentFile = proj + "/doc.score", .library = lib};
  CHECK(roots.documentFolder() == proj);

  CHECK(locateFilePath("<PROJECT>:Audio/kick.wav", roots) == proj + "/Audio/kick.wav");
  CHECK(
      locateFilePath("<LIBRARY>:Samples/snare.wav", roots)
      == lib + "/Samples/snare.wav");
  CHECK(locateFilePath("Audio/kick.wav", roots) == proj + "/Audio/kick.wav");
  CHECK(locateFilePath("/elsewhere/x.wav", roots) == "/elsewhere/x.wav");
  CHECK(locateFilePath("", roots).isEmpty());
}

TEST_CASE("Absolute paths become project- or library-relative", "[unit][projectfiles]")
{
  QTemporaryDir project, library, outside;
  REQUIRE(project.isValid());
  REQUIRE(library.isValid());
  REQUIRE(outside.isValid());

  const QString proj = canonical(project);
  const QString lib = canonical(library);
  const QString out = canonical(outside);

  write_file(proj + "/doc.score", "{}");
  write_file(proj + "/Audio/kick.wav", "kick");
  write_file(lib + "/Samples/snare.wav", "snare");
  write_file(out + "/hat.wav", "hat");

  const PathRoots roots{.documentFile = proj + "/doc.score", .library = lib};

  CHECK(
      relativizeFilePath(proj + "/Audio/kick.wav", roots) == "<PROJECT>:Audio/kick.wav");
  CHECK(
      relativizeFilePath(lib + "/Samples/snare.wav", roots)
      == "<LIBRARY>:Samples/snare.wav");

  // Outside both roots: left alone.
  CHECK(relativizeFilePath(out + "/hat.wav", roots) == out + "/hat.wav");

  // Already relative or already prefixed: idempotent.
  CHECK(relativizeFilePath("<PROJECT>:Audio/kick.wav", roots) == "<PROJECT>:Audio/kick.wav");
  CHECK(relativizeFilePath("<LIBRARY>:x.wav", roots) == "<LIBRARY>:x.wav");
  CHECK(relativizeFilePath("Audio/kick.wav", roots) == "Audio/kick.wav");
  CHECK(relativizeFilePath("", roots).isEmpty());
}

TEST_CASE("The project folder of an unwritten document", "[unit][projectfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = canonical(tmp);

  // QFileInfo::canonicalPath() answers "." — not an empty string — for a file
  // that does not exist. Taking it at face value anchors the whole project to
  // the current working directory.
  const PathRoots pending{.documentFile = root + "/not-saved-yet.score"};
  CHECK(pending.documentFolder() == root);

  write_file(root + "/kick.wav", "kick");
  CHECK(relativizeFilePath(root + "/kick.wav", pending) == "<PROJECT>:kick.wav");
  CHECK(locateFilePath("<PROJECT>:kick.wav", pending) == root + "/kick.wav");

  // A folder that does not exist either: lexical, but still not "."
  const PathRoots future{.documentFile = root + "/new/deeper/doc.score"};
  CHECK(future.documentFolder() == root + "/new/deeper");

  // No document at all: nothing is project-relative.
  const PathRoots none{};
  CHECK(none.documentFolder().isEmpty());
  CHECK(relativizeFilePath(root + "/kick.wav", none) == root + "/kick.wav");
}

TEST_CASE(
    "A sibling folder sharing a prefix is not mistaken for the project",
    "[unit][projectfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = canonical(tmp);

  write_file(root + "/projA/doc.score", "{}");
  write_file(root + "/projAB/kick.wav", "kick");

  const PathRoots roots{.documentFile = root + "/projA/doc.score"};

  // A plain startsWith() would have turned this into "<PROJECT>:B/kick.wav".
  CHECK(
      relativizeFilePath(root + "/projAB/kick.wav", roots)
      == root + "/projAB/kick.wav");
}

TEST_CASE("Files are compared by content", "[unit][projectfiles]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = canonical(tmp);

  write_file(root + "/a.bin", QByteArray(200000, 'x'));
  write_file(root + "/same.bin", QByteArray(200000, 'x'));
  write_file(root + "/different.bin", QByteArray(200000, 'y'));
  write_file(root + "/shorter.bin", QByteArray(100000, 'x'));

  CHECK(sameFileContents(root + "/a.bin", root + "/a.bin"));
  CHECK(sameFileContents(root + "/a.bin", root + "/same.bin"));
  CHECK_FALSE(sameFileContents(root + "/a.bin", root + "/different.bin"));
  CHECK_FALSE(sameFileContents(root + "/a.bin", root + "/shorter.bin"));
  CHECK_FALSE(sameFileContents(root + "/a.bin", root + "/nope.bin"));
}

TEST_CASE("Placement copies a file once, whatever the reference count",
          "[unit][projectfiles]")
{
  QTemporaryDir project, media;
  REQUIRE(project.isValid());
  REQUIRE(media.isValid());

  const QString proj = canonical(project);
  const QString src = canonical(media);
  write_file(src + "/kick.wav", "kick");

  FilePlacement placement{proj};

  const auto first = placement.place(src + "/kick.wav", FileKind::Audio);
  CHECK(first.destination == proj + "/Audio/kick.wav");
  CHECK_FALSE(first.alreadyInProject);
  CHECK_FALSE(first.reused);

  const auto second = placement.place(src + "/kick.wav", FileKind::Audio);
  CHECK(second.destination == first.destination);
  CHECK(second.reused);
}

TEST_CASE("Two different files with the same name do not collide",
          "[unit][projectfiles]")
{
  QTemporaryDir project, media;
  REQUIRE(project.isValid());
  REQUIRE(media.isValid());

  const QString proj = canonical(project);
  const QString src = canonical(media);
  write_file(src + "/kicks/kick.wav", "one");
  write_file(src + "/snares/kick.wav", "two");

  FilePlacement placement{proj};

  const auto a = placement.place(src + "/kicks/kick.wav", FileKind::Audio);
  const auto b = placement.place(src + "/snares/kick.wav", FileKind::Audio);

  CHECK(a.destination == proj + "/Audio/kick.wav");
  CHECK(b.destination == proj + "/Audio/kick (1).wav");
  CHECK(a.destination != b.destination);
}

TEST_CASE("An identical file already collected is reused", "[unit][projectfiles]")
{
  QTemporaryDir project, media;
  REQUIRE(project.isValid());
  REQUIRE(media.isValid());

  const QString proj = canonical(project);
  const QString src = canonical(media);
  write_file(src + "/kick.wav", "kick");
  // As if a previous consolidation had already put it there.
  write_file(proj + "/Audio/kick.wav", "kick");

  FilePlacement placement{proj};
  const auto p = placement.place(src + "/kick.wav", FileKind::Audio);

  CHECK(p.destination == proj + "/Audio/kick.wav");
  CHECK(p.reused);
  CHECK_FALSE(p.alreadyInProject);

  // A file of the same name but different content must not be clobbered.
  write_file(src + "/other/kick.wav", "different");
  FilePlacement other{proj};
  const auto q = other.place(src + "/other/kick.wav", FileKind::Audio);
  CHECK(q.destination == proj + "/Audio/kick (1).wav");
  CHECK_FALSE(q.reused);
}

TEST_CASE("A file already inside the project stays put", "[unit][projectfiles]")
{
  QTemporaryDir project;
  REQUIRE(project.isValid());
  const QString proj = canonical(project);
  write_file(proj + "/somewhere/kick.wav", "kick");

  FilePlacement placement{proj};
  const auto p = placement.place(proj + "/somewhere/kick.wav", FileKind::Audio);

  CHECK(p.alreadyInProject);
  CHECK(p.destination == proj + "/somewhere/kick.wav");
}

TEST_CASE("Placement layout follows the options", "[unit][projectfiles]")
{
  QTemporaryDir project, media;
  REQUIRE(project.isValid());
  REQUIRE(media.isValid());
  const QString proj = canonical(project);
  const QString src = canonical(media);
  write_file(src + "/Kicks/kick.wav", "kick");

  {
    ConsolidateOptions flat{.useKindSubfolders = false};
    FilePlacement p{proj, flat};
    CHECK(p.place(src + "/Kicks/kick.wav", FileKind::Audio).destination
          == proj + "/kick.wav");
  }
  {
    ConsolidateOptions keep{.keepSourceFolderName = true};
    FilePlacement p{proj, keep};
    CHECK(p.place(src + "/Kicks/kick.wav", FileKind::Audio).destination
          == proj + "/Audio/Kicks/kick.wav");
  }
}

TEST_CASE("Materializing creates the file and never overwrites",
          "[unit][projectfiles]")
{
  QTemporaryDir project, media;
  REQUIRE(project.isValid());
  REQUIRE(media.isValid());
  const QString proj = canonical(project);
  const QString src = canonical(media);
  write_file(src + "/kick.wav", "kick");

  QString error;
  const QString dst = proj + "/Audio/kick.wav";
  REQUIRE(materializeFile(src + "/kick.wav", dst, CopyMode::Copy, error));
  CHECK(error.isEmpty());
  CHECK(QFileInfo::exists(dst));
  CHECK(sameFileContents(src + "/kick.wav", dst));

  // Second attempt: refuses rather than destroying whatever is there.
  CHECK_FALSE(materializeFile(src + "/kick.wav", dst, CopyMode::Copy, error));
  CHECK_FALSE(error.isEmpty());

#if !defined(_WIN32)
  const QString link = proj + "/Audio/link.wav";
  REQUIRE(materializeFile(src + "/kick.wav", link, CopyMode::Symlink, error));
  CHECK(QFileInfo{link}.isSymLink());
  CHECK(sameFileContents(src + "/kick.wav", link));
#endif
}
