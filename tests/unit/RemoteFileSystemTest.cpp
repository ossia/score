// Browsing files that are on another machine.
//
// QFileSystemModel cannot: it is built on paths this process can stat, and a
// listing that crosses a socket is neither synchronous nor local. So the model
// is driven by score::Environment, and what matters is that it copes with
// answers arriving late, out of order, or not at all.

#include <Library/RemoteFileSystemModel.hpp>

#include <score/tools/Environment.hpp>
#include <score/tools/Uri.hpp>

#include <QMimeData>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

#include <map>

namespace
{
//! An environment whose answers are given by the test, when the test says so.
struct ScriptedEnvironment final : public score::Environment
{
  std::map<QString, std::vector<score::DirEntry>> contents;

  //! Listings asked for but not yet answered, so a test can decide when -- and
  //! whether -- an answer arrives.
  std::vector<std::pair<QString, Callback<std::vector<score::DirEntry>>>> pending;
  int listCalls = 0;

  bool isLocal() const noexcept override { return false; }
  QString resolve(const score::Uri&) const override { return {}; }

  void list(
      const score::Uri& uri, Callback<std::vector<score::DirEntry>> onListed,
      Callback<Failure>) override
  {
    ++listCalls;
    pending.emplace_back(uri.toString(), std::move(onListed));
  }

  void read(const score::Uri&, Callback<QByteArray>, Callback<Failure>) override { }
  void write(const score::Uri&, QByteArray, Done, Callback<Failure>) override { }

  //! Answer the oldest outstanding listing.
  void answer()
  {
    REQUIRE_FALSE(pending.empty());
    auto [path, cb] = pending.front();
    pending.erase(pending.begin());
    if(cb)
      cb(contents[path]);
  }
};

score::DirEntry entry(const QString& name, bool dir)
{
  score::DirEntry e;
  e.uri = score::Uri{score::UriScheme::Library, name};
  e.name = name;
  e.directory = dir;
  return e;
}
}

TEST_CASE("A remote folder is listed when it is opened", "[library][remote]")
{
  ScriptedEnvironment env;
  const auto root = score::Uri{score::UriScheme::Library, QString{}};
  env.contents[root.toString()]
      = {entry("sounds", true), entry("a.wav", false), entry("b.wav", false)};

  Library::RemoteFileSystemModel model{[&env] { return &env; }, nullptr};
  model.setRoot(root);

  // Nothing is fetched until something asks: a library can be large and it is
  // on the other end of a socket.
  CHECK(model.rowCount(QModelIndex{}) == 0);
  REQUIRE(model.canFetchMore(QModelIndex{}));

  model.fetchMore(QModelIndex{});
  CHECK(env.listCalls == 1);

  // Still nothing: the answer has not come back. A model that had rows here
  // would be inventing them.
  CHECK(model.rowCount(QModelIndex{}) == 0);

  env.answer();
  REQUIRE(model.rowCount(QModelIndex{}) == 3);

  // Folders first, then by name, as a file browser shows them.
  CHECK(model.data(model.index(0, 0, QModelIndex{}), Qt::DisplayRole).toString()
        == QStringLiteral("sounds"));
  CHECK(model.isDirectory(model.index(0, 0, QModelIndex{})));
  CHECK(model.data(model.index(1, 0, QModelIndex{}), Qt::DisplayRole).toString()
        == QStringLiteral("a.wav"));
  CHECK_FALSE(model.isDirectory(model.index(1, 0, QModelIndex{})));
}

TEST_CASE("A remote folder is asked about once", "[library][remote]")
{
  ScriptedEnvironment env;
  const auto root = score::Uri{score::UriScheme::Library, QString{}};
  env.contents[root.toString()] = {entry("a.wav", false)};

  Library::RemoteFileSystemModel model{[&env] { return &env; }, nullptr};
  model.setRoot(root);

  model.fetchMore(QModelIndex{});
  // A view calls canFetchMore constantly; each call must not be a request.
  CHECK_FALSE(model.canFetchMore(QModelIndex{}));
  model.fetchMore(QModelIndex{});
  CHECK(env.listCalls == 1);

  env.answer();
  CHECK(model.rowCount(QModelIndex{}) == 1);
  CHECK_FALSE(model.canFetchMore(QModelIndex{}));
}

