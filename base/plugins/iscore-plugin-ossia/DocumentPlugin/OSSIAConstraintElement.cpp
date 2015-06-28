#include "OSSIAConstraintElement.hpp"
#include <API/Headers/Editor/TimeConstraint.h>
#include "../iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "OSSIAScenarioElement.hpp"

#include <boost/range/algorithm.hpp>
OSSIAConstraintElement::OSSIAConstraintElement(
        std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
        const ConstraintModel& iscore_cst,
        QObject* parent):
    iscore::ElementPluginModel{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst}
{
    connect(&iscore_cst, &ConstraintModel::processCreated,
            this, &OSSIAConstraintElement::on_processAdded);
    connect(&iscore_cst, &ConstraintModel::processRemoved,
            this, &OSSIAConstraintElement::on_processRemoved);
}

std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraintElement::constraint() const
{
    return m_ossia_constraint;
}

iscore::ElementPluginModel* OSSIAConstraintElement::clone(
        const QObject* element,
        QObject* parent) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}


iscore::ElementPluginModelType OSSIAConstraintElement::elementPluginId() const
{
    return staticPluginId();
}

void OSSIAConstraintElement::serialize(const VisitorVariant&) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}

void OSSIAConstraintElement::on_processAdded(
        const QString& name,
        const id_type<ProcessModel>& id)
{
    // The DocumentPlugin creates the elements in the processes.
    auto proc = m_iscore_constraint.process(id);
    for(auto&& elt : proc->pluginModelList->list())
    {
        qDebug() << (void*)elt;

        if(auto scenar_elt = dynamic_cast<OSSIAProcessElement*>(elt))
        {
            qDebug("awyisss");
            m_processes.insert({id, scenar_elt});
            break;
        }
    }

}

void OSSIAConstraintElement::on_processRemoved(const id_type<ProcessModel>& process)
{
    auto it = m_processes.find(process);
    if(it != m_processes.end())
    {
        m_processes.erase(it);
    }
}
