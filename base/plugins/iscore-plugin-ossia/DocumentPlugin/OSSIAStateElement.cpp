#include "OSSIAStateElement.hpp"
#include <iscore/tools/Todo.hpp>
OSSIAStateElement::OSSIAStateElement(
        const StateModel *element,
        QObject *parent):
    QObject{parent}
{
}

QList<std::shared_ptr<OSSIA::State> > OSSIAStateElement::states() const
{
    return m_states;
}


void OSSIAStateElement::addState(std::shared_ptr<OSSIA::State> s)
{
    m_states.append(s);
}

void OSSIAStateElement::removeState(std::shared_ptr<OSSIA::State> s)
{
    m_states.removeOne(s);
}

void OSSIAStateElement::handleEventTriggering(OSSIA::TimeEvent::Status status)
{
}
