// score::listRecursive walks a directory tree through the Environment
// interface. The interesting environment is the remote one, whose listings come
// back whenever they come back -- so the walk is tested against one that
// answers only when told to, and out of the order it was asked.

#include <score/tools/Environment.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>

namespace
{
//! A tree in memory. `deferred` holds the answers until they are released, the
//! way another machine does.
struct FakeEnvironment final : score::Environment
{
  std::map<QString, std::vector<score::DirEntry>> tree;
  std::vector<QString> unlistable;
  std::vector<std::function<void()>> deferred;
  bool defer{false};
  int listCalls{};

  bool isLocal() const noexcept override { return false; }
  QString resolve(const score::Uri&) const override { return {}; }

  void list(
      const score::Uri& uri, Callback<std::vector<score::DirEntry>> onListed,
      Callback<Failure> onFailed) override
  {
    listCalls++;
    const auto path = uri.path;

    auto answer = [this, path, onListed = std::move(onListed),
                   onFailed = std::move(onFailed)] {
      if(std::find(unlistable.begin(), unlistable.end(), path) != unlistable.end())
      {
        if(onFailed)
          onFailed("nope");
        return;
      }
      auto it = tree.find(path);
      if(onListed)
        onListed(it != tree.end() ? it->second : std::vector<score::DirEntry>{});
    };

    if(defer)
      deferred.push_back(std::move(answer));
    else
      answer();
  }

  void read(const score::Uri&, Callback<QByteArray>, Callback<Failure>) override { }
  void write(const score::Uri&, QByteArray, Done, Callback<Failure>) override { }

  void dir(const QString& at, std::vector<std::pair<QString, bool>> children)
  {
    auto& entries = tree[at];
    for(auto& [name, isDir] : children)
      entries.push_back(score::DirEntry{
          score::Uri{score::UriScheme::Library, at.isEmpty() ? name : at + '/' + name},
          name, isDir, 0});
  }
};

FakeEnvironment makeTree()
{
  FakeEnvironment env;
  env.dir("packages", {{"a.device", false}, {"vendor", true}, {"notes.txt", false}});
  env.dir("packages/vendor", {{"b.device", false}, {"deep", true}});
  env.dir("packages/vendor/deep", {{"c.device", false}});
  return env;
}

std::vector<QString> names(const std::vector<score::DirEntry>& entries)
{
  std::vector<QString> out;
  for(auto& e : entries)
    out.push_back(e.name);
  std::sort(out.begin(), out.end());
  return out;
}
}

TEST_CASE("A recursive walk finds matching files at every depth", "[environment]")
{
  auto env = makeTree();

  int called{};
  std::vector<score::DirEntry> got;
  score::listRecursive(
      env, score::Uri{score::UriScheme::Library, "packages"}, ".device",
      [&](std::vector<score::DirEntry> r) {
    called++;
    got = std::move(r);
      });

  CHECK(called == 1);
  CHECK(names(got) == std::vector<QString>{"a.device", "b.device", "c.device"});
}

TEST_CASE("A recursive walk reports once, however the answers arrive", "[environment]")
{
  auto env = makeTree();
  env.defer = true;

  int called{};
  std::vector<score::DirEntry> got;
  score::listRecursive(
      env, score::Uri{score::UriScheme::Library, "packages"}, ".device",
      [&](std::vector<score::DirEntry> r) {
    called++;
    got = std::move(r);
      });

  // Nothing has answered yet: reporting here would report an empty library.
  CHECK(called == 0);

  // Release them last-asked-first, which is what a walk over a network gets.
  while(!env.deferred.empty())
  {
    auto answer = env.deferred.back();
    env.deferred.pop_back();
    answer();
  }

  CHECK(called == 1);
  CHECK(names(got) == std::vector<QString>{"a.device", "b.device", "c.device"});
}

TEST_CASE("A directory that cannot be listed does not lose the others", "[environment]")
{
  auto env = makeTree();
  env.unlistable.push_back("packages/vendor");

  int called{};
  std::vector<score::DirEntry> got;
  score::listRecursive(
      env, score::Uri{score::UriScheme::Library, "packages"}, ".device",
      [&](std::vector<score::DirEntry> r) {
    called++;
    got = std::move(r);
      });

  CHECK(called == 1);
  CHECK(names(got) == std::vector<QString>{"a.device"});
}

TEST_CASE("A recursive walk stops at the depth it was given", "[environment]")
{
  auto env = makeTree();

  std::vector<score::DirEntry> got;
  score::listRecursive(
      env, score::Uri{score::UriScheme::Library, "packages"}, ".device",
      [&](std::vector<score::DirEntry> r) { got = std::move(r); }, 1);

  // One level below the root, so vendor is listed and deep is not.
  CHECK(names(got) == std::vector<QString>{"a.device", "b.device"});
}

TEST_CASE("A walk that loops back on itself terminates", "[environment]")
{
  // What a symlink to a parent looks like from here.
  FakeEnvironment env;
  env.dir("packages", {{"self", true}, {"a.device", false}});
  env.tree["packages/self"] = env.tree["packages"];
  env.tree["packages/self"][0].uri = score::Uri{score::UriScheme::Library, "packages"};

  int called{};
  score::listRecursive(
      env, score::Uri{score::UriScheme::Library, "packages"}, ".device",
      [&](std::vector<score::DirEntry>) { called++; }, 4);

  CHECK(called == 1);
  CHECK(env.listCalls <= 16);
}
