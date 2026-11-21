// Snippets: .scenario files the process library lists under its Snippets
// branch, in the folders they are saved in.

#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Scenario/Library/Snippets.hpp>

#include <Library/Snippet.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
std::string utf8(const QString& s)
{
  return s.toStdString();
}
}

TEST_CASE("A snippet is listed under the folders below its snippets folder", "[snippets]")
{
  using Scenario::snippetCategories;
  CHECK(snippetCategories("/root/packages/user/snippets").isEmpty());
  CHECK(snippetCategories("/root/packages/user/snippets/Drums") == QStringList{"Drums"});
  CHECK(
      snippetCategories("/root/packages/user/snippets/Drums/Kits")
      == (QStringList{"Drums", "Kits"}));
  // A package's own snippets folder.
  CHECK(
      snippetCategories("/root/packages/a-package/snippets/Fx") == QStringList{"Fx"});
  // Saved elsewhere, by a drop in the library's file view: its folder.
  CHECK(snippetCategories("/root/packages/user/sketches") == QStringList{"sketches"});
}

TEST_CASE("Only .scenario files are snippets", "[snippets]")
{
  const auto entry = Scenario::snippetEntry("/root/packages/user/snippets/Drums/Kick.scenario");
  REQUIRE(entry);
  CHECK(entry->rootKey == Library::snippetKey());
  CHECK(entry->categoryPath == QStringList{"Drums"});
  CHECK(entry->node.data.key == Library::snippetKey());
  CHECK(entry->node.data.prettyName == "Kick");
  CHECK(entry->node.data.customData == "/root/packages/user/snippets/Drums/Kick.scenario");

  CHECK_FALSE(Scenario::snippetEntry("/root/packages/user/cues/Messages.cues"));
  CHECK_FALSE(Scenario::snippetEntry("/root/packages/user/snippets/Song.score"));
}

TEST_CASE("Saving a snippet writes it below the snippets folder", "[snippets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QByteArray json = R"({"Intervals":[],"States":[],"Cables":[]})";

    const auto file = Scenario::writeSnippet(json, dir.path(), "Drums/Kick", nullptr);
    REQUIRE(file);
    CHECK(*file == dir.path() + "/snippets/Drums/Kick.scenario");
    QFile f{*file};
    REQUIRE(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == json);

    // No way out of the snippets folder, and no empty name.
    const auto escaped = Scenario::writeSnippet(json, dir.path(), "../../Out", nullptr);
    if(escaped)
      CHECK(escaped->startsWith(dir.path() + "/snippets/"));
    CHECK_FALSE(Scenario::writeSnippet(json, dir.path(), "  ", nullptr));
  });
}
