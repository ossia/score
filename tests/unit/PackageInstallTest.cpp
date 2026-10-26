// Where a downloaded package ends up. Archives are as often as not wrapped in
// a single folder named after the repository they come from, and the install
// path has to look the same either way -- over a previous install, and with
// hidden entries which no directory listing shows by default.
//
// No download: the functions take the paths the extractor wrote, so a handful
// of files in a temporary folder is a whole archive as far as they know.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <PackageManager/Install.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace PM;

namespace
{
struct Fixture
{
  QTemporaryDir tmp;
  QString packages = tmp.path();
  QString incoming = packages + "/.incoming-companion-modules";
  QString destination = packages + "/companion-modules";

  //! Emulates an extraction: creates the files and returns them like zdl does
  std::vector<QString> extract(const QStringList& entries)
  {
    std::vector<QString> res;
    for(const QString& entry : entries)
    {
      const QString path = incoming + '/' + entry;
      REQUIRE(QDir{}.mkpath(QFileInfo{path}.absolutePath()));

      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(entry.toUtf8());
      res.push_back(path);
    }
    return res;
  }

  QStringList installed() const
  {
    auto entries = QDir{destination}.entryList(
        QDir::Dirs | QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    entries.sort();
    return entries;
  }
};

bool contentsAre(const QString& path, const QByteArray& expected)
{
  QFile f{path};
  return f.open(QIODevice::ReadOnly) && f.readAll() == expected;
}
}

TEST_CASE("wrapped archive loses its wrapper", "[packagemanager]")
{
  Fixture fx;
  const auto res = fx.extract(
      {"bitfocus-modules-data/addon.json", "bitfocus-modules-data/package.json",
       "bitfocus-modules-data/node-runtime/bin/node",
       "bitfocus-modules-data/companion-bundled-modules/generic/main.js"});

  REQUIRE(archiveRootFolder(fx.incoming, res) == "bitfocus-modules-data");

  QString err;
  REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));
  REQUIRE(err.isEmpty());

  REQUIRE(
      fx.installed()
      == QStringList{
          "addon.json", "companion-bundled-modules", "node-runtime", "package.json"});
  REQUIRE(
      contentsAre(fx.destination + "/addon.json", "bitfocus-modules-data/addon.json"));
  REQUIRE(QFileInfo::exists(fx.destination + "/node-runtime/bin/node"));
  REQUIRE(!QFileInfo::exists(fx.destination + "/bitfocus-modules-data"));
  REQUIRE(!QFileInfo::exists(fx.incoming));
}

TEST_CASE("flat archive is installed as-is", "[packagemanager]")
{
  Fixture fx;
  const auto res = fx.extract({"addon.json", "lib/thing.js"});

  REQUIRE(archiveRootFolder(fx.incoming, res).isEmpty());

  QString err;
  REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));

  REQUIRE(fx.installed() == QStringList{"addon.json", "lib"});
  REQUIRE(QFileInfo::exists(fx.destination + "/lib/thing.js"));
  REQUIRE(!QFileInfo::exists(fx.incoming));
}

TEST_CASE("an archive with several roots is not unwrapped", "[packagemanager]")
{
  Fixture fx;
  const auto res = fx.extract({"data/addon.json", "docs/readme.md"});

  REQUIRE(archiveRootFolder(fx.incoming, res).isEmpty());

  QString err;
  REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));

  REQUIRE(fx.installed() == QStringList{"data", "docs"});
}

TEST_CASE("a single top-level file is not a wrapper", "[packagemanager]")
{
  Fixture fx;
  const auto res = fx.extract({"addon.json"});

  REQUIRE(archiveRootFolder(fx.incoming, res).isEmpty());

  QString err;
  REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));

  REQUIRE(fx.installed() == QStringList{"addon.json"});
}

TEST_CASE("hidden entries do not change the layout", "[packagemanager]")
{
  Fixture fx;

  SECTION("inside the wrapper")
  {
    const auto res = fx.extract(
        {"depth-camera/.hidden", "depth-camera/addon.json", "depth-camera/.git/config"});

    REQUIRE(archiveRootFolder(fx.incoming, res) == "depth-camera");

    QString err;
    REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));

    REQUIRE(fx.installed() == QStringList{".git", ".hidden", "addon.json"});
  }

  SECTION("beside the wrapper")
  {
    const auto res = fx.extract({".DS_Store", "depth-camera/addon.json"});

    REQUIRE(archiveRootFolder(fx.incoming, res).isEmpty());

    QString err;
    REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));

    REQUIRE(fx.installed() == QStringList{".DS_Store", "depth-camera"});
  }
}

