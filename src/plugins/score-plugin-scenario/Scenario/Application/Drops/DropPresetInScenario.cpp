#include <Scenario/Application/Drops/DropPresetInScenario.hpp>
#include <Scenario/Application/Drops/DropPresetInInterval.hpp>

#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <QFile>
#include <QUrl>
#include <QFileInfo>

namespace Scenario
{

DropPresetInScenario::DropPresetInScenario()
{
  m_acceptableSuffixes.push_back("scorepreset");
}

bool DropPresetInScenario::drop(
    const ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  QByteArray presetData;
  QString filename;

  if (mime.hasUrls())
  {
    if (QFile f{mime.urls()[0].toLocalFile()};
        QFileInfo{f}.suffix() == "scorepreset" && f.open(QIODevice::ReadOnly))
    {
      filename = QFileInfo{f}.fileName();
      presetData = f.readAll();
    }
  }
  else if(mime.hasFormat(score::mime::processpreset()))
  {
    presetData = mime.data(score::mime::processpreset());
  }
  else
  {
    return false;
  }

  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  const TimeVal t = std::chrono::seconds{5};

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  auto& procs = pres.context().context.app.interfaces<Process::ProcessFactoryList>();

  if(auto preset = Process::Preset::fromJson(procs, presetData))
  {
    m.loadProcessFromPreset(interval, *preset);
    m.showRack(interval);
    m.submit(new Scenario::Command::ChangeElementName{interval, preset->name});
    m.commit();
  }

  return true;
}

}
