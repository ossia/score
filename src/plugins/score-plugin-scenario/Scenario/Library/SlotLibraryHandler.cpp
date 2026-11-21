#include <State/MessageListSerialization.hpp>

#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>

#include <Library/FileSystemModel.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/Snippet.hpp>
#include <Library/ProcessWidget.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/tools/File.hpp>

#include <QMimeData>

#include <algorithm>

namespace Scenario
{

bool SlotLibraryHandler::onDrop(
    const QMimeData& mime, int row, int column, const QDir& parent)
{
  if(!mime.hasFormat(score::mime::layerdata()))
    return false;
  if(mime.hasFormat(score::mime::processpreset()))
    return false; // it is handled in Library::PresetLibraryHandler{}

  auto json = readJson(mime.data(score::mime::layerdata()));
  if(!json.HasMember("Path") || !json.HasMember("Duration"))
    return false;

  auto basename = JsonValue{json["Process"]["Metadata"]["ScriptingName"]}.toString();
  if(basename.isEmpty())
    basename = "Process";

  const QString filename
      = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".layer");

  if(QFile f(filename); f.open(QIODevice::WriteOnly))
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
    const QMimeData& mime, int row, int column, const QDir& parent)
{
  if(mime.hasFormat(score::mime::scenariodata()))
  {
    auto obj = readJson(mime.data(score::mime::scenariodata()));
    const auto& states = obj["States"].GetArray();
    const auto& intervals = obj["Intervals"].GetArray();
    if(states.Size() == 1 && obj["Intervals"].GetArray().Size() == 0)
    {
      const auto& state = states[0];

      // Go from a tree to a list
      const State::MessageList& flattened
          = flatten(JsonValue{state["Messages"]}.to<Process::MessageNode>());

      auto basename = JsonValue{state["Metadata"]["ScriptingName"]}.toString();
      QString filename
          = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".cues");
      if(QFile f(filename); f.open(QIODevice::WriteOnly))
      {
        f.write(score::marshall<JSONObject>(flattened).toByteArray());
      }
    }
    else
    {
      const auto& interval = intervals[0];
      auto basename = JsonValue{interval["Metadata"]["ScriptingName"]}.toString();

      QString filename
          = score::addUniqueSuffix(parent.absolutePath() + "/" + basename + ".scenario");

      if(QFile f(filename); f.open(QIODevice::WriteOnly))
      {
        f.write(mime.data(score::mime::scenariodata()));
      }
    }

    return true;
  }
  else if(mime.hasFormat(score::mime::messagelist()))
  {
    QString filename = score::addUniqueSuffix(parent.absolutePath() + "/Messages.cues");
    if(QFile f(filename); f.open(QIODevice::WriteOnly))
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

QStringList snippetCategories(std::string_view absoluteDir)
{
  const QStringList parts
      = QString::fromUtf8(absoluteDir.data(), absoluteDir.size())
            .split('/', Qt::SkipEmptyParts);
  const auto it = std::find(parts.rbegin(), parts.rend(), Library::snippetsFolder());
  if(it != parts.rend())
    return parts.mid(parts.size() - std::distance(parts.rbegin(), it));
  if(parts.isEmpty())
    return {};
  return {parts.back()};
}

void ScenarioLibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  model.addContainer(
      Library::snippetKey(), QObject::tr("Snippets"),
      Process::getCategoryIcon(QStringLiteral("Snippets")));
}

std::optional<Library::ProcessEntry>
ScenarioLibraryHandler::scanPath(std::string_view path)
{
  return snippetEntry(path);
}

std::optional<Library::ProcessEntry> snippetEntry(std::string_view path)
{
  score::PathInfo info{path};
  if(!info.fileName.ends_with(std::string_view{".scenario"}))
    return std::nullopt;

  Library::ProcessData data;
  data.key = Library::snippetKey();
  data.prettyName
      = QString::fromUtf8(info.completeBaseName.data(), info.completeBaseName.size());
  data.customData
      = QString::fromUtf8(info.absoluteFilePath.data(), info.absoluteFilePath.size());
  return Library::ProcessEntry{
      data.key, snippetCategories(info.absolutePath), {std::move(data), {}}};
}
}
