#include "OSSIATimeNodeElement.hpp"

#include <API/Headers/Editor/TimeNode.h>

OSSIATimeNodeElement::OSSIATimeNodeElement(
        std::shared_ptr<OSSIA::TimeNode> ossia_tn,
        const TimeNodeModel& element,
        QObject* parent):
    QObject{parent},
    m_node{ossia_tn}
{
    using namespace OSSIA;
}

std::shared_ptr<OSSIA::TimeNode> OSSIATimeNodeElement::timeNode() const
{
    return m_node;
}
