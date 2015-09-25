#include "OSSIATimeNodeElement.hpp"

#include <API/Headers/Editor/TimeNode.h>
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"
#include "iscore2OSSIA.hpp"

OSSIATimeNodeElement::OSSIATimeNodeElement(
        std::shared_ptr<OSSIA::TimeNode> ossia_tn,
        const TimeNodeModel& element,
        const DeviceList& devlist,
        QObject* parent):
    QObject{parent},
    m_ossia_node{ossia_tn},
    m_iscore_node{element},
    m_deviceList{devlist}
{
    connect(m_iscore_node.trigger(), &TriggerModel::triggerChanged,
        this, &OSSIATimeNodeElement::on_triggerChanged);

    on_triggerChanged(m_iscore_node.trigger()->expression());
}

std::shared_ptr<OSSIA::TimeNode> OSSIATimeNodeElement::timeNode() const
{
    return m_ossia_node;
}

void OSSIATimeNodeElement::on_triggerChanged(const iscore::Trigger& c)
try
{
    auto expr = iscore::convert::expression(c, m_deviceList);

    m_ossia_node->setExpression(expr);
}
catch(std::exception& e)
{
    qDebug() << e.what();
}
