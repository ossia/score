// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OpenDocumentsFile.hpp"

#include <QFile>
#include <QStandardPaths>

namespace score
{
QString OpenDocumentsFile::path()
{
  static QString path = [] () -> QString {
    auto paths
        = QStandardPaths::standardLocations(QStandardPaths::TempLocation);
    return paths.first() + "/score_open_docs";
  }();

  return path;
}

bool OpenDocumentsFile::exists()
{
  return QFile::exists(OpenDocumentsFile::path());
}
}
