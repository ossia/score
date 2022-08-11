#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Application/Drops/DropLayerInScenario.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
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
  if(mime.formats().contains(score::mime::layerdata()))
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
  if(!json.HasMember("Path") || !json.HasMember("Duration"))
    return false;

  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  const TimeVal t = TimeVal::fromMsecs(json["Duration"].GetDouble());

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  DropLayerInInterval::perform(interval, pres.context().context, m, json);

  m.submit(new Scenario::Command::ChangeElementName{interval, filename});
  m.commit();
  return true;
}

}
