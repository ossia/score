// The order a folder's files are indexed in, which files are listed, and how a
// number sent to a folder combobox picks one: what the sound set and the
// combobox both rely on to mean the same file by the same number.

#include <score/tools/FolderFiles.hpp>
#include <score/tools/NaturalSort.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <halp/folder_combobox.hpp>

#include <limits>
#include <random>

TEST_CASE("Natural order compares digit runs by value", "[tools][natural-sort]")
{
  using score::naturalLess;
  CHECK(naturalLess("a2", "a10"));
  CHECK(!naturalLess("a10", "a2"));
  CHECK(naturalLess("kick 9.wav", "kick 10.wav"));
  // Case does not order, but still gives a strict order
  CHECK(naturalLess("Snare.wav", "tom.wav"));
  CHECK(naturalLess("a.wav", "B.wav"));
  CHECK(naturalLess("A.wav", "a.wav") != naturalLess("a.wav", "A.wav"));
  // Leading zeros: equal values, still strictly ordered
  CHECK(naturalLess("a01", "a1") != naturalLess("a1", "a01"));
  CHECK(naturalLess("a1", "a1x"));
  CHECK(!naturalLess("a1", "a1"));

  QStringList files{"x10", "x9", "x100", "x1", "X2", "x02", "y", "x"};
  score::sortNaturally(files);
  // X2 and x02 are both 2: what tells them apart is their text
  CHECK(files == QStringList{"x", "x1", "X2", "x02", "x9", "x10", "x100", "y"});
}

TEST_CASE("Natural order reads 0 to 9 only as digits", "[tools][natural-sort]")
{
  using score::naturalLess;
  // Fullwidth digits are text: their code points order them, never their
  // value against ASCII digits
  const QString fullwidth9 = QStringLiteral("a９");
  const QString fullwidth1 = QStringLiteral("a１");
  CHECK(naturalLess("a10", fullwidth9));
  CHECK(naturalLess("a10", fullwidth1));
  CHECK(naturalLess(fullwidth1, fullwidth9));
}

TEST_CASE("Natural order is a strict weak order", "[tools][natural-sort]")
{
  std::mt19937 rng{1234};
  const QString alphabet = QStringLiteral("0019aAb .");
  std::uniform_int_distribution<int> len{0, 5}, ch{0, int(alphabet.size()) - 1};
  QStringList words;
  for(int i = 0; i < 200; i++)
  {
    QString w;
    for(int n = len(rng); n > 0; n--)
      w += alphabet[ch(rng)];
    words.push_back(w);
  }
  for(const auto& a : words)
  {
    CHECK(!score::naturalLess(a, a));
    for(const auto& b : words)
    {
      if(score::naturalLess(a, b))
        CHECK(!score::naturalLess(b, a));
      if(a != b)
        CHECK(score::naturalLess(a, b) != score::naturalLess(b, a));
    }
  }
}

TEST_CASE("A folder's files: readable ones, filtered, in natural order", "[tools][folder]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QDir root{dir.path()};
  const auto touch = [&](const QString& name) {
    QFile f{root.filePath(name)};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("x");
  };
  for(auto name : {"s10.wav", "s9.wav", "s1.WAV", "notes.txt", "s2.aif"})
    touch(name);
  REQUIRE(root.mkdir("sub"));
  touch("sub/s0.wav");

  CHECK(score::folderFiles({}, {}).isEmpty());
  CHECK(
      score::folderFiles(dir.path(), {"*.wav", "*.aif"})
      == QStringList{"s1.WAV", "s2.aif", "s9.wav", "s10.wav"});
  CHECK(
      score::folderFiles(dir.path(), {})
      == QStringList{"notes.txt", "s1.WAV", "s2.aif", "s9.wav", "s10.wav"});
  CHECK(
      score::folderFiles(dir.path(), {"*.wav"}, true)
      == QStringList{"s1.WAV", "s9.wav", "s10.wav", "sub/s0.wav"});

  // A file that cannot be read is not listed: an index past it would pick
  // another file than the one shown
  QFile unreadable{root.filePath("s5.wav")};
  REQUIRE(unreadable.open(QIODevice::WriteOnly));
  unreadable.close();
  REQUIRE(unreadable.setPermissions(QFileDevice::Permissions{}));
  if(!QFileInfo{unreadable.fileName()}.isReadable())
    CHECK(
        score::folderFiles(dir.path(), {"*.wav"})
        == QStringList{"s1.WAV", "s9.wav", "s10.wav"});
  unreadable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

TEST_CASE("A folder combobox's number is an index into its files", "[halp][folder]")
{
  using halp::folder_index;
  constexpr int largest = std::numeric_limits<int>::max();
  CHECK(folder_index("0") == 0);
  CHECK(folder_index("3") == 3);
  CHECK(folder_index("3.7") == 3);
  CHECK(folder_index("1e3") == 1000);
  CHECK(folder_index("-0") == 0);
  CHECK(folder_index("1e20") == largest);
  CHECK(folder_index("inf") == largest);
  // Names
  CHECK(folder_index("") == std::nullopt);
  CHECK(folder_index("kick.wav") == std::nullopt);
  CHECK(folder_index("3.wav") == std::nullopt);
  CHECK(folder_index("3 ") == std::nullopt);
  CHECK(folder_index("+3") == std::nullopt);
  CHECK(folder_index("-1") == std::nullopt);
  CHECK(folder_index("-inf") == std::nullopt);
  CHECK(folder_index("nan") == std::nullopt);
  CHECK(folder_index("-") == std::nullopt);
}
