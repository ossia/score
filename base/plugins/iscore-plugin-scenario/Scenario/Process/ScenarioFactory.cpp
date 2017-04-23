#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>

#include <QDataStream>
#include <QIODevice>
#include <QMap>
#include <iscore/tools/std/Optional.hpp>

#include "ScenarioFactory.hpp"
#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/model/Identifier.hpp>

namespace Process
{
class LayerPresenter;
}
class LayerView;
class QGraphicsItem;
class QObject;

namespace Scenario
{
class ConstraintModel;
class ConstraintViewModel;

UuidKey<Process::ProcessModel>
ScenarioFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, Scenario::ProcessModel>::get();
}

QString ScenarioFactory::prettyName() const
{
  return Metadata<PrettyName_k, Scenario::ProcessModel>::get();
}

Process::ProcessModel* ScenarioFactory::make(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
{
  return new Scenario::ProcessModel{duration, id, parent};
}

//////

ScenarioTemporalLayerFactory::ScenarioTemporalLayerFactory(
    Scenario::EditionSettings& e)
    : m_editionSettings{e}
{
}

Process::LayerView* ScenarioTemporalLayerFactory::makeLayerView(
    const Process::ProcessModel& viewmodel, QGraphicsItem* parent)
{
  if (dynamic_cast<const TemporalScenarioLayer*>(&viewmodel))
    return new TemporalScenarioView{parent};

  return nullptr;
}

bool ScenarioTemporalLayerFactory::matches(
    const UuidKey<Process::ProcessModel>& p) const
{
  return p == Metadata<ConcreteKey_k, Scenario::ProcessModel>::get();
}

UuidKey<Process::ProcessModel>
ScenarioTemporalLayerFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, Scenario::TemporalScenarioLayer>::
      get();
}

Process::LayerPresenter* ScenarioTemporalLayerFactory::makeLayerPresenter(
    const Process::ProcessModel& lm,
    Process::LayerView* view,
    const Process::ProcessPresenterContext& context,
    QObject* parent)
{
  if (auto vm = dynamic_cast<const Scenario::ProcessModel*>(&lm))
  {
    auto pres = new TemporalScenarioPresenter{m_editionSettings, *vm, view,
                                              context, parent};
    static_cast<TemporalScenarioView*>(view)->setPresenter(pres);
    return pres;
  }
  return nullptr;
}
}
