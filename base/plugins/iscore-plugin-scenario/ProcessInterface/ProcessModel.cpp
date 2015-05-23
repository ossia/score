#include "ProcessModel.hpp"
#include "ProcessViewModel.hpp"


ProcessModel::ProcessModel(const TimeValue& duration, const id_type<ProcessModel>& id, const QString& name, QObject* parent):
    IdentifiedObject<ProcessModel>{id, name, parent},
    m_duration{duration}
{

}


QByteArray ProcessModel::makeViewModelConstructionData() const { return {}; }


ProcessViewModel*ProcessModel::makeViewModel(
        const id_type<ProcessViewModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto pvm = makeViewModel_impl(viewModelId, constructionData, parent);
    addViewModel(pvm);

    return pvm;
}


ProcessViewModel*ProcessModel::loadViewModel(
        const VisitorVariant& v,
        QObject* parent)
{
    auto pvm = loadViewModel_impl(v, parent);
    addViewModel(pvm);

    return pvm;
}

ProcessViewModel*ProcessModel::cloneViewModel(
        const id_type<ProcessViewModel>& newId,
        const ProcessViewModel& source,
        QObject* parent)
{
    auto pvm = cloneViewModel_impl(newId, source, parent);
    addViewModel(pvm);

    return pvm;
}


ProcessViewModel*ProcessModel::makeTemporaryViewModel(
        const id_type<ProcessViewModel>& newId,
        const ProcessViewModel& source,
        QObject* parent)
{
    return cloneViewModel_impl(newId, source, parent);
}



QVector<ProcessViewModel*> ProcessModel::viewModels() const
{
    return m_viewModels;
}


void ProcessModel::expandProcess(ExpandMode mode, const TimeValue& t)
{
    if(mode == ExpandMode::Scale)
    {
        setDurationAndScale(t);
    }
    else
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


const TimeValue&ProcessModel::duration() const
{
    return m_duration;
}


void ProcessModel::addViewModel(ProcessViewModel* m)
{
    connect(m, &ProcessViewModel::destroyed,
            this, [=] () { removeViewModel(m); });
    m_viewModels.push_back(m);
}


void ProcessModel::removeViewModel(ProcessViewModel* m)
{
    int index = m_viewModels.indexOf(m);

    if(index != -1)
    {
        m_viewModels.remove(index);
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
