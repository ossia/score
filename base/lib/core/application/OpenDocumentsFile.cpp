#include <QFile>
#include <QStandardPaths>
#include <QStringList>

#include "OpenDocumentsFile.hpp"

static const QString filePath = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first() + "/iscore_open_docs";


namespace iscore
{
bool OpenDocumentsFile::exists()
{
    return QFile::exists(filePath);
}

QString OpenDocumentsFile::path()
{
    return filePath;
}
}
