#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <algorithm>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iterator>

#include "SlotModel.hpp"
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/tools/Todo.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
namespace Scenario
{
SlotModel::SlotModel(
    const Id<SlotModel>& id, const qreal slotHeight, RackModel* parent)
    : Entity{id, Metadata<ObjectKey_k, SlotModel>::get(), parent}
{
  m_height = slotHeight;
  metadata().setInstanceName(*this);
}

SlotModel::SlotModel(
    const SlotModel& source,
    const Id<SlotModel>& id,
    RackModel* parent)
    : Entity{source, id, Metadata<ObjectKey_k, SlotModel>::get(), parent}
    , m_frontLayerModelId{source.m_frontLayerModelId}
    , m_processes{source.m_processes}
    , m_height{source.getHeight()}
{
  metadata().setInstanceName(*this);
}

RackModel& SlotModel::rack() const
{
  return *static_cast<RackModel*>(parent());
}

void SlotModel::addLayer(Id<Process::ProcessModel> p)
{
  auto it = ossia::find(m_processes, p);
  if(it == m_processes.end())
    m_processes.push_back(p);
  putToFront(p);
  layerAdded(parentConstraint().processes.at(p));
}

void SlotModel::removeLayer(Id<Process::ProcessModel> p)
{
  boost::range::remove_erase(m_processes, p);
  if (!m_processes.empty())
  {
    putToFront(*layers().begin());
  }
  else
  {
    m_frontLayerModelId = iscore::none;
  }
  layerRemoved(parentConstraint().processes.at(p));
  // TODO send a signal?
}
void SlotModel::putToFront(const OptionalId<Process::ProcessModel>& id)
{
  if (!id)
    return;

  if (id != m_frontLayerModelId)
  {
    auto lay = ossia::find(m_processes, *id);
    if (lay != m_processes.end())
    {
      m_frontLayerModelId = id;
      emit layerModelPutToFront(*frontLayerModel());
    }
  }
}

const Process::ProcessModel* SlotModel::frontLayerModel() const
{
  if (!m_frontLayerModelId)
    return nullptr;

  return & parentConstraint().processes.at(*m_frontLayerModelId);
}

void SlotModel::on_deleteSharedProcessModel(const Process::ProcessModel& proc)
{
  removeLayer(proc.id());
}

void SlotModel::setHeight(qreal arg)
{
  if (m_height != arg)
  {
    m_height = arg;
    emit HeightChanged(arg);
  }
}

void SlotModel::setFocus(bool arg)
{
  if (m_focus == arg)
    return;

  m_focus = arg;
  emit focusChanged(arg);
}

ConstraintModel& SlotModel::parentConstraint() const
{
  return static_cast<ConstraintModel&>(*parent()->parent());
}

qreal SlotModel::getHeight() const
{
  return m_height;
}

bool SlotModel::focus() const
{
  return m_focus;
}
}
