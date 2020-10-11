#include <Library/FileSystemModel.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>

#include <QMimeData>
namespace Scenario
{

// Taken from https://stackoverflow.com/a/18073392/1495627
// Adds a unique suffix to a file name so no existing file has the same file
// name. Can be used to avoid overwriting existing files. Works for both
// files/directories, and both relative/absolute paths. The suffix is in the
// form - "path/to/file.tar.gz", "path/to/file (1).tar.gz",
// "path/to/file (2).tar.gz", etc.
QString addUniqueSuffix(const QString& fileName)
{
  // If the file doesn't exist return the same name.
  if (!QFile::exists(fileName))
  {
    return fileName;
  }

  QFileInfo fileInfo(fileName);
  QString ret;

  // Split the file into 2 parts - dot+extension, and everything else. For
  // example, "path/file.tar.gz" becomes "path/file"+".tar.gz", while
  // "path/file" (note lack of extension) becomes "path/file"+"".
  QString secondPart = fileInfo.completeSuffix();
  QString firstPart;
  if (!secondPart.isEmpty())
  {
    secondPart = "." + secondPart;
    firstPart = fileName.left(fileName.size() - secondPart.size());
  }
  else
  {
    firstPart = fileName;
  }

  // Try with an ever-increasing number suffix, until we've reached a file
  // that does not yet exist.
  for (int ii = 1;; ii++)
  {
    // Construct the new file name by adding the unique number between the
    // first and second part.
    ret = QString("%1 (%2)%3").arg(firstPart).arg(ii).arg(secondPart);
    // If no file exists with the new name, return it.
    if (!QFile::exists(ret))
    {
      return ret;
    }
  }
}

bool SlotLibraryHandler::onDrop(
    Library::FileSystemModel& model,
    const QMimeData& mime,
    int row,
    int column,
    const QModelIndex& parent)
{
  if (!mime.hasFormat(score::mime::layerdata()))
    return false;

  auto file = model.fileInfo(parent);

  auto json = readJson(mime.data(score::mime::layerdata()));
  if(!json.HasMember("Path") || !json.HasMember("Duration"))
    return false;

  QString path = file.isDir() ? file.absoluteFilePath() : file.absolutePath();

  auto basename = JsonValue{json["Process"]["Metadata"]["ScriptingName"]}.toString();
  if (basename.isEmpty())
    basename = "Process";

  QString filename = addUniqueSuffix(path + "/" + basename + ".layer");

  if (QFile f(filename); f.open(QIODevice::WriteOnly))
  {
    json.RemoveMember("PID");
    f.write(jsonToByteArray(json));
  }

  return true;
}

QSet<QString> SlotLibraryHandler::acceptedMimeTypes() const noexcept
{
  return {score::mime::layerdata()};
}

QSet<QString> SlotLibraryHandler::acceptedFiles() const noexcept
{
  return {"layer"};
}

bool ScenarioLibraryHandler::onDrop(
    Library::FileSystemModel& model,
    const QMimeData& mime,
    int row,
    int column,
    const QModelIndex& parent)
{
  if (mime.hasFormat(score::mime::scenariodata()))
  {
    auto file = model.fileInfo(parent);
    QString path = file.isDir() ? file.absoluteFilePath() : file.absolutePath();

    auto obj = readJson(mime.data(score::mime::scenariodata()));
    const auto& states = obj["States"].GetArray();
    if (states.Size() == 1 && obj["Intervals"].GetArray().Size() == 0)
    {
      const auto& state = states[0];

      // Go from a tree to a list
      const State::MessageList& flattened
          = flatten(JsonValue{state["Messages"]}.to<Process::MessageNode>());

      auto basename = JsonValue{state["Metadata"]["ScriptingName"]}.toString();
      QString filename = addUniqueSuffix(path + "/" + basename + ".cues");
      if (QFile f(filename); f.open(QIODevice::WriteOnly))
      {
        f.write(score::marshall<JSONObject>(flattened).toByteArray());
      }
    }
    else
    {
      auto basename = "Scenario";
      QString filename = addUniqueSuffix(path + "/" + basename + ".scenario");

      if (QFile f(filename); f.open(QIODevice::WriteOnly))
      {
        f.write(mime.data(score::mime::scenariodata()));
      }
    }

    return true;
  }
  else if (mime.hasFormat(score::mime::messagelist()))
  {
    auto file = model.fileInfo(parent);
    QString path = file.isDir() ? file.absoluteFilePath() : file.absolutePath();

    QString filename = addUniqueSuffix(path + "/Messages.cues");
    if (QFile f(filename); f.open(QIODevice::WriteOnly))
    {
      f.write(mime.data(score::mime::messagelist()));
    }
    return true;
  }
  return false;
}

QSet<QString> ScenarioLibraryHandler::acceptedMimeTypes() const noexcept
{
  return {score::mime::scenariodata(), score::mime::messagelist()};
}

QSet<QString> ScenarioLibraryHandler::acceptedFiles() const noexcept
{
  return {"scenario", "cues"};
}
}
