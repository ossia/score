// The preset tree the library shows and the preset button's Forward / Back
// walk: folders from the categories, in one order for both.

#include <Process/PresetTree.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const auto key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("49047204-5c1e-4b54-9f43-2b583f664b2e"));
const auto other = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"));

Process::Preset preset(
    const QString& name, const QString& category, UuidKey<Process::ProcessModel> k = key,
    const QString& effect = {})
{
  Process::Preset p;
  p.name = name;
  p.category = category;
  p.key = {k, effect};
  return p;
}

QStringList names(const std::vector<const Process::Preset*>& v)
{
  QStringList out;
  for(auto* p : v)
    out.push_back(p->name);
  return out;
}

QStringList folders(const Process::PresetTree::Node& n)
{
  QStringList out;
  for(auto& e : n.entries)
    if(e.child >= 0)
      out.push_back(n.children[std::size_t(e.child)]->name);
  return out;
}
}

TEST_CASE("a category is a path of folders", "[presets]")
{
  CHECK(Process::presetCategoryPath("") == QStringList{});
  CHECK(Process::presetCategoryPath(" A / B ") == (QStringList{"A", "B"}));
  CHECK(Process::presetCategoryPath("A//B/") == (QStringList{"A", "B"}));
  CHECK(Process::presetCategoryPath(" / ") == QStringList{});
}

TEST_CASE("presets are in folders, a folder's own first, then by name", "[presets]")
{
  const std::vector<Process::Preset> library{
      preset("z", "Z"),   preset("b", "A/B"), preset("a2", "A"),  preset("root", ""),
      preset("c", "A-C"), preset("b2", " A / B "), preset("a1", "A"),
      preset("not mine", "A", other)};
  const Process::PresetTree tree{Process::ProcessIdentifier{key, {}}, library};

  // The root's presets, then its folders by name
  CHECK(folders(tree.root) == (QStringList{"A", "A-C", "Z"}));
  REQUIRE(tree.root.entries.front().preset);
  CHECK(tree.root.entries.front().preset->name == "root");

  // " A / B " and "A/B" are one folder, inside A, after A's own presets
  const auto& a = *tree.root.children[0];
  CHECK(folders(a) == QStringList{"B"});
  CHECK(a.entries[0].preset->name == "a1");
  CHECK(a.entries[1].preset->name == "a2");
  CHECK(a.entries[2].child == 0);

  CHECK(
      names(tree.ordered())
      == (QStringList{"root", "a1", "a2", "b", "b2", "c", "z"}));
}

TEST_CASE("a script's presets are those of its script", "[presets]")
{
  const std::vector<Process::Preset> library{
      preset("one", "", key, "a.js"), preset("two", "", key, "b.js"),
      preset("any", "", key, "")};
  CHECK(
      names(Process::PresetTree{Process::ProcessIdentifier{key, "a.js"}, library}.ordered())
      == QStringList{"one"});
  // No script given: all of the kind
  CHECK(
      names(Process::PresetTree{Process::ProcessIdentifier{key, {}}, library}.ordered())
      == (QStringList{"any", "one", "two"}));
}

TEST_CASE("a plug-in's own presets come last, in a folder of their own", "[presets]")
{
  const std::vector<Process::Preset> library{
      preset("lib", ""), preset("same", Process::PresetTree::builtinFolderName())};
  const std::vector<Process::Preset> builtins{
      preset("Zeta", "Bass"), preset("Alpha", ""), preset("Mid", "Bass")};
  const Process::PresetTree tree{Process::ProcessIdentifier{key, {}}, library, builtins};

  // Not merged with a library category of the same name
  const auto name = Process::PresetTree::builtinFolderName();
  CHECK(folders(tree.root) == (QStringList{name, name}));
  const auto& own = *tree.root.children.back();
  CHECK(folders(own) == QStringList{"Bass"});
  // In the plug-in's order within a category, not sorted by name
  CHECK(
      names(tree.ordered()) == (QStringList{"lib", "same", "Alpha", "Zeta", "Mid"}));
}
