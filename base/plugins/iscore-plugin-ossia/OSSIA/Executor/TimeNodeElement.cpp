#include <Editor/TimeNode.h>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <QDebug>
#include <exception>

#include "Editor/Expression.h"
#include "TimeNodeElement.hpp"

namespace RecreateOnPlay
{
TimeNodeElement::TimeNodeElement(
        std::shared_ptr<OSSIA::TimeNode> ossia_tn,
        const Scenario::TimeNodeModel& element,
        const Device::DeviceList& devlist,
        QObject* parent):
    QObject{parent},
    m_ossia_node{ossia_tn},
    m_iscore_node{element},
    m_deviceList{devlist}
{
    try
    {
        auto expr = iscore::convert::expression(element.trigger()->expression(), m_deviceList);

        m_ossia_node->setExpression(expr);
    }
    catch(std::exception& e)
    {
        qDebug() << e.what();
        m_ossia_node->setExpression(OSSIA::Expression::create(true));
    }

    connect(m_iscore_node.trigger(), &Scenario::TriggerModel::triggered,
            this, [&] () {
        try {
        m_ossia_node->trigger();
        }
        catch(...)
        {

        }
    });
}

std::shared_ptr<OSSIA::TimeNode> TimeNodeElement::OSSIATimeNode() const
{
    return m_ossia_node;
}

const Scenario::TimeNodeModel&TimeNodeElement::iscoreTimeNode() const
{
    return m_iscore_node;
}

}
