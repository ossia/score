#include "ProcessModel.hpp"
#include "LayerModel.hpp"


ProcessModel::ProcessModel(
        const TimeValue& duration,
        const id_type<ProcessModel>& id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<ProcessModel>{id, name, parent},
    m_duration{duration}
{

}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const id_type<ProcessModel>& id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<ProcessModel>{id, name, parent},
    pluginModelList{source.pluginModelList, this},
    m_duration{source.duration()}
{

}


QByteArray ProcessModel::makeViewModelConstructionData() const { return {}; }


LayerModel*ProcessModel::makeLayer(
        const id_type<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lm = makeLayer_impl(viewModelId, constructionData, parent);
    addLayer(lm);

    return lm;
}


LayerModel*ProcessModel::loadLayer(
        const VisitorVariant& v,
        QObject* parent)
{
    auto lm = loadLayer_impl(v, parent);
    addLayer(lm);

    return lm;
}

LayerModel*ProcessModel::cloneLayer(
        const id_type<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto lm = cloneLayer_impl(newId, source, parent);
    addLayer(lm);

    return lm;
}


LayerModel*ProcessModel::makeTemporaryLayer(
        const id_type<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return cloneLayer_impl(newId, source, parent);
}



QVector<LayerModel*> ProcessModel::layers() const
{
    return m_layers;
}


void ProcessModel::expandProcess(ExpandMode mode, const TimeValue& t)
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


void ProcessModel::setDuration(const TimeValue& other)
{
    m_duration = other;
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
    int index = m_layers.indexOf(m);

    if(index != -1)
    {
        m_layers.remove(index);
    }
}


ProcessModel* parentProcess(QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !obj->inherits("ProcessModel"))
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
    while(obj && !obj->inherits("ProcessModel"))
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
