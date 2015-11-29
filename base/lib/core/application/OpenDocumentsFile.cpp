#include <qfile.h>
#include <qstandardpaths.h>
#include <qstringlist.h>

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
