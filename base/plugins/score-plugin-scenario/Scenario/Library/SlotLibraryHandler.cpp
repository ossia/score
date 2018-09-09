#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <QMimeData>
#include <Library/JSONLibrary/FileSystemModel.hpp>
namespace Scenario
{

// Taken from https://stackoverflow.com/a/18073392/1495627
// Adds a unique suffix to a file name so no existing file has the same file
// name. Can be used to avoid overwriting existing files. Works for both
// files/directories, and both relative/absolute paths. The suffix is in the
// form - "path/to/file.tar.gz", "path/to/file (1).tar.gz",
// "path/to/file (2).tar.gz", etc.
QString addUniqueSuffix(const QString &fileName)
{
  // If the file doesn't exist return the same name.
  if (!QFile::exists(fileName)) {
    return fileName;
  }

  QFileInfo fileInfo(fileName);
  QString ret;

  // Split the file into 2 parts - dot+extension, and everything else. For
  // example, "path/file.tar.gz" becomes "path/file"+".tar.gz", while
  // "path/file" (note lack of extension) becomes "path/file"+"".
  QString secondPart = fileInfo.completeSuffix();
  QString firstPart;
  if (!secondPart.isEmpty()) {
    secondPart = "." + secondPart;
    firstPart = fileName.left(fileName.size() - secondPart.size());
  } else {
    firstPart = fileName;
  }

  // Try with an ever-increasing number suffix, until we've reached a file
  // that does not yet exist.
  for (int ii = 1; ; ii++) {
    // Construct the new file name by adding the unique number between the
    // first and second part.
    ret = QString("%1 (%2)%3").arg(firstPart).arg(ii).arg(secondPart);
    // If no file exists with the new name, return it.
    if (!QFile::exists(ret)) {
      return ret;
    }
  }
}

bool SlotLibraryHandler::onDrop(
    Library::FileSystemModel& model,
    const QMimeData& mime,
    int row, int column, const QModelIndex& parent)
{
  if(!mime.hasFormat(score::mime::layerdata()))
    return false;

  auto file = model.fileInfo(parent);

  auto json = QJsonDocument::fromJson(mime.data(score::mime::layerdata())).object();

  QString path = file.isDir() ? file.absoluteFilePath() : file.absolutePath();

  auto basename = json["Process"].toObject()["Metadata"].toObject()["ScriptingName"].toString();
  if(basename.isEmpty())
    basename = "Process";

  QString filename = addUniqueSuffix(path + "/" + basename + ".layer");

  QFile f(filename);
  if(f.open(QIODevice::WriteOnly))
  {
    json.remove("PID");
    f.write(QJsonDocument{json}.toJson());
  }

  return true;
}

QStringList SlotLibraryHandler::acceptedMimeTypes() const
{
  return {score::mime::layerdata()};

}

QStringList SlotLibraryHandler::acceptedFiles() const
{
  return {"*.layer"};
}
}
