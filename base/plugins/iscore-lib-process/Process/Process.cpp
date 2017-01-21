#include <QObject>
#include <algorithm>
#include <stdexcept>

#include "LayerModel.hpp"
#include "Process.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Identifier.hpp>

template class IdentifiedObject<Process::ProcessModel>;
template class iscore::SerializableInterface<Process::ProcessModelFactory>;
namespace Process
{
ProcessModel::ProcessModel(
    TimeVal duration,
    const Id<ProcessModel>& id,
    const QString& name,
    QObject* parent)
    : Entity{id, name, parent}, m_duration{std::move(duration)}
{
}

ProcessModel::~ProcessModel() = default;

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<ProcessModel>& id,
    const QString& name,
    QObject* parent)
    : Entity{source, id, name, parent}, m_duration{source.duration()}
{
}

ProcessModel::ProcessModel(DataStream::Deserializer& vis, QObject* parent)
    : Entity(vis, parent)
{
  vis.writeTo(*this);
}

ProcessModel::ProcessModel(JSONObject::Deserializer& vis, QObject* parent)
    : Entity(vis, parent)
{
  vis.writeTo(*this);
}

std::vector<LayerModel*> ProcessModel::layers() const
{
  return m_layers;
}

void ProcessModel::setParentDuration(ExpandMode mode, const TimeVal& t)
{
  switch (mode)
  {
    case ExpandMode::Scale:
      setDurationAndScale(t);
      break;
    case ExpandMode::GrowShrink:
    {
      if (duration() < t)
        setDurationAndGrow(t);
      else
        setDurationAndShrink(t);
      break;
    }
    case ExpandMode::ForceGrow:
    {
      if (duration() < t)
        setDurationAndGrow(t);
      break;
    }
    case ExpandMode::CannotExpand:
    default:
      break;
  }
}

void ProcessModel::setDuration(const TimeVal& other)
{
  m_duration = other;
  emit durationChanged(m_duration);
}

const TimeVal& ProcessModel::duration() const
{
  return m_duration;
}

int32_t ProcessModel::priority() const
{
  return m_priority;
}

void ProcessModel::setPriority(int32_t i)
{
  if (m_priority != i)
  {
    m_priority = i;
    emit priorityChanged(i);
  }
}

bool ProcessModel::priorityOverride() const
{
  return m_priorityOverride;
}

void ProcessModel::setPriorityOverride(bool o)
{
  if (m_priorityOverride != o)
  {
    m_priorityOverride = o;
    emit priorityOverrideChanged(o);
  }
}

void ProcessModel::addLayer(LayerModel* m)
{
  connect(m, &LayerModel::destroyed, this, [=]() { removeLayer(m); });
  m_layers.push_back(m);
}

void ProcessModel::removeLayer(LayerModel* m)
{
  auto it = ossia::find(m_layers, m);
  if (it != m_layers.end())
  {
    m_layers.erase(it);
  }
}

ProcessModel* parentProcess(QObject* obj)
{
  QString objName(obj ? obj->objectName() : "INVALID");
  while (obj && !dynamic_cast<ProcessModel*>(obj))
  {
    obj = obj->parent();
  }

  if (!obj)
    throw std::runtime_error(
        QString("Object (name: %1) is not child of a Process!")
            .arg(objName)
            .toStdString());

  return static_cast<ProcessModel*>(obj);
}

const ProcessModel* parentProcess(const QObject* obj)
{
  QString objName(obj ? obj->objectName() : "INVALID");
  while (obj && !dynamic_cast<const ProcessModel*>(obj))
  {
    obj = obj->parent();
  }

  if (!obj)
    throw std::runtime_error(
        QString("Object (name: %1) is not child of a Process!")
            .arg(objName)
            .toStdString());

  return static_cast<const ProcessModel*>(obj);
}
}
