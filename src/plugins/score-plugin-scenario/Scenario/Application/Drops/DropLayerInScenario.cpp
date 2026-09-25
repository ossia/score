#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Application/Drops/DropLayerInScenario.hpp>
#include <Scenario/Application/Drops/MessageDropHandler.hpp>
#include <Scenario/Application/Drops/PresetDrop.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace Scenario
{

DropLayerInScenario::DropLayerInScenario()
{
  m_acceptableMimeTypes.push_back(score::mime::layerdata());
  m_acceptableSuffixes.push_back("layer");
}

bool DropLayerInScenario::drop(
    const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  rapidjson::Document json;
  QString filename;
  if(mime.hasFormat(score::mime::layerdata()))
  {
    json = readJson(mime.data(score::mime::layerdata()));
  }
  else if(mime.hasUrls())
  {
    if(QFile f{mime.urls()[0].toLocalFile()};
       QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
    {
      filename = QFileInfo{f}.fileName();
      json = readJson(f.readAll());
    }
  }
  else
  {
    return false;
  }

  if(!json.IsObject() || json.MemberCount() == 0)
    return false;

  // Drop from the preset button: a cue of the processes' current values, or
  // a copy of them; always a copy when they come from another document
  if(auto copy = draggedCopy(json))
  {
    const auto& ctx = pres.context().context;
    const auto processes = draggedProcesses(json, ctx);
    const auto choice = processes.empty()
                            ? std::optional{PresetDrop::Copy}
                            : choosePresetDrop(processes, true);
    if(!choice)
      return true;
    if(*choice != PresetDrop::Copy)
    {
      Scenario::Command::Macro m{new Scenario::Command::CreateStateMacro, ctx};
      auto messages = Command::snapshotMessages(
          processes, m, *choice == PresetDrop::ControlsAndState);
      if(!messages.empty())
      {
        auto& state = createCueState(m, pres, pos, m_magnetic);
        m.addMessages(state, std::move(messages));
        m.commit();
        return true;
      }
    }
    return pasteProcessesInNewBox(pres.model(), pres.toScenarioPoint(pos), *copy, ctx);
  }
  if(!json.HasMember("Path") || !json.HasMember("Duration"))
    return false;

  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  // FIXME this does not look like it should be in msecs
  const TimeVal t = TimeVal::fromMsecs(json["Duration"].GetDouble());

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  if(dropStartsOnPlay())
    addStartOnPlayTrigger(m, Scenario::startTimeSync(interval, scenar));

  DropLayerInInterval::perform(interval, pres.context().context, m, json);

  m.submit(new Scenario::Command::ChangeElementName{interval, filename});
  m.commit();
  return true;
}

}
