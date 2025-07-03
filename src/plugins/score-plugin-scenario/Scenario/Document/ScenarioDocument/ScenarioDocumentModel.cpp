// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ScenarioDocumentModel.hpp"

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/IdentifierDebug.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>

#include <QDebug>
#include <QFileInfo>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::ScenarioDocumentModel)

namespace Scenario
{
ScenarioDocumentModel::ScenarioDocumentModel(
    const score::DocumentContext& ctx,
    QObject* parent)
    : score::DocumentDelegateModel{Id<score::DocumentDelegateModel>(
                                       score::id_generator::getFirstId()),
                                   "Scenario::ScenarioDocumentModel",
                                   parent}
    , m_context{ctx}
    , m_baseScenario{new BaseScenario{Id<BaseScenario>{0}, ctx, this}}
{
  auto& tn = m_baseScenario->startTimeSync();
  tn.setStartPoint(true);
  auto& itv = m_baseScenario->interval();
  // Set default durations
  auto dur = ctx.app.settings<Scenario::Settings::Model>().getDefaultDuration();

  itv.duration.setRigid(false);

  IntervalDurations::Algorithms::changeAllDurations(itv, dur);
  itv.duration.setMaxInfinite(true);
  m_baseScenario->endEvent().setDate(itv.duration.defaultDuration());
  m_baseScenario->endTimeSync().setDate(itv.duration.defaultDuration());

  auto& doc_metadata = ctx.document.metadata();
  itv.metadata().setName(doc_metadata.fileName());

  itv.addSignature(TimeVal::zero(), {4, 4});
  itv.setHasTimeSignature(true);

  using namespace Scenario::Command;

  // Create the root scenario
  AddOnlyProcessToInterval cmd1{
      itv, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), QString{}, QPointF{}};
  cmd1.redo(ctx);
  itv.processes.begin()->setSlotHeight(1500);

  // Select the first state
  score::SelectionDispatcher d{ctx.selectionStack};
  auto scenar = qobject_cast<Scenario::ProcessModel*>(&*itv.processes.begin());
  if(scenar)
    d.select(scenar->startEvent());

  init();
}

void ScenarioDocumentModel::init()
{
  auto& itv = m_baseScenario->interval();
  auto& doc_metadata = m_context.document.metadata();

  connect(
      &doc_metadata, &score::DocumentMetadata::fileNameChanged, this,
      [&](const QString& newName) {
    QFileInfo info(newName);
    itv.metadata().setName(info.completeBaseName());
      });
}

void ScenarioDocumentModel::finishLoading()
{
  // FIXME this is called on on_documentChanged.
  // Should be called only once after load and restore instead

  // Load cables
  for(const auto& bytearray : std::as_const(m_savedCables))
  {
    auto cbl = new Process::Cable{DataStream::Deserializer{bytearray}, this};
    auto src = cbl->source().try_find(m_context);
    auto snk = cbl->sink().try_find(m_context);
    if(src && snk )
    {
      if(auto it = cables.find(cbl->id()); it != cables.end())
      {
        // Process::Cable& ccbl = *it;
        // qDebug() << "warning : cable already exists";
        // qDebug() << "Existing: " << ccbl.source().unsafePath().toString()
        //          << ccbl.sink().unsafePath().toString();
        // qDebug() << "New: " << cbl->source().unsafePath().toString()
        //          << cbl->sink().unsafePath().toString();
        // qDebug() << "?? " << ossia::contains(src->cables(), make_path(ccbl))
        //          << ossia::contains(snk->cables(), make_path(ccbl));
        delete cbl;
      }
      else
      {
        src->addCable(*cbl);
        snk->addCable(*cbl);

        cables.add(cbl);
      }
    }
    else
    {
      qWarning() << "Could not find either source or sink for cable " << cbl->id() << src
                 << snk;
      delete cbl;
    }
  }
  m_savedCables.clear();

  if(m_savedCablesJson.IsArray())
  {
    for(const auto& json : m_savedCablesJson.GetArray())
    {
      auto cbl = new Process::Cable{JSONObject::Deserializer{json}, this};
      auto src = cbl->source().try_find(m_context);
      auto snk = cbl->sink().try_find(m_context);
      if(src && snk && (cables.find(cbl->id()) == cables.end()))
      {
        src->addCable(*cbl);
        snk->addCable(*cbl);

        cables.add(cbl);
      }
      else
      {
        qWarning() << "Could not find either source or sink for cable " << cbl->id()
                   << src << snk;
        delete cbl;
      }
    }
    m_savedCablesJson.Clear();
  }

  // Load buses
  for(auto itv : this->busIntervals)
  {
    const_cast<IntervalModel*>(itv)->busChanged(true);
    connect(
        itv, &Scenario::IntervalModel::identified_object_destroying, this,
        &ScenarioDocumentModel::busDeleted, Qt::UniqueConnection);
  }
}

ScenarioDocumentModel::~ScenarioDocumentModel() { }

IntervalModel& ScenarioDocumentModel::baseInterval() const
{
  return m_baseScenario->interval();
}

void ScenarioDocumentModel::addBus(const Scenario::IntervalModel* itv)
{
  if(!ossia::contains(busIntervals, itv))
  {
    busIntervals.push_back(itv);
    const_cast<IntervalModel*>(itv)->busChanged(true);
    connect(
        itv, &Scenario::IntervalModel::identified_object_destroying, this,
        [this, itv] { removeBus(itv); });
    busesChanged();
  }
}

void ScenarioDocumentModel::removeBus(const Scenario::IntervalModel* itv)
{
  if(ossia::contains(busIntervals, itv))
  {
    ossia::remove_erase(busIntervals, itv);
    const_cast<IntervalModel*>(itv)->busChanged(false);
    busesChanged();
  }
}

void ScenarioDocumentModel::busDeleted(const IdentifiedObjectAbstract* itv)
{
  if(ossia::contains(busIntervals, itv))
  {
    ossia::remove_erase(busIntervals, itv);
    busesChanged();
  }
}

void ScenarioDocumentModel::close()
{
  cables.clear();
  delete m_baseScenario;
  m_baseScenario = nullptr;
}

}
