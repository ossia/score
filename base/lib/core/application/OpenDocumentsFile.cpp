#include <QFile>
#include <QStandardPaths>
#include <QStringList>

#include "OpenDocumentsFile.hpp"

namespace iscore
{
QString OpenDocumentsFile::path()
{
    static QString path = [] {
        auto paths = QStandardPaths::standardLocations(QStandardPaths::TempLocation);
        return paths.first() + "/iscore_open_docs";
    }();

    return path;
}

bool OpenDocumentsFile::exists()
{
    return QFile::exists(OpenDocumentsFile::path());
}
}
