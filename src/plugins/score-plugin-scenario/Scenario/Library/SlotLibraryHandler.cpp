#include <Library/FileSystemModel.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>

#include <QMimeData>

#include <Library/ProcessWidget.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <score/tools/File.hpp>

namespace Scenario
{

bool SlotLibraryHandler::onDrop(
    const QMimeData& mime,
    int row,
    int column,
    const QDir& parent)
{
  if (!mime.hasFormat(score::mime::layerdata()))
    return false;
  if (mime.hasFormat(score::mime::processpreset()))
    return false; // it is handled in Library::PresetLibraryHandler{}

  auto json = readJson(mime.data(score::mime::layerdata()));
  if (!json.HasMember("Path") || !json.HasMember("Duration"))
    return false;

  auto basename
      = JsonValue{json["Process"]["Metadata"]["ScriptingName"]}.toString();
  if (basename.isEmpty())
    basename = "Process";

  const QString filename = score::addUniqueSuffix(
                       parent.absolutePath() + "/" + basename + ".layer");

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
    const QMimeData& mime,
    int row,
    int column,
    const QDir& parent)
{
  if (mime.hasFormat(score::mime::scenariodata()))
  {
    auto obj = readJson(mime.data(score::mime::scenariodata()));
    const auto& states = obj["States"].GetArray();
    if (states.Size() == 1 && obj["Intervals"].GetArray().Size() == 0)
    {
      const auto& state = states[0];

      // Go from a tree to a list
      const State::MessageList& flattened
          = flatten(JsonValue{state["Messages"]}.to<Process::MessageNode>());

      auto basename = JsonValue{state["Metadata"]["ScriptingName"]}.toString();
      QString filename = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".cues");
      if (QFile f(filename); f.open(QIODevice::WriteOnly))
      {
        f.write(score::marshall<JSONObject>(flattened).toByteArray());
      }
    }
    else
    {
      auto basename = "Scenario";
      QString filename = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".scenario");

      if (QFile f(filename); f.open(QIODevice::WriteOnly))
      {
        f.write(mime.data(score::mime::scenariodata()));
      }
    }

    return true;
  }
  else if (mime.hasFormat(score::mime::messagelist()))
  {
    QString filename = score::addUniqueSuffix(parent.absolutePath() + "/Messages.cues");
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
