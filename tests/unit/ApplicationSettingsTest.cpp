// What the command line means, for the options that decide whether anything
// modal can appear at startup. A script has nobody to answer a dialog, so the
// flags it implies matter more than most.

#include <core/application/ApplicationSettings.hpp>

#include <QCoreApplication>
#include <QStringList>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
//! parse() takes argc/argv as well as the list, and rewrites them.
score::ApplicationSettings parse(QStringList args)
{
  std::vector<QByteArray> storage;
  storage.reserve(args.size());
  for(const auto& a : args)
    storage.push_back(a.toUtf8());

  std::vector<char*> argv;
  argv.reserve(storage.size() + 1);
  for(auto& s : storage)
    argv.push_back(s.data());
  argv.push_back(nullptr);

  int argc = int(storage.size());
  score::ApplicationSettings set;
  set.parse(args, argc, argv.data());
  return set;
}
}

TEST_CASE("a plain start restores the previous session", "[app][settings]")
{
  const auto set = parse({"score"});
  CHECK(set.tryToRestore);
  CHECK(!set.hasScript);
}

TEST_CASE("--no-restore stops the restore", "[app][settings]")
{
  const auto set = parse({"score", "--no-restore"});
  CHECK(!set.tryToRestore);
}

// A script expects the document it was given. Restoring hands it a different
// one, and the start screen that offers to do so has nobody to click it.
TEST_CASE("--script implies no restore", "[app][settings]")
{
  const auto set = parse({"score", "--script", "/tmp/nothing.js"});
  CHECK(set.hasScript);
  CHECK(!set.tryToRestore);
}

TEST_CASE("--script and a file still means no restore", "[app][settings]")
{
  const auto set = parse({"score", "--script", "/tmp/nothing.js", "/tmp/doc.score"});
  CHECK(set.hasScript);
  CHECK(!set.tryToRestore);
}
