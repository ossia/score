#include "ProcessSharedModelInterface.hpp"
#include "ProcessViewModelInterface.hpp"


ProcessSharedModelInterface::ProcessSharedModelInterface(const TimeValue& duration, const id_type<ProcessSharedModelInterface>& id, const QString& name, QObject* parent):
    IdentifiedObject<ProcessSharedModelInterface>{id, name, parent},
    m_duration{duration}
{

}


QByteArray ProcessSharedModelInterface::makeViewModelConstructionData() const { return {}; }


ProcessViewModelInterface*ProcessSharedModelInterface::makeViewModel(
        const id_type<ProcessViewModelInterface>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto pvm = makeViewModel_impl(viewModelId, constructionData, parent);
    addViewModel(pvm);

    return pvm;
}


ProcessViewModelInterface*ProcessSharedModelInterface::loadViewModel(
        const VisitorVariant& v,
        QObject* parent)
{
    auto pvm = loadViewModel_impl(v, parent);
    addViewModel(pvm);

    return pvm;
}


ProcessViewModelInterface*ProcessSharedModelInterface::cloneViewModel(
        const id_type<ProcessViewModelInterface>& newId,
        const ProcessViewModelInterface& source,
        QObject* parent)
{
    auto pvm = cloneViewModel_impl(newId, source, parent);
    addViewModel(pvm);

    return pvm;
}


QVector<ProcessViewModelInterface*> ProcessSharedModelInterface::viewModels() const
{
    return m_viewModels;
}


void ProcessSharedModelInterface::expandProcess(ExpandMode mode, const TimeValue& t)
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


void ProcessSharedModelInterface::setDuration(const TimeValue& other)
{
    m_duration = other;
}


const TimeValue&ProcessSharedModelInterface::duration() const
{
    return m_duration;
}


void ProcessSharedModelInterface::addViewModel(ProcessViewModelInterface* m)
{
    connect(m, &ProcessViewModelInterface::destroyed,
            this, [=] () { removeViewModel(m); });
    m_viewModels.push_back(m);
}


void ProcessSharedModelInterface::removeViewModel(ProcessViewModelInterface* m)
{
    int index = m_viewModels.indexOf(m);

    if(index != -1)
    {
        m_viewModels.remove(index);
    }
}


ProcessSharedModelInterface* parentProcess(QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !obj->inherits("ProcessSharedModelInterface"))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<ProcessSharedModelInterface*>(obj);
}


const ProcessSharedModelInterface* parentProcess(const QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !obj->inherits("ProcessSharedModelInterface"))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<const ProcessSharedModelInterface*>(obj);
}
