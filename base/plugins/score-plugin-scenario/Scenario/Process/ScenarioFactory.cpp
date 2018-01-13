// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/MiniScenarioView.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>

#include <QDataStream>
#include <QIODevice>
#include <QMap>
#include <score/tools/std/Optional.hpp>

#include "ScenarioFactory.hpp"
#include <Process/Process.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/Identifier.hpp>

namespace Process
{
class LayerPresenter;
}
class LayerView;
class QGraphicsItem;
class QObject;

namespace Scenario
{
class IntervalModel;

//////

ScenarioTemporalLayerFactory::ScenarioTemporalLayerFactory(
    Scenario::EditionSettings& e)
    : m_editionSettings{e}
{
}

Process::LayerView* ScenarioTemporalLayerFactory::makeLayerView(
    const Process::ProcessModel& p, QGraphicsItem* parent)
{
  if (dynamic_cast<const Scenario::ProcessModel*>(&p))
    return new TemporalScenarioView{parent};

  return nullptr;
}

Process::MiniLayer* ScenarioTemporalLayerFactory::makeMiniLayer(
    const Process::ProcessModel& p,
    QGraphicsItem* parent)
{
  if (auto s = dynamic_cast<const Scenario::ProcessModel*>(&p))
    return new MiniScenarioView{*s, parent};

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
  return Metadata<ConcreteKey_k, Scenario::ProcessModel>::get();
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
    return pres;
  }
  return nullptr;
}
}
