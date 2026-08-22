// The document-free score::locateFilePath(const QString&) overload added by
// c976ba6c98.
//
// tests/unit/ProjectFilesTest.cpp tests a DIFFERENT function of the same name
// -- locateFilePath(name, PathRoots) from ProjectFiles.hpp. This one takes no
// roots at all: it resolves <LIBRARY>: out of QSettings and is what
// Score.readFile reaches when no document is open.

#include <score/tools/FilePath.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
//! Catch2 has no QString stringifier, and "{?} == {?}" says nothing about
//! which path came out wrong.
std::string located(const QString& in)
{
  return score::locateFilePath(in).toStdString();
}

std::string str(const QString& s)
{
  return s.toStdString();
}

struct hermetic_settings
{
  QTemporaryDir dir;

  hermetic_settings()
  {
    REQUIRE(dir.isValid());
    QCoreApplication::setOrganizationName("ossia");
    QCoreApplication::setOrganizationDomain("ossia.io");
    QCoreApplication::setApplicationName("score-test-locatefilepath");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QSettings::setDefaultFormat(QSettings::IniFormat);
  }

  void setRoot(const QString& root)
  {
    QSettings s;
    s.setValue("Library/RootPath", root);
    s.sync();
  }
};
}

TEST_CASE("<LIBRARY>: resolves against the library root", "[unit][filepath]")
{
  hermetic_settings cfg;
  QTemporaryDir lib;
  REQUIRE(lib.isValid());
  cfg.setRoot(lib.path());

  CHECK(located("<LIBRARY>:/Presets/x.fs") == str(lib.path() + "/Presets/x.fs"));

  // Resolution does not depend on the file existing: this is path math, and
  // the caller reports the failure.
  CHECK(located("<LIBRARY>:/nope.js") == str(lib.path() + "/nope.js"));
}

TEST_CASE("a path with no prefix is handed back untouched", "[unit][filepath]")
{
  hermetic_settings cfg;
  cfg.setRoot(QStringLiteral("/some/library"));

  CHECK(located("/absolute/x.fs") == "/absolute/x.fs");
  // Not made absolute either -- the overload only knows about <LIBRARY>:.
  CHECK(located("relative/x.fs") == "relative/x.fs");
  CHECK(located(QString{}).empty());
}

TEST_CASE("the other prefixes need a document and are left alone", "[unit][filepath]")
{
  hermetic_settings cfg;
  cfg.setRoot(QStringLiteral("/some/library"));

  // <PROJECT>: is resolved by the DocumentContext overload; without one there
  // is nothing to resolve it against, so it must survive unchanged rather than
  // be silently mangled.
  CHECK(located("<PROJECT>:/x.fs") == "<PROJECT>:/x.fs");
}

TEST_CASE("an unset library root leaves an absolute remainder", "[unit][filepath]")
{
  hermetic_settings cfg;
  {
    QSettings s;
    s.remove("Library/RootPath");
    s.sync();
  }

  CHECK(located("<LIBRARY>:/x.fs") == "/x.fs");
}
