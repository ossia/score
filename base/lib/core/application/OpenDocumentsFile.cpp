#include "OpenDocumentsFile.hpp"
#include <QStandardPaths>
#include <QFile>

static const QString filePath = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first() + "/iscore_open_docs";


namespace iscore
{
bool openDocumentsFileExists()
{
    return QFile::exists(filePath);
}


QString openDocumentsFilePath()
{
    return filePath;
}
}

