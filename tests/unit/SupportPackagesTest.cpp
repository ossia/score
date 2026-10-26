// Which support package folders have their libraries loaded, and what happens
// to a package folder which cannot be removed.
//
// Two versions of a package may both be on disk: an update installed under a
// new folder name, or a previous version still in use when it was replaced.
// Their libraries usually share names (cudnn64_9.dll, libcudnn.so.9), so only
// the newest may be loaded.

#include <Library/SupportPackages.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#if !defined(_WIN32)
#include <unistd.h>
#endif

using namespace Library;

namespace
{
const QString cuda_key = QStringLiteral("28d97cbc-bc84-4e74-9bc2-fe0f0fccf87c");
const QString trt_key = QStringLiteral("6d0f3c0a-6b55-4f0c-9a59-3b6cf4d5b1e2");

//! A package folder as the package manager leaves it
QString makePackage(const QString& root, const QString& name, const QString& key, int version)
{
  const QString path = QFileInfo{root + '/' + name}.absoluteFilePath();
  REQUIRE(QDir{}.mkpath(path));
  QFile f{path + "/package.json"};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(QJsonDocument{QJsonObject{{"key", key}, {"version", version}, {"raw_name", name}}}
              .toJson());
  return path;
}
}

TEST_CASE("Only the newest install of a support package is loaded", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString support = tmp.path();

  makePackage(support, "cuda-runtime-129", cuda_key, 1);
  const QString cuda = makePackage(support, "cuda-runtime", cuda_key, 2);
  const QString trt = makePackage(support, "tensorrt-runtime", trt_key, 1);

  const QStringList folders = supportFoldersToLoad(support);
  CHECK(folders.size() == 2);
  CHECK(folders.contains(cuda));
  CHECK(folders.contains(trt));
}

TEST_CASE("Support folders without a package.json are loaded", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString manual = QFileInfo{tmp.path() + "/my-libs"}.absoluteFilePath();
  REQUIRE(QDir{}.mkpath(manual));
  REQUIRE(QDir{}.mkpath(tmp.path() + "/.incoming-cuda-runtime"));

  CHECK(supportFoldersToLoad(tmp.path()) == QStringList{manual});
}

TEST_CASE("Installed packages are read from their package.json", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  makePackage(tmp.path(), "cuda-runtime", cuda_key, 2);
  // An install in progress, and a folder without a key, are not packages.
  makePackage(tmp.path(), ".incoming-cuda-runtime", cuda_key, 3);
  REQUIRE(QDir{}.mkpath(tmp.path() + "/no-key"));

  const auto packages = installedPackages(tmp.path());
  REQUIRE(packages.size() == 1);
  CHECK(packages[0].key == QUuid::fromString(cuda_key));
  CHECK(packages[0].version == 2);
}

TEST_CASE("A package folder is removed", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString path = makePackage(tmp.path(), "cuda-runtime", cuda_key, 2);

  CHECK(removePackageFolder(path));
  CHECK(!QFileInfo::exists(path));
  CHECK(removePackageFolder(path)); // already gone is gone
}

