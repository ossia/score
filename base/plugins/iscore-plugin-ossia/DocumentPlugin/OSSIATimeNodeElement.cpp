#include "OSSIATimeNodeElement.hpp"

#include <API/Headers/Editor/TimeNode.h>

OSSIATimeNodeElement::OSSIATimeNodeElement(
        const TimeNodeModel* element,
        QObject* parent):
    iscore::ElementPluginModel{parent}
{
    using namespace OSSIA;
    m_node = TimeNode::create();
}

std::shared_ptr<OSSIA::TimeNode> OSSIATimeNodeElement::timeNode() const
{
    return m_node;
}

iscore::ElementPluginModel* OSSIATimeNodeElement::clone(
        const QObject* element,
        QObject* parent) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return nullptr;
}

iscore::ElementPluginModelType OSSIATimeNodeElement::elementPluginId() const
{
    return staticPluginId();
}

void OSSIATimeNodeElement::serialize(const VisitorVariant&) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
}
