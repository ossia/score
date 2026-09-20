// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OpenDocumentsFile.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace score
{
QString OpenDocumentsFile::path()
{
  static const QString path = []() -> QString {
    static const auto username = []() -> QString {
      auto username = qEnvironmentVariable("USER");
      if(username.isEmpty())
        username = qEnvironmentVariable("USERNAME");
      return username;
    }();
    // Keyed on the application name, not the literal "score", so the two
    // SCORE_CUSTOM_APP_* variables that already separate one user's settings
    // from another's separate their crash-recovery lists too. Default is
    // "score", so the path is unchanged for a normal run.
    auto app = QCoreApplication::applicationName();
    if(app.isEmpty())
      app = QStringLiteral("score");

    const auto paths = QStandardPaths::standardLocations(QStandardPaths::TempLocation);
    return paths.first() + "/" + app + "_open_docs." + username;
  }();

  return path;
}

bool OpenDocumentsFile::exists()
{
#if defined(__EMSCRIPTEN__)
  // There is no file: the list of open documents lives in local storage.
  return !QSettings{}.value("score-backup/docs").toMap().isEmpty();
#else
  return QFile::exists(OpenDocumentsFile::path());
#endif
}
}