TEST_CASE("a previous install is replaced, leftovers and all", "[packagemanager]")
{
  Fixture fx;

  REQUIRE(QDir{}.mkpath(fx.destination + "/stale-dir"));
  {
    QFile f{fx.destination + "/stale.json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("stale");
  }
  {
    QFile f{fx.destination + "/.stale-hidden"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("stale");
  }

  const auto res
      = fx.extract({"depth-camera/addon.json", "depth-camera/model/model.onnx"});

  QString err;
  REQUIRE(moveExtractedPackage(fx.incoming, res, fx.destination, err));

  REQUIRE(fx.installed() == QStringList{"addon.json", "model"});
  REQUIRE(!QFileInfo::exists(fx.destination + ".old"));
  REQUIRE(!QFileInfo::exists(fx.incoming));
}

TEST_CASE("a failed install destroys nothing", "[packagemanager]")
{
  Fixture fx;

  REQUIRE(QDir{}.mkpath(fx.destination));
  {
    QFile f{fx.destination + "/addon.json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("previous");
  }

  QString err;
  REQUIRE(!moveExtractedPackage(fx.incoming, {}, fx.destination, err));
  REQUIRE(!err.isEmpty());

  REQUIRE(contentsAre(fx.destination + "/addon.json", "previous"));
}

namespace
{
QString writePackage(const QString& root, const QString& name, const QString& key, int version)
{
  const QString path = QFileInfo{root + '/' + name}.absoluteFilePath();
  REQUIRE(QDir{}.mkpath(path));
  QFile f{path + "/package.json"};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(QJsonDocument{QJsonObject{{"key", key}, {"version", version}}}.toJson());
  return path;
}
}

// An update installed under a new folder name: the CUDA runtime went from
// support/cuda-runtime-129 to support/cuda-runtime with the same key.
TEST_CASE("an update removes the previous install from its other folder", "[packagemanager]")
{
  QTemporaryDir tmp;
  const QString packages = tmp.path() + "/packages";
  const QString support = tmp.path() + "/support";
  const QString key = "28d97cbc-bc84-4e74-9bc2-fe0f0fccf87c";

  const QString old = writePackage(support, "cuda-runtime-129", key, 1);
  const QString misplaced = writePackage(packages, "cuda-runtime-old", key, 1);
  const QString other = writePackage(support, "tensorrt-runtime", "6d0f3c0a-6b55-4f0c-9a59-3b6cf4d5b1e2", 1);
  const QString fresh = writePackage(support, "cuda-runtime", key, 2);

  CHECK(removeOtherInstalls(QUuid::fromString(key), fresh, {packages, support}) == 2);
  CHECK(!QFileInfo::exists(old));
  CHECK(!QFileInfo::exists(misplaced));
  CHECK(QFileInfo::exists(fresh));
  CHECK(QFileInfo::exists(other));

  // Nothing left to remove
  CHECK(removeOtherInstalls(QUuid::fromString(key), fresh, {packages, support}) == 0);
}

#if !defined(_WIN32)
// packages/foo-dev -> ~/src/foo, a package under development with the key of
// the one installed from the store: neither the link nor what it points to go.
TEST_CASE("an update leaves a package folder linked by hand alone", "[packagemanager]")
{
  QTemporaryDir tmp;
  const QString packages = tmp.path() + "/packages";
  const QString key = "28d97cbc-bc84-4e74-9bc2-fe0f0fccf87c";

  const QString sources = writePackage(tmp.path() + "/src", "foo", key, 1);
  const QString linked = packages + "/foo-dev";
  REQUIRE(QDir{}.mkpath(packages));
  REQUIRE(QFile::link(sources, linked));
  const QString fresh = writePackage(packages, "foo", key, 2);

  CHECK(removeOtherInstalls(QUuid::fromString(key), fresh, {packages}) == 0);
  CHECK(QFileInfo{linked}.isSymLink());
  CHECK(QFileInfo::exists(sources + "/package.json"));
}
#endif