TEST_CASE("An unanswered listing leaves the model usable", "[library][remote]")
{
  ScriptedEnvironment env;
  const auto root = score::Uri{score::UriScheme::Library, QString{}};

  Library::RemoteFileSystemModel model{[&env] { return &env; }, nullptr};
  model.setRoot(root);
  model.fetchMore(QModelIndex{});

  // The other machine never answers. Nothing must claim rows that are not
  // there, and nothing must crash.
  CHECK(model.rowCount(QModelIndex{}) == 0);
  CHECK_FALSE(model.index(0, 0, QModelIndex{}).isValid());
  CHECK(model.data(model.index(0, 0, QModelIndex{}), Qt::DisplayRole).isNull());
}

TEST_CASE("A listing that arrives after the model is gone is dropped",
          "[library][remote]")
{
  ScriptedEnvironment env;
  const auto root = score::Uri{score::UriScheme::Library, QString{}};
  env.contents[root.toString()] = {entry("a.wav", false)};

  {
    Library::RemoteFileSystemModel model{[&env] { return &env; }, nullptr};
    model.setRoot(root);
    model.fetchMore(QModelIndex{});
  }

  // The document was closed while a listing was in flight. Answering must not
  // touch the model that asked.
  REQUIRE_NOTHROW(env.answer());
}

TEST_CASE("A remote file can be dragged, a folder cannot", "[library][remote]")
{
  ScriptedEnvironment env;
  const auto root = score::Uri{score::UriScheme::Library, QString{}};
  env.contents[root.toString()] = {entry("sounds", true), entry("a.wav", false)};

  Library::RemoteFileSystemModel model{[&env] { return &env; }, nullptr};
  model.setRoot(root);
  model.fetchMore(QModelIndex{});
  env.answer();

  const auto folder = model.index(0, 0, QModelIndex{});
  const auto file = model.index(1, 0, QModelIndex{});

  CHECK_FALSE(model.flags(folder) & Qt::ItemIsDragEnabled);
  CHECK(model.flags(file) & Qt::ItemIsDragEnabled);

  // What travels is the uri, not a path, and under a type of its own: a
  // text/uri-list would claim these are files this machine can open, and every
  // existing drop handler would believe it.
  std::unique_ptr<QMimeData> mime{model.mimeData({file})};
  REQUIRE(mime);
  CHECK_FALSE(mime->hasUrls());
  REQUIRE(mime->hasFormat(score::remoteUriMimeType()));

  const auto payload = QString::fromUtf8(mime->data(score::remoteUriMimeType()));
  CHECK(payload.contains(QStringLiteral("a.wav")));
  CHECK(payload.startsWith(QStringLiteral("<LIBRARY>:")));

  // A folder alone yields nothing rather than an empty drop.
  CHECK(model.mimeData({folder}) == nullptr);
}

TEST_CASE("The environment is asked for again each time", "[library][remote]")
{
  // Document::environment() is created lazily and replaced once a session says
  // where the files are, and the panel is told the document exists before that
  // happens. A model that captured the first one would be calling through an
  // object the replacement destroyed -- which in the wasm build shows up as
  // "function signature mismatch" from a dead vtable.
  ScriptedEnvironment first, second;
  const auto root = score::Uri{score::UriScheme::Library, QString{}};
  second.contents[root.toString()] = {entry("from-the-other-machine", false)};

  score::Environment* current = &first;
  Library::RemoteFileSystemModel model{[&current] { return current; }, nullptr};
  model.setRoot(root);

  // Replaced before anything is listed, as a session does.
  current = &second;

  model.fetchMore(QModelIndex{});
  CHECK(first.listCalls == 0);
  REQUIRE(second.listCalls == 1);

  second.answer();
  REQUIRE(model.rowCount(QModelIndex{}) == 1);
  CHECK(model.data(model.index(0, 0, QModelIndex{}), Qt::DisplayRole).toString()
        == QStringLiteral("from-the-other-machine"));
}

TEST_CASE("No environment at all is not a crash", "[library][remote]")
{
  Library::RemoteFileSystemModel model{[] { return nullptr; }, nullptr};
  model.setRoot(score::Uri{score::UriScheme::Library, QString{}});

  REQUIRE_NOTHROW(model.fetchMore(QModelIndex{}));
  CHECK(model.rowCount(QModelIndex{}) == 0);
}
