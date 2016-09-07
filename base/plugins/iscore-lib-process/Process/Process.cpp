#include <QObject>
#include <algorithm>
#include <stdexcept>

#include "LayerModel.hpp"
#include "Process.hpp"
#include <Process/ExpandMode.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/Algorithms.hpp>

template class IdentifiedObject<Process::ProcessModel>;
template class iscore::SerializableInterface<Process::ProcessFactory>;
namespace Process
{
ProcessModel::ProcessModel(
        TimeValue duration,
        const Id<ProcessModel>& id,
        const QString& name,
        QObject* parent):
    Entity{id, name, parent},
    m_duration{std::move(duration)}
{
    metadata().setName(QString("Process.%1").arg(*this->id().val()));
}

ProcessModel::~ProcessModel() = default;


ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<ProcessModel>& id,
        const QString& name,
        QObject* parent):
    Entity{source, id, name, parent},
    m_duration{source.duration()}
{
    metadata().setName(QString("Process.%1").arg(*this->id().val()));
}

ProcessModel::ProcessModel(Deserializer<DataStream>& vis, QObject* parent) :
    Entity(vis, parent)
{
    vis.writeTo(*this);
}

ProcessModel::ProcessModel(Deserializer<JSONObject>& vis, QObject* parent) :
    Entity(vis, parent)
{
    vis.writeTo(*this);
}

std::vector<LayerModel*> ProcessModel::layers() const
{
    return m_layers;
}


void ProcessModel::setParentDuration(ExpandMode mode, const TimeValue& t)
{
    switch(mode)
    {
        case ExpandMode::Scale:
            setDurationAndScale(t);
            break;
        case ExpandMode::GrowShrink:
        {
            if(duration() < t)
                setDurationAndGrow(t);
            else
                setDurationAndShrink(t);
            break;
        }
        case ExpandMode::ForceGrow:
        {
            if(duration() < t)
                setDurationAndGrow(t);
            break;
        }
        case ExpandMode::Fixed:
        default:
            break;
    }
}


void ProcessModel::setDuration(const TimeValue& other)
{
    m_duration = other;
    emit durationChanged(m_duration);
}


const TimeValue& ProcessModel::duration() const
{
    return m_duration;
}


void ProcessModel::addLayer(LayerModel* m)
{
    connect(m, &LayerModel::destroyed,
            this, [=] () { removeLayer(m); });
    m_layers.push_back(m);
}


void ProcessModel::removeLayer(LayerModel* m)
{
    auto it = find(m_layers, m);
    if(it != m_layers.end())
    {
        m_layers.erase(it);
    }
}


ProcessModel* parentProcess(QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !dynamic_cast<ProcessModel*>(obj))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<ProcessModel*>(obj);
}


const ProcessModel* parentProcess(const QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !dynamic_cast<const ProcessModel*>(obj))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<const ProcessModel*>(obj);
}
}
