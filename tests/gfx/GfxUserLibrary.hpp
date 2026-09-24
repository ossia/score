#pragma once

// Access to a score user library for the tests that exercise its presets.
//
// The rasterizer, compute and viewer presets those tests drive are not part of
// this repository, and neither are the headers they #include from their
// packages. A test run therefore only reaches them through
// SCORE_TEST_LIBRARY_ROOT, the root of a library (the directory holding
// packages/), and skips without it: the user's own library would make the
// result depend on whatever copy of the packages that machine has.

#include <Library/LibrarySettings.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <string>

namespace score::test::gfx::library
{

inline QString root()
{
  return qEnvironmentVariable("SCORE_TEST_LIBRARY_ROOT");
}

/// Absolute path of `relative` inside the first package of the library that
/// provides it (packages are searched in name order), or an empty string.
inline QString find(const QString& relative)
{
  const QString r = root();
  if(r.isEmpty())
    return {};
  const QDir packages{r + QStringLiteral("/packages")};
  for(const auto& pkg :
      packages.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
  {
    const QString candidate = packages.filePath(pkg + QLatin1Char('/') + relative);
    if(QFileInfo::exists(candidate))
      return QFileInfo{candidate}.absoluteFilePath();
  }
  return {};
}

/// Directory holding `file` in the library, e.g. the rasterizer presets as
/// dir_of("presets/rasterizers/classic_pbr_full.frag").
inline QString dir_of(const QString& relativeFile)
{
  const QString f = find(relativeFile);
  return f.isEmpty() ? QString{} : QFileInfo{f}.absolutePath();
}

inline std::string skip_reason(const QString& relative)
{
  if(root().isEmpty())
    return "SCORE_TEST_LIBRARY_ROOT is not set: no score library to take the "
           "presets from";
  return "no package under " + root().toStdString() + "/packages provides "
         + relative.toStdString();
}

/// Makes the test library the application's library, so that the shader
/// include search path resolves the presets' headers from its packages.
struct RootGuard
{
  Library::Settings::Model& lib;
  QString previous;

  explicit RootGuard(const score::GUIApplicationContext& ctx)
      : lib{ctx.settings<Library::Settings::Model>()}
      , previous{lib.getRootPath()}
  {
    lib.setRootPath(root());
  }
  RootGuard(const RootGuard&) = delete;
  RootGuard& operator=(const RootGuard&) = delete;
  ~RootGuard() { lib.setRootPath(previous); }
};

}
