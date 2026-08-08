// Dropping a file when the score runs on another machine.
//
// The drop itself was never the problem: it went through, the command
// replicated, and the host created the process -- pointing at a path only the
// machine that did the dropping had. Nothing could open it, so the drop looked
// like it had done nothing.
//
// What is asserted here is the decision, not the copying (that is
// test_unit_import_file): a drop on a non-local environment must take the bytes
// and send them, and a drop on a local one must still leave the file alone.

#include <Process/Drop/ProcessDropHandler.hpp>

#include <score/plugins/InterfaceList.hpp>
#include <score/tools/Environment.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/Uri.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct RecordingEnvironment final : score::Environment
{
  bool local{};
  std::vector<score::Uri> written;

  explicit RecordingEnvironment(bool isLocal)
      : local{isLocal}
  {
  }

  bool isLocal() const noexcept override { return local; }
  QString resolve(const score::Uri& uri) const override
  {
    return local ? uri.path : QString{};
  }
  void list(const score::Uri&, Callback<std::vector<score::DirEntry>>, Callback<Failure>)
      override
  {
  }
  void read(const score::Uri&, Callback<QByteArray>, Callback<Failure>) override { }
  void write(const score::Uri& uri, QByteArray, Done onWritten, Callback<Failure>) override
  {
    written.push_back(uri);
    if(onWritten)
      onWritten();
  }
};

//! A real file on disk, as a drop names one.
QUrl writeTempWav(QTemporaryDir& dir)
{
  const QString path = dir.path() + "/dropped.wav";
  QFile f{path};
  SCORE_ASSERT(f.open(QIODevice::WriteOnly));
  // A header sndfile will not choke on is not needed: what is under test is
  // which path comes out, and no decoder runs here.
  f.write(QByteArray{"RIFF....WAVEfmt some bytes"});
  f.close();
  return QUrl::fromLocalFile(path);
}
}

TEST_CASE("A file dropped for another machine is sent there", "[drop]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto env = std::make_unique<RecordingEnvironment>(false);
    auto* envPtr = env.get();
    doc->setEnvironment(std::move(env));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    QMimeData mime;
    mime.setUrls({writeTempWav(dir)});

    const auto& handlers = ctx.interfaces<Process::ProcessDropHandlerList>();
    handlers.getDrop(mime, doc->context());

    // The bytes went to the machine that will open them, addressed by the one
    // scheme that means the same thing on both.
    REQUIRE(envPtr->written.size() == 1);
    CHECK(envPtr->written.front().scheme == score::UriScheme::Cache);

    // And what the document will store for it must be that same portable
    // spelling: an empty or absolute path here is the whole bug.
    const QString cached = score::importFile("x.wav", QByteArray{"bytes"}, *envPtr);
    REQUIRE(!cached.isEmpty());
    const QString stored = score::relativizeFilePath(cached, doc->context());
    INFO("stored: " << stored.toStdString());
    CHECK(stored.startsWith("<CACHE>:"));
    QFile::remove(cached);
  });
}

TEST_CASE("A file dropped on the machine running the score is left alone", "[drop]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto env = std::make_unique<RecordingEnvironment>(true);
    auto* envPtr = env.get();
    doc->setEnvironment(std::move(env));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    QMimeData mime;
    mime.setUrls({writeTempWav(dir)});

    const auto& handlers = ctx.interfaces<Process::ProcessDropHandlerList>();
    handlers.getDrop(mime, doc->context());

    // Copying every dropped file into the cache would be a pointless copy of
    // everything a user ever drags in.
    CHECK(envPtr->written.empty());
  });
}
