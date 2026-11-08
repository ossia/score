#include <Scenario/Application/Drops/DropPresetInInterval.hpp>
#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Application/Drops/DropPresetInScenario.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace Scenario
{

DropPresetInScenario::DropPresetInScenario()
{
  m_acceptableSuffixes.push_back("scp");
}

bool DropPresetInScenario::drop(
    const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if(isProcessesDrag(mime))
    return false;
  auto& procs = pres.context().context.app.interfaces<Process::ProcessFactoryList>();
  std::shared_ptr<Process::Preset> preset;

  if(mime.hasUrls())
  {
    const QString file = mime.urls()[0].toLocalFile();
    const QFileInfo info{file};
    // We do not want to catch every file
    if(info.suffix() != "scp" || !info.isReadable())
      return false;
    preset = Process::Preset::fromFile(procs, file);
  }
  else if(mime.hasFormat(score::mime::processpreset()))
  {
    preset = Process::Preset::fromJson(procs, mime.data(score::mime::processpreset()));
  }
  else
  {
    return false;
  }

  if(!preset)
    return true;

  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  const TimeVal t = TimeVal::fromMsecs(5000);

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  // The box is always in the void, not after anything that would start it
  addStartOnPlayTrigger(m, Scenario::startTimeSync(interval, scenar));

  m.loadProcessFromPreset(interval, *preset);
  m.showRack(interval);
  m.submit(new Scenario::Command::ChangeElementName{interval, preset->name});
  m.commit();

  return true;
}

}
