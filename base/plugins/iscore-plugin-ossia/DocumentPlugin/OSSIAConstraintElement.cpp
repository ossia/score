#include "OSSIAConstraintElement.hpp"
#include <API/Headers/Editor/TimeConstraint.h>
#include "../iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
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
}

void OSSIAConstraintElement::on_processRemoved(const id_type<ProcessModel>& process)
{

}
