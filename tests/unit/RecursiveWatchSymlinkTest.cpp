// for_all_files follows directory links (symlinks, and junctions on Windows) out
// of the walked folder, as the library does when packages are linked in from
// elsewhere, e.g. a network share. A link leading back up the tree is walked
// once, not forever. Paths go through non-ASCII names both ways: the root is
// handed in as UTF-8, and every path given back is UTF-8.

#include <score/tools/RecursiveWatch.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace
{
std::filesystem::path toPath(const QString& p)
{
  return std::filesystem::path{p.toStdU16String()};
}

bool makeDirLink(const QString& target, const QString& link)
{
  std::error_code ec;
  std::filesystem::create_directory_symlink(toPath(target), toPath(link), ec);
  if(!ec)
    return true;
#if defined(_WIN32)
  // A symlink needs Developer Mode or elevation, a junction does not, and the
  // walk sees both the same way.
  return QProcess::execute(
             "cmd", {"/c", "mklink", "/J", QDir::toNativeSeparators(link),
                     QDir::toNativeSeparators(target)})
         == 0;
#else
  return false;
#endif
}

void touch(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
}

QStringList walk(const QString& root)
{
  QStringList res;
  score::for_all_files(root.toStdString(), [&](std::string_view p) {
    res.push_back(QString::fromUtf8(p.data(), p.size()));
  });
  return res;
}

bool hasSuffix(const QStringList& paths, const QString& suffix)
{
  for(const auto& p : paths)
    if(p.endsWith(suffix))
      return true;
  return false;
}
}

TEST_CASE("for_all_files follows a directory link out of the root", "[recursive_watch]")
{
  QTemporaryDir outside;
  QTemporaryDir root;
  REQUIRE(outside.isValid());
  REQUIRE(root.isValid());

  const QString target = outside.filePath(QStringLiteral("modèles"));
  REQUIRE(QDir{}.mkpath(target + "/sub"));
  touch(target + "/a.scp");
  touch(target + "/sub/b.scp");

  // Directly in root: on Windows llfio names these differently from deeper ones.
  touch(root.filePath(QStringLiteral("racine-é.scp")));

  if(!makeDirLink(target, root.filePath(QStringLiteral("préréglages"))))
    SKIP("cannot create a directory link here");

  const auto paths = walk(root.path());
  INFO(paths.join('\n').toStdString());
  CHECK(paths.size() == 3);
  CHECK(hasSuffix(paths, QStringLiteral("/racine-é.scp")));
  CHECK(hasSuffix(paths, QStringLiteral("/a.scp")));
  CHECK(hasSuffix(paths, QStringLiteral("/sub/b.scp")));
  for(const auto& p : paths)
  {
    INFO(p.toStdString());
    CHECK(QFileInfo::exists(p));
  }
}

TEST_CASE("for_all_files walks a link back up the tree only once", "[recursive_watch]")
{
  QTemporaryDir root;
  REQUIRE(root.isValid());

  REQUIRE(QDir{}.mkpath(root.filePath("d")));
  touch(root.filePath("d/f.scp"));

  if(!makeDirLink(root.path(), root.filePath("d/up")))
    SKIP("cannot create a directory link here");

  const auto paths = walk(root.path());
  INFO(paths.join('\n').toStdString());
  CHECK(paths.size() == 1);
  CHECK(hasSuffix(paths, QStringLiteral("/d/f.scp")));
  for(const auto& p : paths)
    CHECK(QFileInfo::exists(p));
}
