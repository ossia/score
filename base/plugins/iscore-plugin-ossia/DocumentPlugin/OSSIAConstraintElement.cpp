#include "OSSIAConstraintElement.hpp"
#include <API/Headers/Editor/TimeConstraint.h>
#include "../iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
OSSIAConstraintElement::OSSIAConstraintElement(
        const ConstraintModel* element, QObject* parent):
    iscore::ElementPluginModel{parent}
{
    //m_constraint = OSSIA::TimeConstraint::create( );
}

std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraintElement::constraint() const
{
    return m_constraint;
}

iscore::ElementPluginModel* OSSIAConstraintElement::clone(
        const QObject* element,
        QObject* parent) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return nullptr;
}


iscore::ElementPluginModelType OSSIAConstraintElement::elementPluginId() const
{
    return staticPluginId();
}

void OSSIAConstraintElement::serialize(const VisitorVariant&) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
}
