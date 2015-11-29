#include <QObject>
#include <algorithm>
#include <stdexcept>

#include "LayerModel.hpp"
#include "Process.hpp"
#include <Process/ExpandMode.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>

Process::Process(
        const TimeValue& duration,
        const Id<Process>& id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<Process>{id, name, parent},
    m_duration{duration}
{
    metadata.setName(QString("Process.%1").arg(*this->id().val()));
}

Process::~Process()
{

}

Process::Process(
        const Process& source,
        const Id<Process>& id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<Process>{id, name, parent},
    m_duration{source.duration()}
{
    metadata.setName(QString("Process.%1").arg(*this->id().val()));
}


QByteArray Process::makeLayerConstructionData() const { return {}; }


LayerModel*Process::makeLayer(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lm = makeLayer_impl(viewModelId, constructionData, parent);
    addLayer(lm);

    return lm;
}


LayerModel*Process::loadLayer(
        const VisitorVariant& v,
        QObject* parent)
{
    auto lm = loadLayer_impl(v, parent);
    addLayer(lm);

    return lm;
}

LayerModel*Process::cloneLayer(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto lm = cloneLayer_impl(newId, source, parent);
    addLayer(lm);

    return lm;
}


LayerModel*Process::makeTemporaryLayer(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return cloneLayer_impl(newId, source, parent);
}



std::vector<LayerModel*> Process::layers() const
{
    return m_layers;
}


void Process::expandProcess(ExpandMode mode, const TimeValue& t)
{
    if(mode == ExpandMode::Scale)
    {
        setDurationAndScale(t);
    }
    else if (mode == ExpandMode::Grow)
    {
        if(duration() < t)
            setDurationAndGrow(t);
        else
            setDurationAndShrink(t);
    }
}


void Process::setDuration(const TimeValue& other)
{
    m_duration = other;
    emit durationChanged(m_duration);
}


const TimeValue& Process::duration() const
{
    return m_duration;
}


void Process::addLayer(LayerModel* m)
{
    connect(m, &LayerModel::destroyed,
            this, [=] () { removeLayer(m); });
    m_layers.push_back(m);
}


void Process::removeLayer(LayerModel* m)
{
    auto it = find(m_layers, m);
    if(it != m_layers.end())
    {
        m_layers.erase(it);
    }
}


Process* parentProcess(QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !dynamic_cast<Process*>(obj))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<Process*>(obj);
}


const Process* parentProcess(const QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !dynamic_cast<const Process*>(obj))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<const Process*>(obj);
}
