// Unit test: Process::ComboBox's runtime-populated item list.
//
// A combobox port's items used to be fixed once, at construction. They can now
// be replaced at run time -- by an avnd object calling update_items, or by
// listing a sibling folder port. That makes three things worth pinning:
//  * what happens to the selected value when the list changes under it,
//  * that a folder listing filters, sorts and degrades predictably,
//  * that a document written before any of this existed still loads.

#include <score_test/App.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
using Alts = std::vector<std::pair<QString, ossia::value>>;

Alts alts(std::initializer_list<const char*> names)
{
  Alts a;
  for(auto* n : names)
    a.emplace_back(QString::fromUtf8(n), std::string(n));
  return a;
}

Process::ComboBox make_combo(QObject& parent, Alts a, const char* init)
{
  return Process::ComboBox{
      std::move(a), ossia::value{std::string(init)}, "combo", Id<Process::Port>{0},
      &parent};
}

void touch(const QString& dir, const char* name)
{
  QFile f{dir + "/" + QString::fromUtf8(name)};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write("x");
}
}

TEST_CASE("Replacing the items keeps a selection that is still there", "[combobox]")
{
  QObject parent;
  auto combo = make_combo(parent, alts({"a", "b", "c"}), "a");
  combo.setValue(ossia::value{std::string("b")});

  int changed = 0;
  QObject::connect(
      &combo, &Process::ComboBox::alternativesChanged, &parent, [&] { changed++; });

  combo.setAlternatives(alts({"b", "c", "d"}));
  CHECK(changed == 1);
  CHECK(combo.value() == ossia::value{std::string("b")});
  CHECK(combo.count() == 3);
}

TEST_CASE("A selection that disappears falls back to the init value", "[combobox]")
{
  QObject parent;
  auto combo = make_combo(parent, alts({"a", "b", "c"}), "a");
  combo.setValue(ossia::value{std::string("c")});

  combo.setAlternatives(alts({"a", "b"}));
  CHECK(combo.value() == ossia::value{std::string("a")});
}

// Folder-backed comboboxes name a file that an upstream process may not have
// written yet: such a selection must survive a refresh rather than snap away.
TEST_CASE("A selection absent from the list and from init is left alone", "[combobox]")
{
  QObject parent;
  auto combo = make_combo(parent, alts({"a", "b"}), "zzz");
  combo.setValue(ossia::value{std::string("not-yet.wav")});

  combo.setAlternatives(alts({"a", "b"}));
  CHECK(combo.value() == ossia::value{std::string("not-yet.wav")});
}

TEST_CASE("Setting the same items again changes nothing and is silent", "[combobox]")
{
  QObject parent;
  auto combo = make_combo(parent, alts({"a", "b"}), "a");

  int changed = 0;
  QObject::connect(
      &combo, &Process::ComboBox::alternativesChanged, &parent, [&] { changed++; });

  combo.setAlternatives(alts({"a", "b"}));
  CHECK(changed == 0);
}

TEST_CASE("A folder listing is filtered, sorted, and never empty", "[combobox][folder]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  touch(dir.path(), "b.wav");
  touch(dir.path(), "a.wav");
  touch(dir.path(), "notes.txt");

  QObject parent;
  auto combo = make_combo(parent, alts({"-"}), "-");

  SECTION("no filter lists everything, sorted by name")
  {
    combo.repopulateFromFolder(dir.path());
    REQUIRE(combo.count() == 3);
    CHECK(combo.getValues()[0].first == "a.wav");
    CHECK(combo.getValues()[1].first == "b.wav");
    CHECK(combo.getValues()[2].first == "notes.txt");
  }

  SECTION("an extension filter keeps only what matches")
  {
    combo.fileExtensions = QStringList{"*.wav"};
    combo.repopulateFromFolder(dir.path());
    REQUIRE(combo.count() == 2);
    CHECK(combo.getValues()[0].first == "a.wav");
    CHECK(combo.getValues()[1].first == "b.wav");
  }

  SECTION("the value of an item is the bare file name")
  {
    combo.repopulateFromFolder(dir.path());
    CHECK(combo.getValues()[0].second == ossia::value{std::string("a.wav")});
  }

  SECTION("an empty or missing folder degrades to a single placeholder")
  {
    QTemporaryDir empty;
    combo.repopulateFromFolder(empty.path());
    REQUIRE(combo.count() == 1);
    CHECK(combo.getValues()[0].first == "-");

    combo.repopulateFromFolder("/does/not/exist");
    REQUIRE(combo.count() == 1);
    CHECK(combo.getValues()[0].first == "-");

    combo.repopulateFromFolder({});
    REQUIRE(combo.count() == 1);
  }
}

TEST_CASE("Folder-combobox metadata survives a JSON round-trip", "[combobox][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QObject parent;
    auto combo = make_combo(parent, alts({"a.wav"}), "a.wav");
    combo.folderPortName = "Folder";
    combo.fileExtensions = QStringList{"*.wav", "*.aif"};

    JSONReader r;
    r.readFrom(combo);
    const rapidjson::Document doc = toValue(r);
    JSONObject::Deserializer des{doc};
    Process::ComboBox reloaded{des, &parent};

    CHECK(reloaded.folderPortName == "Folder");
    CHECK(reloaded.fileExtensions == QStringList{"*.wav", "*.aif"});
    CHECK(reloaded.count() == 1);
  });
}

// A .score written before folder-comboboxes existed has no FolderPort key.
TEST_CASE("A JSON document without the new keys still loads", "[combobox][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QObject parent;
    auto combo = make_combo(parent, alts({"a", "b"}), "a");

    JSONReader r;
    r.readFrom(combo);
    rapidjson::Document doc = toValue(r);
    doc.RemoveMember("FolderPort");
    doc.RemoveMember("FileExtensions");

    JSONObject::Deserializer des{doc};
    Process::ComboBox reloaded{des, &parent};
    CHECK(reloaded.folderPortName.isEmpty());
    CHECK(reloaded.fileExtensions.isEmpty());
    CHECK(reloaded.count() == 2);
  });
}

// Note: the binary (.scorebin) path is deliberately not covered here. A port
// cannot be marshalled standalone through DataStream in this harness -- a plain
// ControlInlet round-trip aborts in checkDelimiter() on unmodified code too,
// because ports are written through the port factory's own framing. Covering it
// needs a document-level fixture.