#if !defined(_WIN32)
// A folder whose content cannot be removed stands for a library loaded on
// Windows: a subfolder without write permission holds a file which cannot go.
TEST_CASE("A package folder in use is removed at the next start", "[unit][library]")
{
  if(::geteuid() == 0)
    SKIP("Permissions do not stop root from removing the folder");

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString old = makePackage(tmp.path(), "cuda-runtime-129", cuda_key, 1);
  const QString cuda = makePackage(tmp.path(), "cuda-runtime", cuda_key, 2);

  const QString locked = old + "/lib";
  REQUIRE(QDir{}.mkpath(locked));
  {
    QFile f{locked + "/cudnn64_9.dll"};
    REQUIRE(f.open(QIODevice::WriteOnly));
  }
  REQUIRE(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));

  CHECK(!removePackageFolder(old));
  CHECK(isMarkedForRemoval(old));
  // Marked: not loaded, even though it would otherwise be a candidate
  CHECK(supportFoldersToLoad(tmp.path()) == QStringList{cuda});

  // "Next start": the library is not in use any more
  REQUIRE(QFile::setPermissions(
      locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
  removeMarkedPackageFolders(tmp.path());
  CHECK(!QFileInfo::exists(old));
  CHECK(QFileInfo::exists(cuda));
}
#endif

#if !defined(_WIN32)
TEST_CASE("Removing a package folder linked by hand removes the link only", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString sources = makePackage(tmp.path() + "/src", "foo", cuda_key, 1);
  const QString linked = tmp.path() + "/packages/foo-dev";
  REQUIRE(QDir{}.mkpath(tmp.path() + "/packages"));
  REQUIRE(QFile::link(sources, linked));

  CHECK(isLinkedPackageFolder(linked));
  CHECK(!isLinkedPackageFolder(sources));
  CHECK(removePackageFolder(linked));
  CHECK(!QFileInfo{linked}.isSymLink());
  CHECK(QFileInfo::exists(sources + "/package.json"));
  CHECK(!isMarkedForRemoval(sources));
}
#endif

TEST_CASE("A support package is loaded once per process", "[unit][library]")
{
  // The keys loaded are remembered for the whole process: keys of its own.
  const QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString other = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString first = makePackage(tmp.path(), "runtime", key, 1);

  CHECK(!isSupportPackageLoaded(QUuid::fromString(key)));
  CHECK(loadSupportPackages(tmp.path()).contains(first));
  CHECK(isSupportPackageLoaded(QUuid::fromString(key)));

  // Uninstalled, then installed again, or updated: not before a restart.
  REQUIRE(removePackageFolder(first));
  const QString second = makePackage(tmp.path(), "runtime-2", key, 2);
  // Installed since, another package is loaded; the loaded one again, to load
  // what may have been waiting for it.
  const QString third = makePackage(tmp.path(), "other", other, 1);
  const QStringList loaded = loadSupportPackages(tmp.path());
  CHECK(!loaded.contains(second));
  CHECK(loaded.contains(third));

  CHECK(loadSupportPackages(tmp.path()).contains(third));
}

TEST_CASE("What interrupted installs left is removed", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString root = tmp.path();

  auto make = [&](const QString& name, bool folder, bool old) {
    const QString path = root + '/' + name;
    if(folder)
      REQUIRE(QDir{}.mkpath(path + "/lib"));
    {
      QFile f{folder ? path + "/lib/libfoo.so" : path};
      REQUIRE(f.open(QIODevice::WriteOnly));
    }
    if(old)
    {
      const std::filesystem::path p{path.toStdU16String()};
      std::filesystem::last_write_time(
          p, std::filesystem::last_write_time(p) - std::chrono::hours{48});
    }
    return path;
  };

  const QString oldFolder = make(".incoming-cuda-runtime", true, true);
  const QString oldDownload = make(".zdl-download-abcdef.zip", false, true);
  // Another score installing right now
  const QString current = make(".incoming-tensorrt-runtime", true, false);
  const QString installed = makePackage(root, "cuda-runtime", cuda_key, 2);

  removeInterruptedInstalls(root);
  CHECK(!QFileInfo::exists(oldFolder));
  CHECK(!QFileInfo::exists(oldDownload));
  CHECK(QFileInfo::exists(current));
  CHECK(QFileInfo::exists(installed));
}

#if !defined(_WIN32)
// koaia bundles cuDNN in its lib folder as libcudnn_graph.so.9; the cuda-runtime
// package ships libcudnn_graph.so.9.27.0 and a libcudnn_graph.so.9 link to it.
TEST_CASE("Libraries the application bundles are not loaded from a package", "[unit][library]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString bundled = tmp.path() + "/app/lib";
  const QString pkg = makePackage(tmp.path() + "/support", "cuda-runtime", cuda_key, 2);
  REQUIRE(QDir{}.mkpath(bundled));

  auto touch = [](const QString& path) {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
  };
  touch(bundled + "/libcudnn_graph.so.9");
  touch(pkg + "/libcudnn_graph.so.9.27.0");
  REQUIRE(QFile::link("libcudnn_graph.so.9.27.0", pkg + "/libcudnn_graph.so.9"));
  touch(pkg + "/libcublas.so.13.4.3");
  REQUIRE(QFile::link("libcublas.so.13.4.3", pkg + "/libcublas.so.13"));

  CHECK(supportLibraryFiles({pkg}, bundled) == QStringList{pkg + "/libcublas.so.13.4.3"});
  CHECK(supportLibraryFiles({pkg}, tmp.path() + "/nothing").size() == 2);
}
#endif
